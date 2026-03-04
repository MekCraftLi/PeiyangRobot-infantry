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
#include "Config/Chassis/hw-config.h"
#include "Config/Gimbal/hw-config.h"
#include "Config/config.h"

#include "System/DataHub/blackboard.h"

/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit                        = CommanderSrvc::instance();

__attribute__((section(".dma_pool"))) static uint8_t rxbuf[32] = {0};

static Dr16Data dr16Data;


// --- 宏观运动参数限制 ---
// 宏观运动限制 (可根据机械结构调整)
RemoteDR16& remote             = RemoteDR16::instance();

/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "Input"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/





/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/





CommanderSrvc::CommanderSrvc()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 10),
      _joystickDeadzone(0.02f), _work(-0.25f, 0.5f, false, HoldCondition::LessOrEqual),
      _trigFricToggle(-0.5f, 0.001f, true, HoldCondition::LessOrEqual),
      _triggerBurst(0.5f, 1.0f, true, HoldCondition::GreaterOrEqual),
      _trigSingleRelease(0.5f, 0.001f, true, HoldCondition::LessOrEqual), _trigSpin(baseTrigger, false) {}

void CommanderSrvc::init() {
    /* ========================================================
     * 1. 建立 Action 映射绑定 (物理控件 -> 触发器过滤 -> 高级意图)
     * ======================================================== */

    // 【底盘平移】左摇杆 Y轴 -> 前后(X)；左摇杆 X轴 -> 左右(Y)
    actionMoveX.bind(RemoteDR16::instance().getLeftY(), &_joystickDeadzone);
    actionMoveY.bind(RemoteDR16::instance().getLeftX(), &_joystickDeadzone);

    // 【底盘旋转】右摇杆 X轴 -> 旋转(Spin)
    //  Spin.bind(RemoteDR16::instance().getRightX(), &_joystickDeadzone);

    // 【云台控制】右摇杆 Y轴 -> Pitch俯仰
    actionYaw.bind(RemoteDR16::instance().getRightX(), &_joystickDeadzone);
    actiongPitch.bind(RemoteDR16::instance().getRightY(), &_joystickDeadzone);

    // 【模式切换】右开关 -> 控制模式仲裁 (传入 nullptr 代表直通，无须死区处理)
    actionCtrlMode.bind(RemoteDR16::instance().getSwRight(), &_work);


    actionFricToggle.bind(RemoteDR16::instance().getSwLeft(), &_trigFricToggle);
    actionShootBurst.bind(RemoteDR16::instance().getSwLeft(), &_triggerBurst);
    actionShootSingle.bind(RemoteDR16::instance().getSwLeft(), &_trigSingleRelease);
    actionSpinMode.bind(RemoteDR16::instance().getWheel(), &_trigSpin);



    /* ========================================================
     * 2. 初始化硬件通信
     * ======================================================== */
    HAL_UARTEx_ReceiveToIdle_DMA(&Config::Hardware::Comms::REMOTE_UART, rxbuf, sizeof(rxbuf));
}

void CommanderSrvc::run() {
    /* ========================================================
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
    RemoteDR16::instance().updateRaw(dr16Data);

    /* ========================================================
     * 2. 动作层：驱动所有 Action 执行死区过滤、归一化、仲裁计算
     * ======================================================== */
    // for (auto& a : _actions) {
    //     a.update(dt);
    // }
    actionCtrlMode.update(dt);
    actionSateStop.update(dt);
    actionMoveX.update(dt);
    actionMoveY.update(dt);
    actionSpin.update(dt);
    actionYaw.update(dt);
    actiongPitch.update(dt);
    actionFricToggle.update(dt);
    actionShootBurst.update(dt);
    actionShootSingle.update(dt);
    actionSpinMode.update(dt);

    /* ========================================================
     * 3. 仲裁层 第一阶：决断控制源 (Control Source)
     * ======================================================== */
#ifdef GIMBAL
    // 默认最高安全等级，除非确认遥控器在线且给出运行指令
    /* 3. 第一阶仲裁：决断控制权 */
    ControlSource currentSource = ControlSource::SAFE_STOP;

    if (RemoteDR16::instance().isConnected()) {
        // 读取完美归一化后的浮点数：-1.0f(上), 0.0f(中), 1.0f(下)
        float swState = actionCtrlMode.getValue();

        if (actionCtrlMode.isTriggered()) {
            if (swState > -0.5f) {
                currentSource = ControlSource::REMOTE;
            } else {
                currentSource = ControlSource::VISION;
            }
        } else {
            currentSource = ControlSource::SAFE_STOP;
        }

    } else {
        currentSource = ControlSource::SAFE_STOP;
    }

    /* ========================================================
     * 4. 仲裁层 第二阶：根据控制源填充控制指令
     * ======================================================== */

    GimbalCmd finalGimbalCmd{};
    ShootCmd finalShootCmd{};
    GimbalToChassisComm g2cComm{};
    ImuState imuState{};


    // 【关键】先从黑板中 Read 出上一帧的历史指令。
    // 如果后续不修改它，写回的就是历史值，天然实现“状态无缝保留”。

    Blackboard::instance().gimbalCmd.read(finalGimbalCmd);
    Blackboard::instance().shootCmd.read(finalShootCmd);
    Blackboard::instance().imuState.read(imuState);


    finalShootCmd.event = ShootEvent::NONE;
    switch (currentSource) {
        case ControlSource::SAFE_STOP: {
            // 彻底切断底层动力
            g2cComm.msg.mode        = (uint8_t)CHASSIS_RELAX;
            finalShootCmd.event     = ShootEvent::EMERGENCY_STOP;
            finalGimbalCmd.mode     = GIMBAL_RELAX;

            finalGimbalCmd.yawVel   = 0;
            finalGimbalCmd.pitchVel = 0;

        } break;

        case ControlSource::REMOTE: {
            // 遥控器映射
            if (actionSpinMode.isTriggered()) {
                g2cComm.msg.mode = CHASSIS_SPIN;
            } else {
                g2cComm.msg.mode = CHASSIS_NORMAL;
            }
            g2cComm.msg.vx          = actionMoveX.getValue() * Config::Algorithm::Chassis::MAX_VX * 10;
            // 运动计算坐标系和遥控器方向相反
            g2cComm.msg.vy          = -actionMoveY.getValue() * Config::Algorithm::Chassis::MAX_VY * 10;


            finalGimbalCmd.mode     = GIMBAL_RC;
            float yawInput          = actionYaw.getValue();
            float pitchInput        = actiongPitch.getValue();

            // 遥控器的Y轴与标准正方向（左）相反, X轴与标准正方向(下)相反
            finalGimbalCmd.yawVel   = -yawInput * Config::Algorithm::Gimbal::MAX_YAW_SPEED;
            finalGimbalCmd.pitchVel = -pitchInput * Config::Algorithm::Gimbal::MAX_PITCH_SPEED;


            // 发射事件映射
            if (actionFricToggle.isTriggered()) {
                finalShootCmd.event = ShootEvent::FRIC_TOGGLE;
            } else if (actionShootBurst.isTriggered()) {
                finalShootCmd.event = ShootEvent::BURST_START;
            } else if (actionShootSingle.isTriggered()) {
                finalShootCmd.event = ShootEvent::SINGLE_FIRE;
            }

        } break;

        case ControlSource::VISION: {
            // 切换为自动模式标志位，底层算法任务接收到此 Mode 后将使用视觉逻辑
            g2cComm.msg.mode    = CHASSIS_NORMAL;
            finalGimbalCmd.mode = GIMBAL_AUTO;

            // 可以在此处从 Vision 接收缓冲中提取数据并覆盖目标值
            // 如果不作操作，由于提前 Read 了历史数据，底盘将以切换瞬间的速度继续运动
        } break;

        default:
            break;
    }
    finalGimbalCmd.timestamp = current_tick;

    /* ========================================================
     * 5. 发布层：将仲裁后的最终真理写入黑板
     * ======================================================== */
    Blackboard::instance().g2cOutput.write(g2cComm);
    Blackboard::instance().gimbalCmd.write(finalGimbalCmd);
    Blackboard::instance().shootCmd.write(finalShootCmd);
#elifdef CHASSIS
    GimbalToChassisComm g2cComm{};
    ChassisCmd cmd{};
    Blackboard::instance().rComm.read(g2cComm);
    cmd.mode = g2cComm.msg.mode;
    cmd.vx   = (float)g2cComm.msg.vx / 10;
    cmd.vy   = (float)g2cComm.msg.vy / 10;
    if (cmd.mode == CHASSIS_NORMAL) {
        cmd.vw =0;
    } else if (cmd.mode == CHASSIS_SPIN) {
        cmd.vw = Config::Algorithm::Chassis::MAX_VW;
    }

    Blackboard::instance().chassisCmd.write(cmd);

#endif
}
void CommanderSrvc::onUartRxEventCallback(size_t size) {

    memcpy(&dr16Data, rxbuf, size);
    HAL_UARTEx_ReceiveToIdle_DMA(&Config::Hardware::Comms::REMOTE_UART, rxbuf, sizeof(rxbuf));
    RemoteDR16::instance().onDataReceived();
}
void CommanderSrvc::onUartErrCallback() {
    HAL_UARTEx_ReceiveToIdle_DMA(&Config::Hardware::Comms::REMOTE_UART, rxbuf, sizeof(rxbuf));
}

