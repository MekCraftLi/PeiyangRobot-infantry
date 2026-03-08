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
constexpr float MAX_VX = 3.0f; // 前后最大平移速度 (m/s)
constexpr float MAX_VY = 3.0f; // 左右最大平移速度 (m/s)
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

constexpr float DM_MOTOR_KP = 50.0f;
constexpr float DM_MOTOR_KI = 1.0f;
constexpr float DM_MOTOR_KD = 1.5f;

}

namespace Gimbal {
// 遥控器推满时，云台的最大旋转速度 (rad/s)
constexpr float MAX_YAW_SPEED   = 6.28f; // 约 180度/秒
constexpr float MAX_PITCH_SPEED = 6.28f;  // 约 114度/秒
constexpr float YAW_INERTIA_K = 2.3f;
// 云台 Pitch 轴物理限幅 (防止撞击底盘/弹仓)
constexpr float PITCH_ELEVATION_LIMIT =  -0.332f; // 抬头上限 (rad)
constexpr float PITCH_DEPRESSION_LIMIT = 0.821f; // 低头下限 (rad)

constexpr float PITCH_K_GRAVITY = 0.0f;

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
}
}


/*-------- 3. interface ----------------------------------------------------------------------------------------------*/




/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_ALGO_CONFIG_H*/
