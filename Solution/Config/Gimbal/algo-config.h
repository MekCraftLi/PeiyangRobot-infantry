/**
 *******************************************************************************
 * @file    algo-config.h
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
 * @date    2026/3/1
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_ALGO_CONFIG_H
#define INFANTRY_ALGO_CONFIG_H



/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

//#include "../config.h"

#include <cstdint>




/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/


namespace Config::Algorithm {

// 统一的 PID 参数结构体
struct PidParam {
    float kp, ki, kd;
    float integral_limit;
    float max_out;
};

// ========================================
// 1. 底盘控制参数
// ========================================
namespace Chassis {
// 宏观运动速度限制
constexpr float MAX_VX = 30.0f; // 前后最大平移速度 (m/s)
constexpr float MAX_VY = 30.0f; // 左右最大平移速度 (m/s)
constexpr float MAX_VW = 5.0f; // 最大旋转角速度 (rad/s)

// 动力轮速度环 PID 默认参数 (需根据实际整定)
constexpr PidParam DRIVE_SPD_PID = {10.0f, 0.1f, 0.0f, 5000.0f, 16384.0f};

// 航向舵位置环 (外环) PID
constexpr PidParam STEER_POS_PID = {5.0f, 0.0f, 0.0f, 10.0f, 50.0f};
// 航向舵速度环 (内环) PID
constexpr PidParam STEER_SPD_PID = {8.0f, 0.1f, 0.0f, 5000.0f, 16384.0f};

constexpr float DM_MOTOR_PMAX = 12.5f;
constexpr float DM_MOTOR_VMAX = 30.0f;
constexpr float DM_MOTOR_TMAX = 10.0f;

}

namespace Gimbal {
// 遥控器推满时，云台的最大旋转速度 (rad/s)
constexpr float MAX_YAW_SPEED   = 6.28f; // 约 180度/秒
constexpr float MAX_PITCH_SPEED = 6.28f;  // 约 114度/秒
constexpr float YAW_INERTIA_K = 2.3f;

constexpr float PITCH_K_GRAVITY_COS = -0.8f; // 水平方向质心补偿
constexpr float PITCH_K_GRAVITY_SIN = -0.5f; // 垂直方向质心补偿

constexpr int32_t TRIGGER_ECD_CIRCLE     = 8192 * 36;
constexpr int32_t TRIGGER_ECD_PER_BULLET = TRIGGER_ECD_CIRCLE / 8;


enum aim_target
{
    armor,
    rune
};



#define DOG_2
//#define STEER





#ifdef DOG_1

//自瞄模式下-------------------------------------------

//yaw轴速度环pid参数
#define AUTO_YAW_SPEED_PID_KP 28.0f
#define AUTO_YAW_SPEED_PID_KI 0.0f
#define AUTO_YAW_SPEED_PID_KD 0.0f

//yaw轴位置环pid参数
#define AUTO_YAW_POS_PID_KP 27.0f
#define AUTO_YAW_POS_PID_KI 0.0f
#define AUTO_YAW_POS_PID_KD 0.3f

//pitch轴达妙mit控制阻抗系数
#define AUTO_DM_MOT_PITCH_KP 20.0f
#define AUTO_DM_MOT_PITCH_KI 0.0f
#define AUTO_DM_MOT_PITCH_KD 0.7f

//pitch轴达妙mit控制的重力补偿的pid的参数
#define AUTO_PITCH_DM_MOT_KP 28.0f
#define AUTO_PITCH_DM_MOT_KI 0.0f
#define AUTO_PITCH_DM_MOT_KD 0.5f

//手动模式下------------------------------------------
//yaw轴速度环pid参数
#define YAW_SPEED_PID_KP 15.0f
#define YAW_SPEED_PID_KI 0.0f
#define YAW_SPEED_PID_KD 0.0f

//yaw轴位置环pid参数
#define YAW_POS_PID_KP 25.0f
#define YAW_POS_PID_KI 0.0f
#define YAW_POS_PID_KD 0.3f

//pitch轴达妙mit控制阻抗系数
#define DM_MOT_PITCH_KP 20.0f
#define DM_MOT_PITCH_KI 0.0f
#define DM_MOT_PITCH_KD 0.7f

//pitch轴达妙mit控制的重力补偿的pid的参数
#define PITCH_DM_MOT_KP 28.0f
#define PITCH_DM_MOT_KI 0.0f
#define PITCH_DM_MOT_KD 0.5f


//-----------------------------------------------------


//拨弹盘单发情况速度环pid参数
#define TRIGGER_SINGLE_SPEED_PID_KP 0.13f
#define TRIGGER_SINGLE_SPEED_PID_KI 0.0f
#define TRIGGER_SINGLE_SPEED_PID_KD 0.0f

//拨弹盘单发情况位置环pid参数
#define TRIGGER_SINGLE_POS_PID_KP 2300.0f
#define TRIGGER_SINGLE_POS_PID_KI 0.0f
#define TRIGGER_SINGLE_POS_PID_KD 0.0f

//拨弹盘连发情况速度环pid参数
#define TRIGGER_BURST_SPEED_PID_KP 0.13f
#define TRIGGER_BURST_SPEED_PID_KI 0.0f
#define TRIGGER_BURST_SPEED_PID_KD 0.0002f

//pitch轴物理限幅参数
#define PITCH_LIMIT_MAX -2.80f
#define PITCH_LIMIT_MIN -1.61f

#define PITCH_ALIGN_TARGET_RAD  -0.7f

//yaw轴初始偏移角
#define _YAW_OFFSET 2550

//摩擦轮速度环pid参数
#define FRIC_SPEED_PID_KP 0.3f
#define FRIC_SPEED_PID_KI 0.0f
#define FRIC_SPEED_PID_KD 0.00002f

//弹速修正系数
#define BULLET_SPEED_COMP_KP 0.8125f

// 发射速度 (发/秒)
constexpr float SHOOT_SPEED                                       = 10.0f;

#define TRIGGER_MOTOR_ID pyro::dji_motor_tx_frame_t::id_3




// // 正常模式下使用的零偏补偿值 (这些值应通过开启 IMU_CALIBRATION_MODE 测量后填入)
// constexpr float GYRO_BIAS_X = -0.000265247712f;
// constexpr float GYRO_BIAS_Y = 0.00205873931f;
// constexpr float GYRO_BIAS_Z = -0.00308620022f;




#endif

#ifdef DOG_2

//自瞄模式下-------------------------------------------

//yaw轴速度环pid参数
#define AUTO_YAW_SPEED_PID_KP 40.0f
#define AUTO_YAW_SPEED_PID_KI 0.0f
#define AUTO_YAW_SPEED_PID_KD 0.0f

//yaw轴位置环pid参数
#define AUTO_YAW_POS_PID_KP 23.0f
#define AUTO_YAW_POS_PID_KI 0.0f
#define AUTO_YAW_POS_PID_KD 0.0f

//pitch轴达妙mit控制阻抗系数
#define AUTO_DM_MOT_PITCH_KP 20.0f
#define AUTO_DM_MOT_PITCH_KI 0.0f
#define AUTO_DM_MOT_PITCH_KD 0.7f

//pitch轴达妙mit控制的重力补偿的pid的参数
#define AUTO_PITCH_DM_MOT_KP 28.0f
#define AUTO_PITCH_DM_MOT_KI 0.0f
#define AUTO_PITCH_DM_MOT_KD 0.5f

//手动模式下------------------------------------------

//yaw轴速度环pid参数
#define YAW_SPEED_PID_KP 15.0f
#define YAW_SPEED_PID_KI 0.0f
#define YAW_SPEED_PID_KD 0.0f

//yaw轴位置环pid参数
#define YAW_POS_PID_KP 27.0f
#define YAW_POS_PID_KI 0.0f
#define YAW_POS_PID_KD 0.0f

//yaw轴初始偏移角
#define _YAW_OFFSET 2300

//pitch轴达妙mit控制阻抗系数
#define DM_MOT_PITCH_KP 25.0f
#define DM_MOT_PITCH_KI 0.0f
#define DM_MOT_PITCH_KD 0.7f

//pitch轴达妙mit控制的重力补偿的pid的参数
#define PITCH_DM_MOT_KP 28.0f
#define PITCH_DM_MOT_KI 0.0f
#define PITCH_DM_MOT_KD 0.5f

//pitch轴物理限幅参数
#define PITCH_LIMIT_MAX 1.70f
#define PITCH_LIMIT_MIN 2.94f

#define PITCH_ALIGN_TARGET_RAD -0.4f

//--------------------------------------------------------

//拨弹盘单发情况速度环pid参数
#define TRIGGER_SINGLE_SPEED_PID_KP 0.16f
#define TRIGGER_SINGLE_SPEED_PID_KI 0.0f
#define TRIGGER_SINGLE_SPEED_PID_KD 0.0f

//拨弹盘单发情况位置环pid参数
#define TRIGGER_SINGLE_POS_PID_KP 2300.0f
#define TRIGGER_SINGLE_POS_PID_KI 0.0f
#define TRIGGER_SINGLE_POS_PID_KD 0.0f

//拨弹盘连发情况速度环pid参数
#define TRIGGER_BURST_SPEED_PID_KP 0.16f
#define TRIGGER_BURST_SPEED_PID_KI 0.0f
#define TRIGGER_BURST_SPEED_PID_KD 0.0002f

//摩擦轮速度环pid参数
#define FRIC_SPEED_PID_KP 0.3f
#define FRIC_SPEED_PID_KI 0.0f
#define FRIC_SPEED_PID_KD 0.00002f

//弹速修正系数
#define BULLET_SPEED_COMP_KP 0.77f

// 发射速度 (发/秒)
constexpr float SHOOT_SPEED = 15.0f;

#define TRIGGER_MOTOR_ID pyro::dji_motor_tx_frame_t::id_2







#endif

#ifdef STEER

//自瞄模式下-------------------------------------------

//yaw轴速度环pid参数
#define AUTO_YAW_SPEED_PID_KP 40.0f
#define AUTO_YAW_SPEED_PID_KI 0.0f
#define AUTO_YAW_SPEED_PID_KD 0.0f

//yaw轴位置环pid参数
#define AUTO_YAW_POS_PID_KP 23.0f
#define AUTO_YAW_POS_PID_KI 0.0f
#define AUTO_YAW_POS_PID_KD 0.3f

//pitch轴达妙mit控制阻抗系数
#define AUTO_DM_MOT_PITCH_KP 20.0f
#define AUTO_DM_MOT_PITCH_KI 0.0f
#define AUTO_DM_MOT_PITCH_KD 0.7f

//pitch轴达妙mit控制的重力补偿的pid的参数
#define AUTO_PITCH_DM_MOT_KP 30.0f
#define AUTO_PITCH_DM_MOT_KI 0.0f
#define AUTO_PITCH_DM_MOT_KD 2.0f

//手动模式下------------------------------------------

//yaw轴速度环pid参数
#define YAW_SPEED_PID_KP 15.0f
#define YAW_SPEED_PID_KI 0.0f
#define YAW_SPEED_PID_KD 0.0f

//yaw轴位置环pid参数
#define YAW_POS_PID_KP 27.0f
#define YAW_POS_PID_KI 0.0f
#define YAW_POS_PID_KD 0.3f

//pitch轴达妙mit控制阻抗系数
#define DM_MOT_PITCH_KP 20.0f
#define DM_MOT_PITCHZ_KI 0.0f
#define DM_MOT_PITCH_KD 0.7f

//pitch轴达妙mit控制的重力补偿的pid的参数
#define PITCH_DM_MOT_KP 30.0f
#define PITCH_DM_MOT_KI 0.0f
#define PITCH_DM_MOT_KD 2.0f

//--------------------------------------------------------

//拨弹盘单发情况速度环pid参数
#define TRIGGER_SINGLE_SPEED_PID_KP 0.23f
#define TRIGGER_SINGLE_SPEED_PID_KI 0.0f
#define TRIGGER_SINGLE_SPEED_PID_KD 0.0005f

//拨弹盘单发情况位置环pid参数
#define TRIGGER_SINGLE_POS_PID_KP 2300.0f
#define TRIGGER_SINGLE_POS_PID_KI 0.0f
#define TRIGGER_SINGLE_POS_PID_KD 0.0f

//拨弹盘连发情况速度环pid参数
#define TRIGGER_BURST_SPEED_PID_KP 0.13f
#define TRIGGER_BURST_SPEED_PID_KI 0.0f
#define TRIGGER_BURST_SPEED_PID_KD 0.0000f

//pitch轴物理限幅参数
#define PITCH_LIMIT_MAX -0.48f
#define PITCH_LIMIT_MIN 0.82f

//yaw轴初始偏移角
#define _YAW_OFFSET 2600

//摩擦轮速度环pid参数
#define FRIC_SPEED_PID_KP 0.3f
#define FRIC_SPEED_PID_KI 0.0f
#define FRIC_SPEED_PID_KD 0.00002f

//弹速修正系数
#define BULLET_SPEED_COMP_KP 0.81f

// 发射速度 (发/秒)
constexpr float SHOOT_SPEED                                       = 10.0f;

#define TRIGGER_MOTOR_ID pyro::dji_motor_tx_frame_t::id_3





#endif








}

// ========================================
// 2. 输入与交互参数
// ========================================
namespace Input {
constexpr float JOYSTICK_DEADZONE = 0.02f; // 摇杆 2% 死区，防漂移

// DR16 拨杆状态阈值判断
constexpr float SW_DOWN_THRESHOLD =  0.25f; // 拨杆在下 (安全/急停)
constexpr float SW_UP_THRESHOLD   = -0.50f; // 拨杆在上 (视觉/自动)
// 介于两者之间则为拨杆在中 (遥控器模式)

constexpr float Y_SENSITIVITY = 100.0f;
constexpr float X_SENSITIVITY = 100.0f;
}

namespace Imu {
// --- IMU 校准与恒温控制配置 ---
// 设置为 1 开启 60s 静置校准模式，设置为 0 正常运行
// --- IMU 校准与恒温控制配置 ---
#define IMU_CALIBRATION_MODE 0

constexpr float TARGET_TEMPERATURE = 50.0f;
constexpr float TEMP_DEADBAND = 0.15f;

// 【核心修改】：将功率上限从 30% 暴降至 8%！
// 既然 30% 功率能导致 0.8℃/s 的升温，8% 的功率将升温率压制在 0.2℃/s 左右。
// 这样在 1.28 秒的盲区内，温度最多只会上漂 0.25℃，彻底抹杀大幅超调的物理基础。
constexpr float MAX_HEATER_POWER_RATIO = 0.08f;

constexpr float CALIB_TEMP_TOLERANCE = 0.5f;
// 软启动步长保持不变
constexpr float PWM_RAMP_STEP_RATIO    = 0.002f;

// 温度熔断阈值
constexpr float TEMP_MIN_SAFE          = -10.0f;
constexpr float TEMP_MAX_SAFE          = 55.0f; // 目标温度提高了，安全阈值相应放宽

#ifdef DOG_2
// 正常模式下使用的零偏补偿值 (这些值应通过开启 IMU_CALIBRATION_MODE 测量后填入)
constexpr float GYRO_BIAS_X = -0.00630061096f;
constexpr float GYRO_BIAS_Y = -0.000766833022F;
constexpr float GYRO_BIAS_Z = 0.000817186432f;

constexpr float ACCEL_BIAS_X = 0.0f;
constexpr float ACCEL_BIAS_Y = 0.0f;
constexpr float ACCEL_BIAS_Z = 0.0f;

#endif

#ifdef DOG_1
// 正常模式下使用的零偏补偿值 (这些值应通过开启 IMU_CALIBRATION_MODE 测量后填入)
constexpr float GYRO_BIAS_X = -0.00370389153f;
constexpr float GYRO_BIAS_Y = -0.0047872453f;
constexpr float GYRO_BIAS_Z = 0.00183101289f;

constexpr float ACCEL_BIAS_X = 0.0f;
constexpr float ACCEL_BIAS_Y = 0.0f;
constexpr float ACCEL_BIAS_Z = 0.0f;

#endif

#ifdef STEER
// 正常模式下使用的零偏补偿值 (这些值应通过开启 IMU_CALIBRATION_MODE 测量后填入)
constexpr float GYRO_BIAS_X = 0.000321589207f;
constexpr float GYRO_BIAS_Y = -0.000884396781f;
constexpr float GYRO_BIAS_Z = 0.000591511256f;

constexpr float ACCEL_BIAS_X = 0.0f;
constexpr float ACCEL_BIAS_Y = 0.0f;
constexpr float ACCEL_BIAS_Z = 0.0f;

#endif

}
}


/*-------- 3. interface ----------------------------------------------------------------------------------------------*/




/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_ALGO_CONFIG_H*/
