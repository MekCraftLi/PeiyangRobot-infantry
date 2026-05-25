/**
 *******************************************************************************
 * @file    commander.cpp
 * @brief   输入指挥服务 — 4 层输入管线实现
 *
 * 管线流程 (每 tick):
 *   1. 读取遥控器硬件缓存 (DMA → updateRaw)
 *   2. 驱动所有 Action 更新 (控件值 → 触发器判定)
 *   3. 仲裁控制源 (SAFE_STOP / REMOTE / VISION)
 *   4. 根据控制源填充指令 (云台 / 底盘 / 射击)
 *   5. 写入黑板 (Blackboard)
 *
 * 仲裁规则:
 *   - 遥控器离线 → SAFE_STOP (急停)
 *   - DR16:  右开关上/中 → REMOTE, 下 → VISION
 *   - 图传:  开关 < 0 → SAFE_STOP, 鼠标右键 → VISION,
 *            其余 → REMOTE
 *******************************************************************************
 * @author  MekLi
 * @date    2026/2/27
 * @version 2.0
 *******************************************************************************
 */

#include "commander.h"
#include "Application/fire-ctrl-app.h"
#include "Application/movtion-ctrl-app.h"
#include "Config/config.h"
#include "System/DataHub/blackboard.h"
#include "usart.h"

#include <algorithm>

/* -------- 应用属性 -------------------------------------------------------------------------------------------------
 */

#define APPLICATION_ENABLE     true
#define APPLICATION_NAME       "Input"
#define APPLICATION_STACK_SIZE 1024
#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];

[[maybe_unused]] static auto& forceInit                        = CommanderSrvc::instance();

/* -------- 遥控器 DMA 缓存 -------------------------------------------------------------------------------------------
 */

__attribute__((section(".dma_pool"))) static uint8_t rxbuf[32] = {0};

#ifdef GIMBAL
#if REMOTE_DEVICE == REMOTE_DR16
static Dr16Data _dr16Data;
#elif REMOTE_DEVICE == REMOTE_VIDEO_LINK
static VideoLinkRawData _videoLinkRawData;
#elif REMOTE_DEVICE == REMOTE_GAMEPAD
static GamepadRawData _gamepadRawData;
#endif
#endif

/* -------- 辅助函数 -------------------------------------------------------------------------------------------------
 */

namespace {
inline float clampUnit(float v) { return std::max(-1.0f, std::min(1.0f, v)); }
inline float composeAxis(float a, float b) { return clampUnit(a + b); }
} // namespace

/* -------- 构造 -----------------------------------------------------------------------------------------------------
 */

CommanderSrvc::CommanderSrvc()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 10) {
    // 所有触发器已在头文件中通过 in-class 初始化完成, 无需此处重复.
}

/* -------- 初始化: 绑定物理控件 → Action -------------------------------------------------- */

void CommanderSrvc::init() {
    // ── 1. Action 映射绑定 (由各遥控器类实现) ──
#if REMOTE_DEVICE == REMOTE_DR16
    RemoteDR16::instance().bindActions(_actions, triggers);
#elif REMOTE_DEVICE == REMOTE_VIDEO_LINK
    VideoLinkRemote::instance().bindActions(_actions, triggers);
#elif REMOTE_DEVICE == REMOTE_GAMEPAD
    // Gamepad 保持内联绑定
    actionMoveX.bind(BluetoothGamepad::instance().getRightTrigger(), &triggers.joystickDeadzone);
    actionBreak.bind(BluetoothGamepad::instance().getLeftTrigger(), &triggers.joystickDeadzone);
    actionYaw.bind(BluetoothGamepad::instance().getAxis(AxisID::ViewYaw), &triggers.joystickDeadzone);
    actionRelax.bind(BluetoothGamepad::instance().getButtonA(), &_relax);
    actionHandbrakeDepth.bind(BluetoothGamepad::instance().getButtonB(), &_handbreak);
#endif

#ifdef GIMBAL
#if REMOTE_DEVICE != REMOTE_GAMEPAD
    // 视觉控制信号绑定
    actionVisionSingle.bind(&_visionFireControl, &triggers.visionSingleShotRise);
#endif
#endif

    // ── 2. 初始化硬件通信 (HAL 中断方式) ──
    HAL_UARTEx_ReceiveToIdle_DMA(&Config::Hardware::Comms::REMOTE_UART, rxbuf, sizeof(rxbuf));
}

/* -------- 私有辅助方法 ----------------------------------------------------------------------------------------------
 */

void CommanderSrvc::resolveChassisMode(bool spinRequested, GimbalToChassisComm& comm) {
    comm.msg.mode = spinRequested ? CHASSIS_SPIN : CHASSIS_NORMAL;
}

void CommanderSrvc::resolveMovement(GimbalCmd& gCmd, GimbalToChassisComm& comm) {
#if REMOTE_DEVICE == REMOTE_VIDEO_LINK
    // 复合轴: 摇杆 + 键盘 → 底盘速度
    const float moveXInput = composeAxis(actionMoveX.getValue(), actionMoveXKey.getValue());
    const float moveYInput = composeAxis(actionMoveY.getValue(), actionMoveYKey.getValue());
    // 复合轴: 鼠标 × 灵敏度 + 摇杆 → 云台角速度
    const float yawInput =
        composeAxis(actionMouseYaw.getValue() * Config::Algorithm::Input::Y_SENSITIVITY, actionYaw.getValue());
    const float pitchInput =
        composeAxis(actionMousePitch.getValue() * Config::Algorithm::Input::X_SENSITIVITY, actionPitch.getValue());
#else
    // DR16: 直接摇杆映射
    const float moveXInput = actionMoveX.getValue();
    const float moveYInput = actionMoveY.getValue();
    const float yawInput   = actionYaw.getValue();
    const float pitchInput = actionPitch.getValue();
#endif

    // 遥控器方向与运动坐标系部分相反, 取负补偿
    comm.msg.vx   = moveXInput * Config::Algorithm::Chassis::MAX_VX;
    comm.msg.vy   = -moveYInput * Config::Algorithm::Chassis::MAX_VY;
    gCmd.yawVel   = -yawInput * Config::Algorithm::Gimbal::MAX_YAW_SPEED;
    gCmd.pitchVel = -pitchInput * Config::Algorithm::Gimbal::MAX_PITCH_SPEED;
    #if (defined(DOG_1)||defined(DOG_2))
    comm.msg.yawVel100= gCmd.yawVel * 100;
    #endif
}

/* -------- 主循环 ----------------------------------------------------------------------------------------------------
 */

void CommanderSrvc::run() {
    // ── 0. 计算时间步长 ──
    static uint32_t lastTick = xTaskGetTickCount();
    uint32_t nowTick         = xTaskGetTickCount();
    float dt                 = (float)(nowTick - lastTick) / 1000.0f;
    if (dt <= 0.0f)
        dt = 0.001f;
    lastTick = nowTick;

    // ── 1. 更新遥控器原始数据 ──
#ifdef GIMBAL
#if REMOTE_DEVICE == REMOTE_DR16
    RemoteDR16::instance().updateRaw(_dr16Data);
#elif REMOTE_DEVICE == REMOTE_VIDEO_LINK
    VideoLinkRemote::instance().updateRaw(_videoLinkRawData);
#elif REMOTE_DEVICE == REMOTE_GAMEPAD
    BluetoothGamepad::instance().updateRaw(_gamepadRawData);
#endif
#endif

    // ── 2. 驱动所有 Action 更新 ──
#if REMOTE_DEVICE != REMOTE_GAMEPAD || defined(CHASSIS)
    for (auto& action : _actions)
        action.update(dt);

    // 视觉控件更新



#else
    for (auto& action : _gpActions)
        action.update(dt);
#endif

    // ── 2.5 更新 Debug 触发状态 ──
#if REMOTE_DEVICE != REMOTE_GAMEPAD || defined(CHASSIS)
    debug.ctrlMode      = actionCtrlMode.isTriggered();
    debug.moveX         = actionMoveX.isTriggered();
    debug.moveY         = actionMoveY.isTriggered();
    debug.moveXKey      = actionMoveXKey.isTriggered();
    debug.moveYKey      = actionMoveYKey.isTriggered();
    debug.yaw           = actionYaw.isTriggered();
    debug.pitch         = actionPitch.isTriggered();
    debug.mouseYaw      = actionMouseYaw.isTriggered();
    debug.mousePitch    = actionMousePitch.isTriggered();
    debug.mouseBurst    = actionMouseBurst.isTriggered();
    debug.mouseSingle   = actionMouseSingle.isTriggered();
    debug.mouseVision   = actionMouseVision.isTriggered();
    debug.fricToggle    = actionFricToggle.isTriggered();
    debug.shootBurst    = actionShootBurst.isTriggered();
    debug.shootSingle   = actionShootSingle.isTriggered();
    debug.spinMode      = actionSpinMode.isTriggered();
    debug.keySpin       = actionKeySpin.isTriggered();
    debug.capSwitch     = actionCapSwitch.isTriggered();
    debug.keyboardFric  = actionKeyboardFric.isTriggered();
    debug.fn1Switch     = actionFn1Switch.isTriggered();
    debug.turboMode     = actionTurboMode.isTriggered();
    debug.stepClimb     = actionStepClimb.isTriggered();
    debug.selfRescue    = actionSelfRescue.isTriggered();
    debug.manualRescue  = actionManualRescue.isTriggered();
    debug.gimbalReverse = actionGimbalReverse.isTriggered();
    debug.jump          = actionJump.isTriggered();
    debug.aimMode       = actionAimMode.isTriggered();
    debug.legLength     = actionLegLength.isTriggered();
    debug.reverseEdge   = actionReverseEdge.isTriggered();
#else
    debug.gpRelax     = actionRelax.isTriggered();
    debug.gpHandbrake = actionHandbrakeDepth.isTriggered();
    debug.gpMoveX     = actionMoveX.isTriggered();
    debug.gpYaw       = actionYaw.isTriggered();
    debug.gpBrake     = actionBreak.isTriggered();
#endif

    // ── 3~5. 仲裁 → 填充指令 → 写入黑板 ──

#ifdef GIMBAL

#if REMOTE_DEVICE != REMOTE_GAMEPAD
    // ========================================
    //  DR16 / VideoLink 共用仲裁逻辑
    // ========================================

    GimbalCmd gCmd{};
    ShootCmd sCmd{};
    GimbalToChassisComm comm{};
    VisionCommand vCmd{};
    ImuState imuState{};


    // ── 3. 仲裁控制源 (统一逻辑) ──
    // 3档开关归一化: -1.0(上) / 0.0(中) / 1.0(下)
    //   < -0.5 → SAFE_STOP    [-0.5, 0.5] → REMOTE    > 0.5 → VISION
    ControlSource currentSource = ControlSource::SAFE_STOP;
    const float swState         = actionCtrlMode.getValue();

    if (swState > TriggerCfg::MODE_SW_VISION_THRESH || actionMouseVision.isTriggered()) {
        currentSource = ControlSource::VISION;
    } else if (swState >= TriggerCfg::MODE_SW_STOP_THRESH) {
        currentSource = ControlSource::REMOTE;
    }

    // ── 4. 读取黑板历史值 (未修改字段天然保留) ──

    Blackboard::instance().gimbalCmd.read(gCmd);
    Blackboard::instance().shootCmd.read(sCmd);
    Blackboard::instance().imuState.read(imuState);
    Blackboard::instance().visionCmd.read(vCmd);

    sCmd.event               = ShootEvent::NONE;

    // 预计算运动意图 (REMOTE / VISION 共用)
    const bool spinRequested = actionSpinMode.isTriggered() || actionKeySpin.isTriggered();

    switch (currentSource) {
        case ControlSource::SAFE_STOP: {
            comm.msg.mode         = (uint8_t)CHASSIS_RELAX;
            sCmd.event            = ShootEvent::EMERGENCY_STOP;
            sCmd.state.burstShot  = 0;
            gCmd.mode             = GIMBAL_RELAX;
            gCmd.yawVel           = 0;
            gCmd.pitchVel         = 0;
            comm.msg.turboMode    = 0;  
            comm.msg.stepClimb    = 0;
            comm.msg.legLength    = 0;
            comm.msg.selfRescue   = 0;
            comm.msg.manualRescue = 0;
            comm.msg.gimbalReverse= 0;
            comm.msg.jump         = 0;
            

        } break;

        case ControlSource::REMOTE: {
            resolveChassisMode(spinRequested, comm);
            resolveMovement(gCmd, comm);

            gCmd.mode           = GIMBAL_NORMAL;
            gCmd.targetYawSpeed = 0;



            /*
             * 发射信号判断
             */

            if (actionFricToggle.isTriggered() || actionKeyboardFric.isTriggered()) {
                sCmd.event = ShootEvent::FRIC_TOGGLE;
            }

            if (actionShootBurst.isTriggered() || actionMouseBurst.isTriggered()) {
                sCmd.state.burstShot = 1;
            } 
            
            else {
                sCmd.state.burstShot = 0;
            }

            if (actionShootSingle.isTriggered() || actionMouseSingle.isTriggered()) {
                sCmd.event = ShootEvent::SINGLE_FIRE;
            }



        } break;

        case ControlSource::VISION: {
            resolveChassisMode(spinRequested, comm);
            resolveMovement(gCmd, comm);

            gCmd.mode                  = GIMBAL_AUTO;

            // 视觉指令覆盖云台目标

            gCmd.targetYaw             = vCmd.targetYaw;
            gCmd.targetPitch           = -vCmd.targetPitch;
            gCmd.targetYawSpeed        = vCmd.targetYawSpeed;
            gCmd.pitchVel              = -vCmd.targetPitchSpeed;
            gCmd.targetYawAcceleration = vCmd.targetYawAcceleration;



            _visionFireControl.updateRaw(vCmd.fireCommand ? 1 : 0);
            actionVisionSingle.update(dt);


            if (actionFricToggle.isTriggered() || actionKeyboardFric.isTriggered()) {
                sCmd.event = ShootEvent::FRIC_TOGGLE;
            }


            /*
             * 判断是否处于单发模式
             * 如果在单发模式，检测发火信号的01跳变
             * 如果不是单发模式，将发火信号作为连发状态
             */
            if (vCmd.isSingleShot) {
                if (actionVisionSingle.isTriggered()) {
                    sCmd.event = ShootEvent::SINGLE_FIRE;
                }
            } else {
                sCmd.state.burstShot = vCmd.fireCommand;
            }



        } break;

        default:
            break;
    }

    gCmd.timestamp = nowTick;

    // 底盘模式: Relax/Align 状态强制 RELAX, Manual/Auto 正常控制
    {
        auto motionState = MovtionCtrlApp::instance().getMotionState();
        if (motionState == MovtionCtrlApp::MotionState::Relax || motionState == MovtionCtrlApp::MotionState::Align) {
            comm.msg.mode = (uint8_t)CHASSIS_RELAX;
            comm.msg.vx   = 0;
            comm.msg.vy   = 0;
        }
    }
    comm.msg.fn1Switch     = 0;

    // 功能标志位
    comm.msg.capSwitch     = actionCapSwitch.isTriggered() ? 1 : 0;
    comm.msg.turboMode     = actionTurboMode.isTriggered() ? 1 : 0;
    comm.msg.stepClimb     = actionStepClimb.isTriggered() ? 1 : 0;
    comm.msg.legLength     = triggers.legLengthCycle.getIndex(); // 0/1/2
    comm.msg.selfRescue    = actionSelfRescue.isTriggered() ? 1 : 0;
    comm.msg.manualRescue  = actionManualRescue.isTriggered() ? 1 : 0;
    comm.msg.gimbalReverse = actionGimbalReverse.isTriggered() ? 1 : 0;
    comm.msg.jump          = actionJump.isTriggered() ? 1 : 0;
    comm.msg.fireState     = static_cast<uint8_t>(FireCtrlApp::instance().getFireState());
    comm.msg.aimMode       = triggers.aimModeCycle.getIndex();
    
    #ifdef STEER
    #if REMOTE_DEVICE == REMOTE_DR16
    
    if(_dr16Data.s2 ==2)
    {
        comm.msg.spining       = 1;
    }
    #endif
    #if REMOTE_DEVICE == REMOTE_VIDEO_LINK
    comm.msg.spining       = (triggers.spinKeyToggle.isToggledOn() ? 1 : 0)|| (triggers.spinToggle.isToggledOn() ? 1 : 0);
    #endif
    #endif
    //#if (defined(DOG_1)||defined(DOG_2))
    
#else
    // ========================================
    //  Gamepad 专用逻辑
    // ========================================

    GimbalCmd gCmd{};
    GimbalToChassisComm comm{};
    Blackboard::instance().gimbalCmd.read(gCmd);

    if (actionRelax.isTriggered()) {
        gCmd.mode     = GIMBAL_RELAX;
        comm.msg.mode = CHASSIS_RELAX;
        gCmd.yawVel   = 0;
        gCmd.pitchVel = 0;
    } else {
        gCmd.mode     = GIMBAL_NORMAL;   
        comm.msg.mode = CHASSIS_NORMAL;
        gCmd.yawVel   = actionYaw.getValue();

        float pureVel = actionMoveX.getValue() - actionBreak.getValue();
        if (pureVel < 0)
            pureVel = 0;
        comm.msg.vx = pureVel * Config::Algorithm::Chassis::MAX_VX * 10;
    }
#endif

    // ── 5. 写入黑板 ──
    Blackboard::instance().g2cOutput.write(comm);
    Blackboard::instance().gimbalCmd.write(gCmd);
    Blackboard::instance().shootCmd.write(sCmd);

#elif defined(CHASSIS)
    // ========================================
    //  底盘板逻辑: 转发云台指令 → 底盘指令
    // ========================================

    GimbalToChassisComm comm{};
    ChassisCmd cmd{};
    Blackboard::instance().rComm.read(comm);

    cmd.mode      = comm.msg.mode;
    cmd.capSwitch = comm.msg.capSwitch;
    cmd.vx        = (float)comm.msg.vx / 10;
    cmd.vy        = (float)comm.msg.vy / 10;

    if ((cmd.mode & 0x03) == CHASSIS_NORMAL)
        cmd.vw = 0;
    else if ((cmd.mode & 0x03) == CHASSIS_SPIN)
        cmd.vw = Config::Algorithm::Chassis::MAX_VW;

    Blackboard::instance().chassisCmd.write(cmd);

#endif
}

/* -------- UART 回调 (HAL 中断方式) -------------------------------------------------- */

#ifdef GIMBAL
void CommanderSrvc::onUartRxEventCallback(size_t size) {
#if REMOTE_DEVICE == REMOTE_DR16
    memcpy(&_dr16Data, rxbuf, size);
    HAL_UARTEx_ReceiveToIdle_DMA(&Config::Hardware::Comms::REMOTE_UART, rxbuf, sizeof(rxbuf));
    RemoteDR16::instance().onDataReceived();
#elif REMOTE_DEVICE == REMOTE_VIDEO_LINK
    memcpy(&_videoLinkRawData, rxbuf, size);
    HAL_UARTEx_ReceiveToIdle_DMA(&Config::Hardware::Comms::REMOTE_UART, rxbuf, sizeof(rxbuf));
    VideoLinkRemote::instance().onDataReceived();
#elif REMOTE_DEVICE == REMOTE_GAMEPAD
    memcpy(&_gamepadRawData, rxbuf, size);
    HAL_UARTEx_ReceiveToIdle_DMA(&Config::Hardware::Comms::REMOTE_UART, rxbuf, sizeof(rxbuf));
    BluetoothGamepad::instance().onDataReceived();
#endif
}

void CommanderSrvc::onUartErrCallback() {
    HAL_UARTEx_ReceiveToIdle_DMA(&Config::Hardware::Comms::REMOTE_UART, rxbuf, sizeof(rxbuf));
}
#endif
