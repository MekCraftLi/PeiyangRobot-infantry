/**
 *******************************************************************************
 * @file    input.cpp
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


// 全局动作实例 (通常放在一个 GlobalContext 或单例中)
InputAction Action_MoveX;
InputAction Action_MoveY;
InputAction Action_Yaw;
InputAction Action_Pitch;
InputAction Action_FrictionOn;
InputAction Action_FireSingle;
InputAction Action_FireBurst;
InputAction Action_ModeSwitch; // 稍微复杂一点，处理三种模式

// 实例化全局的 Action 对象
InputAction Action_CtrlMode;
InputAction Action_FuncMode;

InputAction Action_Spin;

InputAction Action_GimbalYaw;
InputAction Action_GimbalPitch;


// --- 宏观运动参数限制 ---
// 宏观运动限制 (可根据机械结构调整)
RemoteDR16& remote     = RemoteDR16::instance();

/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "Input"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/





/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/

namespace Actions {
// 1. 系统级意图 (仲裁器专用)
InputAction CtrlMode; // 控制源切换 (DR16, 视觉, 键盘)
InputAction SafeStop; // 物理急停

// 2. 底盘意图 (底盘任务专用)
InputAction MoveX;
InputAction MoveY;
InputAction Spin;

// 3. 云台意图 (云台任务专用)
InputAction GimbalYaw;
InputAction GimbalPitch;
InputAction FireSingle;

} // namespace Actions



CommanderSrvc::CommanderSrvc()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 1),
      _joystickDeadzone(0.02f) // 设定 2% 的死区，防止摇杆不回中导致漂移
{}

void CommanderSrvc::init() {
    /* ========================================================
         * 1. 建立 Action 映射绑定 (物理控件 -> 触发器过滤 -> 高级意图)
         * ======================================================== */

    // 【底盘平移】左摇杆 Y轴 -> 前后(X)；左摇杆 X轴 -> 左右(Y)
    Actions::MoveX.bind(RemoteDR16::instance().getLeftY(), &_joystickDeadzone);
    Actions::MoveY.bind(RemoteDR16::instance().getLeftX(), &_joystickDeadzone);

    // 【底盘旋转】右摇杆 X轴 -> 旋转(Spin)
    //Actions::Spin.bind(RemoteDR16::instance().getRightX(), &_joystickDeadzone);

    // 【云台控制】右摇杆 Y轴 -> Pitch俯仰
    Actions::GimbalYaw.bind(RemoteDR16::instance().getRightX(), &_joystickDeadzone);
    Actions::GimbalPitch.bind(RemoteDR16::instance().getRightY(), &_joystickDeadzone);

    // 【模式切换】右开关 -> 控制模式仲裁 (传入 nullptr 代表直通，无须死区处理)
    Actions::CtrlMode.bind(RemoteDR16::instance().getSwRight(), nullptr);

    /* ========================================================
     * 2. 初始化硬件通信
     * ======================================================== */
    HAL_UARTEx_ReceiveToIdle_DMA(&REMOTE_UART, rxbuf, sizeof(rxbuf));
}

void CommanderSrvc::run() {
/* ========================================================
     * 0. 计算时间步长 (dt)，用于动作系统内部的积分或时长判定
     * ======================================================== */
    static uint32_t last_tick = xTaskGetTickCount();
    uint32_t current_tick = xTaskGetTickCount();
    float dt = (current_tick - last_tick) / 1000.0f;

    if (dt <= 0.0f) dt = 0.001f; // 防止极高频或同Tick调用导致 dt 为 0
    last_tick = current_tick;

    /* ========================================================
     * 1. 硬件层：提取最新的遥控器 DMA 缓存数据
     * ======================================================== */
    RemoteDR16::instance().updateRaw(dr16Data);

    /* ========================================================
     * 2. 动作层：驱动所有 Action 执行死区过滤、归一化、仲裁计算
     * ======================================================== */
    Actions::MoveX.update(dt);
    Actions::MoveY.update(dt);
    Actions::GimbalYaw.update(dt);
    Actions::GimbalPitch.update(dt);
    Actions::CtrlMode.update(dt);

    /* ========================================================
     * 3. 仲裁层 第一阶：决断控制源 (Control Source)
     * ======================================================== */
    // 默认最高安全等级，除非确认遥控器在线且给出运行指令
    /* 3. 第一阶仲裁：决断控制权 */
    ControlSource current_source = ControlSource::SAFE_STOP;

    if (RemoteDR16::instance().isConnected()) {
        // 读取完美归一化后的浮点数：-1.0f(上), 0.0f(中), 1.0f(下)
        float sw_state = Actions::CtrlMode.getValue();

        // 浮点数区间判断，具有极高的鲁棒性
        if (sw_state > 0.25f) {
            // 接近 1.0f -> 拨杆在下 -> 需求：所有电机无力
            current_source = ControlSource::SAFE_STOP;
        } else if (sw_state > -0.5f) {
            // 接近 0.0f -> 拨杆在中 -> 需求：接收遥控器控制
            current_source = ControlSource::REMOTE;
        } else {
            // 接近 -1.0f -> 拨杆在上 -> 需求：状态保留/其它输入源
            current_source = ControlSource::VISION;
        }
    }

    /* ========================================================
     * 4. 仲裁层 第二阶：根据控制源填充控制指令
     * ======================================================== */
    ChassisCmd finalChassisCmd;
    GimbalCmd  finalGimbalCmd;

    static float targetYawRad = 0.0f;
    static float targetPitchRad = 0.0f;


    // 【关键】先从黑板中 Read 出上一帧的历史指令。
    // 如果后续不修改它，写回的就是历史值，天然实现“状态无缝保留”。
    Blackboard::instance().chassisCmd.read(finalChassisCmd);

    Blackboard::instance().gimbal_cmd.read(finalGimbalCmd);

    switch (current_source) {
        case ControlSource::SAFE_STOP: {
            // 彻底切断底层动力
            finalChassisCmd.mode = CHASSIS_RELAX;
            finalGimbalCmd.mode  = GIMBAL_RELAX;
        }break;

        case ControlSource::REMOTE: {
            // 遥控器映射
            finalChassisCmd.mode = CHASSIS_RC;
            finalChassisCmd.vx = Actions::MoveX.getValue() * Config::Algorithm::Chassis::MAX_VX;
            // 运动计算坐标系和遥控器方向相反
            finalChassisCmd.vy = -Actions::MoveY.getValue() * Config::Algorithm::Chassis::MAX_VY;
            finalChassisCmd.vw = -Actions::Spin.getValue()  * Config::Algorithm::Chassis::MAX_VW;

            finalGimbalCmd.mode  = GIMBAL_RC;
            float yawInput = Actions::GimbalYaw.getValue();
            float pitchInput = Actions::GimbalPitch.getValue();

            float deltaYaw = yawInput * Config::Algorithm::Gimbal::MAX_YAW_SPEED * dt;
            float deltaPitch = pitchInput * Config::Algorithm::Gimbal::MAX_PITCH_SPEED * dt;

            targetYawRad += deltaYaw;
            targetPitchRad += deltaPitch;

            if (targetPitchRad > Config::Algorithm::Gimbal::MAX_PITCH_ANGLE) {
                targetPitchRad = Config::Algorithm::Gimbal::MAX_PITCH_ANGLE;
            } else if (targetPitchRad < Config::Algorithm::Gimbal::MIN_PITCH_ANGLE) {
                targetPitchRad = Config::Algorithm::Gimbal::MIN_PITCH_ANGLE;
            }

            while (targetYawRad > pyro::PI) targetYawRad -= 2.0f * pyro::PI;
            while (targetYawRad < -pyro::PI) targetYawRad += 2.0f * pyro::PI;

            finalGimbalCmd.yawRad = targetYawRad;
            finalGimbalCmd.pitchRad = targetPitchRad;
        }break;

        case ControlSource::VISION: {
            // 切换为自动模式标志位，底层算法任务接收到此 Mode 后将使用视觉逻辑
            finalChassisCmd.mode = CHASSIS_AUTO;
            finalGimbalCmd.mode  = GIMBAL_AUTO;

            // 可以在此处从 Vision 接收缓冲中提取数据并覆盖目标值
            // 如果不作操作，由于提前 Read 了历史数据，底盘将以切换瞬间的速度继续运动
        }break;

        default:
            break;
    }

    finalChassisCmd.timestamp = current_tick;
    finalGimbalCmd.timestamp  = current_tick;

    /* ========================================================
     * 5. 发布层：将仲裁后的最终真理写入黑板
     * ======================================================== */
    Blackboard::instance().chassisCmd.Write(finalChassisCmd);
    Blackboard::instance().gimbal_cmd.Write(finalGimbalCmd);
}
extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {

    memcpy(&dr16Data, rxbuf, Size);

    HAL_UARTEx_ReceiveToIdle_DMA(&REMOTE_UART, rxbuf, sizeof(rxbuf));
}



extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
    // 1. 禁用 UART DMA
    HAL_UART_DMAStop(huart);
    // 2. 清除 UART 错误标志
    __HAL_UART_CLEAR_FEFLAG(huart);  // 帧错误
    __HAL_UART_CLEAR_NEFLAG(huart);  // 噪声错误
    __HAL_UART_CLEAR_OREFLAG(huart); // 溢出错误
    HAL_UARTEx_ReceiveToIdle_DMA(&REMOTE_UART, rxbuf, sizeof(rxbuf));
}
