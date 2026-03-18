/**
 *******************************************************************************
 * @file    commander.cpp
 * @brief   简要描述
 *******************************************************************************
 * @attention
 *
 * none
 *
 *******************************************************************************
 * @note
 *
 * none
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/2/27
 * @version 1.0
 *******************************************************************************
 */




/* ------- define ----------------------------------------------------------------------------------------------------*/




/* ------- include ---------------------------------------------------------------------------------------------------*/



/* I. header */

#include "commander.h"

#include "Board-Support-Pack/GamePad/bluetooth-gamepad.h"
#include "Config/config.h"

#include "System/DataHub/blackboard.h"

#include <algorithm>

/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit                        = CommanderSrvc::instance();

__attribute__((section(".dma_pool"))) static uint8_t rxbuf[32] = {0};


#ifdef GIMBAL
#if REMOTE_DEVICE == REMOTE_DR16
static RemoteDR16 _remoteDR16;
#endif
#if REMOTE_DEVICE == REMOTE_VIDEO_LINK
static VideoLinkRawData _videoLinkRawData;
#elif REMOTE_DEVICE == REMOTE_GAMEPAD
static GamepadRawData _gamepadRawData;
#elif REMOTE_DEVICE == REMOTE_DR16
static Dr16Data _dr16Data;
#endif
#endif

// --- 宏观运动参数限制 ---
// 宏观运动限制 (可根据机械结构调整)
// RemoteDR16& remote = RemoteDR16::instance();

/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "Input"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/





/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/

namespace {
inline float clampUnit(float v) { return std::max(-1.0f, std::min(1.0f, v)); }

inline float composeAxis(float joystick, float keyboard) { return clampUnit(joystick + keyboard); }
} // namespace


CommanderSrvc::CommanderSrvc()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 10),
      _joystickDeadzone(0.02f),
#if REMOTE_DEVICE != REMOTE_GAMEPAD || defined(CHASSIS)
      _work(-0.25f, 0.5f, false, HoldCondition::LessOrEqual),
      _trigFricToggle(-0.5f, 0.001f, true, HoldCondition::LessOrEqual),
      _triggerBurst(0.5f, 1.0f, true, HoldCondition::GreaterOrEqual),
      _trigSingleRelease(0.5f, 0.001f, true, HoldCondition::LessOrEqual),
      _trigQToggleBase(0.5f, 0.001f, true, HoldCondition::GreaterOrEqual), _trigQToggle(_trigQToggleBase, false),
      _trigShiftHold(0.5f, 0.001f, false, HoldCondition::GreaterOrEqual),
      _trigMouseRelease(-0.5f, 0.001f, true, HoldCondition::LessOrEqual),
      _trigMouseBurst(0.5f, 0.7f, false, HoldCondition::GreaterOrEqual), _trigSpin(baseTrigger, false)
#else
      _handbreak(0.0f, 0.001f, false, HoldCondition::GreaterOrEqual),
      _aTest(0.0f, 0.001f, true, HoldCondition::GreaterOrEqual), _relax(_aTest, true)

#endif
{
}


void CommanderSrvc::init() {
    /* ========================================================
     * 1. 建立 Action 映射绑定 (物理控件 -> 触发器过滤 -> 高级意图)
     * ======================================================== */

#if REMOTE_DEVICE == REMOTE_DR16

    // 【底盘平移】左摇杆 Y轴 -> 前后(X)；左摇杆 X轴 -> 左右(Y)
    actionMoveX.bind(RemoteDR16::instance().getLeftY(), &_joystickDeadzone);
    actionMoveY.bind(RemoteDR16::instance().getLeftX(), &_joystickDeadzone);

    // 【底盘旋转】右摇杆 X轴 -> 旋转(Spin)
    //  Spin.bind(RemoteDR16::instance().getRightX(), &_joystickDeadzone);

    // 【云台控制】右摇杆 Y轴 -> Pitch俯仰
    actionYaw.bind(RemoteDR16::instance().getRightX(), &_joystickDeadzone);
    actionPitch.bind(RemoteDR16::instance().getRightY(), &_joystickDeadzone);

    // 【模式切换】右开关 -> 控制模式仲裁 (传入 nullptr 代表直通，无须死区处理)
    actionCtrlMode.bind(RemoteDR16::instance().getSwRight(), &_work);


    actionFricToggle.bind(RemoteDR16::instance().getSwLeft(), &_trigFricToggle);
    actionShootBurst.bind(RemoteDR16::instance().getSwLeft(), &_triggerBurst);
    actionShootSingle.bind(RemoteDR16::instance().getSwLeft(), &_trigSingleRelease);
    actionSpinMode.bind(RemoteDR16::instance().getWheel(), &_trigSpin);
#elif REMOTE_DEVICE == REMOTE_VIDEO_LINK
    auto& videoRemote = VideoLinkRemote::instance();

    // 底盘输入：保留摇杆 Action，同时引入键盘轴 Action
    actionMoveX.bind(videoRemote.getRightY(), &_joystickDeadzone);
    actionMoveY.bind(videoRemote.getRightX(), &_joystickDeadzone);
    actionMoveXKey.bind(videoRemote.getAxisKeyWS());
    actionMoveYKey.bind(videoRemote.getAxisKeyAD());

    // 云台输入：鼠标增量
    actionYaw.bind(videoRemote.getAxis(AxisID::ViewYaw), &_joystickDeadzone);
    actionPitch.bind(videoRemote.getAxis(AxisID::ViewPitch), &_joystickDeadzone);
    actionMouseYaw.bind(videoRemote.getMouseX(), &_joystickDeadzone);
    actionMousePitch.bind(videoRemote.getMouseY(), &_joystickDeadzone);
    actionMouseLeftRaw.bind(videoRemote.getMouseLeft());


    // 【模式切换】右开关 -> 控制模式仲裁 (传入 nullptr 代表直通，无须死区处理)
    actionCtrlMode.bind(videoRemote.getModeSw(), &_work);


    actionFricToggle.bind(videoRemote.getKeyQ(), &_trigMouseFricEdge);
    actionSpinMode.bind(videoRemote.getKeyShift(), &_trigShiftHold);
#elif REMOTE_DEVICE == REMOTE_GAMEPAD
    // 前进和右扳机绑定
    actionMoveX.bind(BluetoothGamepad::instance().getRightTrigger(), &_joystickDeadzone);
    // 刹车和左扳机绑定
    actionBreak.bind(BluetoothGamepad::instance().getLeftTrigger(), &_joystickDeadzone);
    // 转向和左摇杆绑定
    actionYaw.bind(BluetoothGamepad::instance().getAxis(AxisID::ViewYaw), &_joystickDeadzone);
    // 下力和A键绑定
    actionRelax.bind(BluetoothGamepad::instance().getButtonA(), &_relax);
    // 手刹和B键绑定
    actionHandbrakeDepth.bind(BluetoothGamepad::instance().getButtonB(), &_handbreak);

#endif



    /* ========================================================
     * 2. 初始化硬件通信
     * ======================================================== */
    HAL_UARTEx_ReceiveToIdle_DMA(&Config::Hardware::Comms::REMOTE_UART, rxbuf, sizeof(rxbuf));
}

void CommanderSrvc::run() {
    /* =======================================  =================
     * 0. 计算时间步长 (dt)，用于动作系统内部的积分或时长判定
     * ======================================================== */

    static uint32_t last_tick = xTaskGetTickCount();
    uint32_t current_tick     = xTaskGetTickCount();
    float dt                  = (float)(current_tick - last_tick) / 1000.0f;

    if (dt <= 0.0f)
        dt = 0.001f; // 防止极高频或同Tick调用导致 dt 为 0
    last_tick = current_tick;

    /* ========================================================
     * 1. 硬件层：提取最新的遥控器 DMA 缓存数据
     * ======================================================== */
#ifdef GIMBAL
#if REMOTE_DEVICE == REMOTE_DR16
    RemoteDR16::instance().updateRaw(_dr16Data);
#elif REMOTE_DEVICE == REMOTE_VIDEO_LINK
    // 遥控器数据更新
    VideoLinkRemote::instance().updateRaw(_videoLinkRawData);
#elif REMOTE_DEVICE == REMOTE_GAMEPAD
    BluetoothGamepad::instance().updateRaw(_gamepadRawData);
#endif
#endif

    /* ========================================================
     * 2. 动作层：驱动所有 Action 执行死区过滤、归一化、仲裁计算
     * ======================================================== */
    // for (auto& a : _actions) {
    //     a.update(dt);
    // }
#if REMOTE_DEVICE != REMOTE_GAMEPAD || defined(CHASSIS)
    actionCtrlMode.update(dt);
    actionSateStop.update(dt);
    actionMoveX.update(dt);
    actionMoveY.update(dt);
    actionMoveXKey.update(dt);
    actionMoveYKey.update(dt);
    actionYaw.update(dt);
    actionMouseYaw.update(dt);
    actionMousePitch.update(dt);
    actionSpin.update(dt);
    actionPitch.update(dt);
    actionMouseLeftRaw.update(dt);
    actionFricToggle.update(dt);
    actionShootBurst.update(dt);
    actionShootSingle.update(dt);
    actionSpinMode.update(dt);
#else
    actionHandbrakeDepth.update(dt);
    actionBreak.update(dt);
    actionMoveX.update(dt);
    actionYaw.update(dt);
    actionRelax.update(dt);
#endif

    /* ========================================================
     * 3. 仲裁层 第一阶：决断控制源 (Control Source)
     * ======================================================== */
#ifdef GIMBAL
    // 默认最高安全等级，除非确认遥控器在线且给出运行指令
    /* 3. 第一阶仲裁：决断控制权 */

    ControlSource currentSource = ControlSource::SAFE_STOP;

    GimbalCmd gCmd{};
    ShootCmd sCmd{};
    GimbalToChassisComm comm{};
    ImuState imuState{};

#if REMOTE_DEVICE != REMOTE_GAMEPAD // 非游戏手柄控制
    if (remote.isConnected()) {
        // 读取完美归一化后的浮点数：-1.0f(上), 0.0f(中), 1.0f(下)
        float swState = actionCtrlMode.getValue();

#if REMOTE_DEVICE == REMOTE_DR16
        if (actionCtrlMode.isTriggered()) {
            if (swState > -0.5f) {
                currentSource = ControlSource::REMOTE;
            } else {
                currentSource = ControlSource::VISION;
            }
        } else {
            currentSource = ControlSource::SAFE_STOP;
        }
#elif REMOTE_DEVICE == REMOTE_VIDEO_LINK
        if (swState < 0) {
            currentSource = ControlSource::SAFE_STOP;
        } else if (swState > 0) {
            currentSource = ControlSource::VISION;
        } else {
            currentSource = ControlSource::REMOTE;
        }
#endif

    } else {
        currentSource = ControlSource::SAFE_STOP;
    }

    /* ========================================================
     * 4. 仲裁层 第二阶：根据控制源填充控制指令
     * ======================================================== */


    // 【关键】先从黑板中 Read 出上一帧的历史指令。
    // 如果后续不修改它，写回的就是历史值，天然实现“状态无缝保留”。

    Blackboard::instance().gimbalCmd.read(gCmd);
    Blackboard::instance().shootCmd.read(sCmd);
    Blackboard::instance().imuState.read(imuState);


    sCmd.event = ShootEvent::NONE;
    switch (currentSource) {
        case ControlSource::SAFE_STOP: {
            // 彻底切断底层动力
            comm.msg.mode = (uint8_t)CHASSIS_RELAX;
            sCmd.event    = ShootEvent::EMERGENCY_STOP;
            sCmd.state.burstShot = 0;
            gCmd.mode     = GIMBAL_RELAX;

            gCmd.yawVel   = 0;
            gCmd.pitchVel = 0;


        } break;

        case ControlSource::REMOTE: {
            // 遥控器映射
            if (actionSpinMode.isTriggered()) {
                comm.msg.mode = CHASSIS_SPIN;
            } else {
                comm.msg.mode = CHASSIS_NORMAL;
            }

#if REMOTE_DEVICE == REMOTE_VIDEO_LINK
            const float moveXInput = composeAxis(actionMoveX.getValue(), actionMoveXKey.getValue());
            const float moveYInput = composeAxis(actionMoveY.getValue(), actionMoveYKey.getValue());
            const float yawInput =
                composeAxis(actionMouseYaw.getValue() * Config::Algorithm::Input::Y_SENSITIVITY, actionYaw.getValue());
            const float pitchInput = composeAxis(actionMousePitch.getValue() * Config::Algorithm::Input::X_SENSITIVITY,
                                                 actionPitch.getValue());
#else
            const float moveXInput = actionMoveX.getValue();
            const float moveYInput = actionMoveY.getValue();
            const float yawInput   = actionYaw.getValue();
            const float pitchInput = actionPitch.getValue();
#endif

            static float vx, vy;
            vx = comm.msg.vx         = moveXInput * Config::Algorithm::Chassis::MAX_VX * 10;
            // 运动计算坐标系和遥控器方向相反
            vy = comm.msg.vy         = -moveYInput * Config::Algorithm::Chassis::MAX_VY * 10;


            gCmd.mode           = GIMBAL_NORMAL;
            // 遥控器的Y轴与标准正方向（左）相反, X轴与标准正方向(下)相反
            gCmd.yawVel         = -yawInput * Config::Algorithm::Gimbal::MAX_YAW_SPEED;
            gCmd.targetYawSpeed = 0;
            gCmd.pitchVel       = -pitchInput * Config::Algorithm::Gimbal::MAX_PITCH_SPEED;


            // 发射事件映射
#if REMOTE_DEVICE == REMOTE_VIDEO_LINK
            if (actionFricToggle.isTriggered()) {
                sCmd.event = ShootEvent::FRIC_TOGGLE;
            }

            const float mouseVal             = actionMouseLeftRaw.getValue();
            const TriggerState burstHoldState   = _trigMouseBurst.update(mouseVal, dt);
            const TriggerState singleClickState = _trigMouseSingle.update(mouseVal, dt);

            const bool isBurstingNow            = (burstHoldState == TriggerState::Triggered);
            const TriggerState burstEdgeState   = _trigMouseBurstEdge.update(isBurstingNow ? 1.0f : 0.0f, dt);

            // 持续态每帧同步，避免事件丢失后连发状态与输入脱节。
            sCmd.state.burstShot = isBurstingNow ? 1 : 0;

            if (burstEdgeState == TriggerState::Triggered) {
                sCmd.event = isBurstingNow ? ShootEvent::BURST_START : ShootEvent::BURST_STOP;
            } else if (singleClickState == TriggerState::Triggered) {
                sCmd.event = ShootEvent::SINGLE_FIRE;
            }
#else
            if (actionFricToggle.isTriggered()) {
                sCmd.event = ShootEvent::FRIC_TOGGLE;
            } else if (actionShootBurst.isTriggered()) {
                sCmd.event = ShootEvent::BURST_START;
            } else if (actionShootSingle.isTriggered()) {
                sCmd.event = ShootEvent::SINGLE_FIRE;
            }
            sCmd.state.burstShot = 0;
#endif

        } break;

        case ControlSource::VISION: {
            // 切换为自动模式标志位，底层算法任务接收到此 Mode 后将使用视觉逻辑
            comm.msg.mode = CHASSIS_NORMAL; // 视觉通常也需要底盘跟随
            gCmd.mode     = GIMBAL_AUTO;

            // 1. 读取视觉指令
            VisionCommand vCmd{};
            Blackboard::instance().visionCmd.read(vCmd);

            // 2. 将视觉指令映射到云台控制结构体
            gCmd.targetYaw             = vCmd.targetYaw;
            gCmd.targetPitch           = -vCmd.targetPitch;
            gCmd.targetYawSpeed        = vCmd.targetYawSpeed;
            gCmd.targetYawAcceleration = vCmd.targetYawAcceleration;

            const uint8_t curFire = vCmd.fireCommand;
            const uint8_t curSingle = vCmd.isSingleShot;

            const bool isFiring = (curFire == 1U);
            const TriggerState vBurstEdgeState = _visionBurstEdge.update(isFiring ? 1.0f : 0.0f, dt);

            if (actionFricToggle.isTriggered()) {
                sCmd.event = ShootEvent::FRIC_TOGGLE;
            }

            if (curSingle == 1U) {
                if (vBurstEdgeState == TriggerState::Triggered && isFiring) {
                    sCmd.event = ShootEvent::SINGLE_FIRE;
                }
                if (sCmd.state.burstShot == 1U) {
                    sCmd.event = ShootEvent::BURST_STOP;
                    sCmd.state.burstShot = 0;
                }
            } else {
                // 持续态每帧同步，事件丢失也不会导致状态机卡死。
                sCmd.state.burstShot = isFiring ? 1 : 0;
                if (vBurstEdgeState == TriggerState::Triggered) {
                    if (isFiring) {
                        sCmd.event = ShootEvent::BURST_START;
                    } else {
                        sCmd.event = ShootEvent::BURST_STOP;
                    }
                }
            }

        } break;
        default:
            break;
    }
    gCmd.timestamp = current_tick;

    /* ========================================================
     * 5. 发布层：将仲裁后的最终真理写入黑板
     * ======================================================== */

#else
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


    Blackboard::instance().g2cOutput.write(comm);
    Blackboard::instance().gimbalCmd.write(gCmd);
    Blackboard::instance().shootCmd.write(sCmd);
#elif defined(CHASSIS)
    GimbalToChassisComm comm{};
    ChassisCmd cmd{};
    Blackboard::instance().rComm.read(comm);
    cmd.mode = comm.msg.mode;
    cmd.vx   = (float)comm.msg.vx / 10;
    cmd.vy   = (float)comm.msg.vy / 10;
    if (cmd.mode == CHASSIS_NORMAL) {
        cmd.vw = 0;
    } else if (cmd.mode == CHASSIS_SPIN) {
        cmd.vw = Config::Algorithm::Chassis::MAX_VW;
    }

    Blackboard::instance().chassisCmd.write(cmd);

#endif
}

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