/**
 *******************************************************************************
 * @file    data-def.h
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


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_CHASSIS_DATA_DEF_H
#define INFANTRY_CHASSIS_DATA_DEF_H



/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/


#include "Component/Motor/pyro_dji_motor_drv.h"


/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/

// ==========================================
// 1. 意图区数据 (外界输入)
// ==========================================

// 原始遥控器数据 (独立于具体协议)
struct RcRawData {
    float right_x, right_y; // 右摇杆 (-1.0 ~ 1.0)
    float left_x,  left_y;  // 左摇杆 (-1.0 ~ 1.0)
    float dial;             // 拨轮   (-1.0 ~ 1.0)
    uint8_t sw_left;        // 左开关 (1:上, 2:中, 3:下)
    uint8_t sw_right;       // 右开关 (1:上, 2:中, 3:下)
    uint32_t timestamp;
};

enum class ControlSource : uint8_t {
    SAFE_STOP = 0, // 急停/无力 (最高优先级)
    REMOTE    = 1, // 遥控器主导
    VISION    = 2, // 视觉主导
    KEYMOUSE  = 3  // 键鼠主导
};

// ==========================================
// [新增] 模式枚举定义
// ==========================================
enum ChassisMode : uint8_t {
    CHASSIS_RELAX = 0,       // 无力/急停模式
    CHASSIS_RC    = 1,       // 遥控器手动控制模式 (速度环)
    CHASSIS_AUTO  = 2,       // 自动/视觉/状态保留模式
};

enum GimbalMode : uint8_t {
    GIMBAL_RELAX = 0,        // 无力模式
    GIMBAL_RC    = 1,        // 遥控器控制模式 (速度环)
    GIMBAL_AUTO  = 2,        // 视觉自瞄/绝对角度模式 (位置-速度串级环)
};

// 底盘与云台的宏观期望指令
struct ChassisCmd {
    float vx, vy, vw;       // 期望速度 (m/s, rad/s)
    uint8_t mode;           // 模式控制
    uint32_t timestamp;
};

struct GimbalCmd {
    float yawRad, pitchRad; // 期望绝对角度 (rad)
    uint8_t mode;
    uint32_t timestamp;
};

// ==========================================
// 2. 状态区数据 (层级物理反馈)
// ==========================================

struct ImuState {
    float accel[3], gyro[3];
    float roll, pitch, yaw;
    uint32_t timestamp;
};

struct MotorState {
    float pos;        // 角度 (rad)
    float vel;        // 角速度 (rad/s)
    float torque;     // 真实反馈力矩 (N.m) 或 电流 (A)
    int8_t temp;    // 温度 (°C)
};

// 舵轮模块组合状态
struct SwerveModuleState {
    MotorState drive; // 动力轮
    MotorState steer; // 航向舵
};

struct ChassisState {
    SwerveModuleState modules[4]; // 4个舵轮
    uint32_t timestamp;
};

struct GimbalState {
    MotorState yaw;
    MotorState pitch;
    uint32_t timestamp;
};

// ==========================================
// 3. 输出区数据 (执行器控制量)
// ==========================================

struct ChassisOutput {
    float driveCurrent[4]; // 4个动力轮目标电流 (A)
    float steerCurrent[4]; // 4个航向舵目标电流 (A)
};

struct GimbalOutput {
    float yaw_current;
    float pitch_current;
};

// ==========================================
// 4. 遥测/中间区数据 (控制过程可视化)
// ==========================================
// 这部分极其关键，记录了从 "Cmd" 到 "Output" 之间的数学变换细节

struct ChassisTelemetry {
    // 运动学正解 (Forward Kinematics)：根据轮速推算出的底盘真实质心速度
    float real_vx, real_vy, real_vw;

    // 运动学逆解 (Inverse Kinematics)：算出的 4个轮子预期打角和转速
    float targetSteerAngle[4];
    float targetSteerVelocity[4];
    float targetDriveSpd[4];

    // 底盘功率观测器
    float estimated_power_w;     // 预估底盘总消耗功率 (Watt)

    uint32_t timestamp;
};


/*-------- 3. interface ----------------------------------------------------------------------------------------------*/




/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_CHASSIS_DATA_DEF_H*/
