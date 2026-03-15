/**
 *******************************************************************************
 * @file    movtion-ctrl-app.cpp
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

#include "movtion-ctrl-app.h"

#include "../System/DataHub/blackboard.h"
#include "Config/Gimbal/algo-config.h"
#include "System/DataHub/referee-data-hub.h"
#include "dsp/fast_math_functions.h"
#include "pyro_dwt_drv.h"
/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = MovtionCtrlApp::instance();



/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "MovtionCtrl"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/





/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/

MovtionCtrlApp::MovtionCtrlApp()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 1) {}

// =================================================================================
// 实用工具函数：高效角度规整到 [-PI, PI]
// =================================================================================
static inline float wrapAngle(float angle) {
    // 使用 fmodf 替代 while 循环，防止极端情况下循环超时导致任务卡死
    angle = std::fmod(angle + M_PI, 2.0f * M_PI);
    if (angle < 0) {
        angle += 2.0f * M_PI;
    }
    return angle - M_PI;
}

#ifdef CHASSIS
// =================================================================================
// 底盘控制逻辑
// =================================================================================
void MovtionCtrlApp::init() {
    for (uint8_t i = 0; i < 4; i++) {
        motorIdx[i] = Config::Hardware::MotorTopo::DRIVE_MOTOR_IDS[i] - 1;
    }
}

void MovtionCtrlApp::run() {
    static ChassisCmd cmd;
    static ChassisState state;
    static RMRobotStatus refState; // [新增]
    static RMPowerHeatData powerHeatState;
    Blackboard::instance().chassisCmd.read(cmd);
    Blackboard::instance().chassisState.read(state);
    RefereeDataHub::instance().robotStatus.read(refState); // [新增]
    RefereeDataHub::instance().powerHeat.read(powerHeatState);


    ChassisOutput output   = {};
    ChassisTelemetry telem = {};

    // 1. 获取准确的 dt 周期 (S曲线积分和防震荡强依赖 dt)
    static uint32_t dwtCnt;
    float dt = pyro::dwt_drv_t::get_delta_t(&dwtCnt);

    if (cmd.mode == CHASSIS_RELAX) {
        // [新增] 模式切换时清空内部状态，防止切回正常模式时暴走
        vxPlanner.reset();
        vyPlanner.reset();
        vwPlanner.reset();
        Blackboard::instance().chassisOut.write(output);
        return;
    }



    // --- 1. 坐标转换与宏观仲裁 (与原代码一致) ---
    float theta                 = -state.yaw.pos;
    float cosTheta              = std::cos(theta);
    float sinTheta              = std::sin(theta);
    float rawChassisVx          = cmd.vx * cosTheta - cmd.vy * sinTheta;
    float rawChassisVy          = cmd.vx * sinTheta + cmd.vy * cosTheta;
    float rawChassisVw          = (cmd.mode == CHASSIS_NORMAL) ? yawPosPid.calculate(0.0f, state.yaw.pos) : cmd.vw;


    // =========================================================
    // 2. 【核心神技】：S型速度曲线规划 (Jerk 限制)
    // =========================================================
    // 通过 S 曲线，过滤掉无限大的加速度和 Jerk，生成完全符合物理底线的平滑速度
    float chassisVx             = vxPlanner.calculate(rawChassisVx, dt);
    float chassisVy             = vyPlanner.calculate(rawChassisVy, dt);
    // float chassisVw = vwPlanner.calculate(rawChassisVw, dt);
    float chassisVw             = rawChassisVw;

    // --- 2. 运动学解算 (仅算出目标速度，不跑PID) ---
    float halfL                 = Config::Hardware::Chassis::WHEEL_BASE / 2.0f;
    float halfW                 = Config::Hardware::Chassis::TRACK_WIDTH / 2.0f;
    float vyFront               = chassisVy + chassisVw * halfL;
    float vyRear                = chassisVy - chassisVw * halfL;
    float vxLeft                = chassisVx - chassisVw * halfW;
    float vxRight               = chassisVx + chassisVw * halfW;

    float targetVx[4]           = {vxRight, vxLeft, vxLeft, vxRight};
    float targetVy[4]           = {vyFront, vyFront, vyRear, vyRear};

    // 存储中间状态数组
    float idealDriveSpd[4]      = {0};
    float realDriveVel[4]       = {0};
    float filteredTorque[4]     = {0}; // 低通滤波后的负载电流

    static float s_lpfTorque[4] = {0}; // 静态滤波器记忆

    for (int i = 0; i < 4; i++) {
        float tgtSpeed  = std::hypot(targetVx[i], targetVy[i]);
        float tgtAngle  = 0.0f;
        uint8_t id      = motorIdx[i];
        float realAngle = state.modules[id].steer.pos;

        if (tgtSpeed < 0.05f) {
            tgtAngle = realAngle;
            tgtSpeed = 0.0f;
        } else {
            tgtAngle = std::atan2(targetVy[i], targetVx[i]);
        }

        if (i == 0 || i == 3)
            tgtSpeed = -tgtSpeed;
        tgtSpeed *= (Config::Hardware::Chassis::DRIVE_GEAR_RATIO / Config::Hardware::Chassis::WHEEL_RADIUS);

        float errAngle = wrapAngle(tgtAngle - realAngle);
        if (errAngle > M_PI_2) {
            errAngle -= M_PI;
            tgtSpeed = -tgtSpeed;
        } else if (errAngle < -M_PI_2) {
            errAngle += M_PI;
            tgtSpeed = -tgtSpeed;
        }

        // 航向舵逻辑照常运行
        float finalTgtAngle     = realAngle + errAngle;
        float tgtSteerSpd       = steerPosPid[id].calculate(finalTgtAngle, realAngle);
        output.steerVoltage[id] = steerSpdPid[id].calculate(tgtSteerSpd, state.modules[id].steer.vel);

        // [提取] 动力轮参数供功率模块使用
        idealDriveSpd[i]        = tgtSpeed;
        realDriveVel[i]         = state.modules[id].drive.vel;

        // [新增] 电流极简一阶低通滤波 (Alpha=0.2)，滤除高频尖刺
        s_lpfTorque[i]          = 0.8f * s_lpfTorque[i] + 0.2f * state.modules[id].drive.torque;
        filteredTorque[i]       = s_lpfTorque[i];
    }

    // =========================================================
    // 3. 第一层防御：宏观速度诱导 (MPVS)
    // =========================================================


    float kv           = 1.0f;

    float dynamicLimit = PowerLimiter::getDynamicPowerLimit(refState.chassisPowerLimit, powerHeatState.bufferEnergy);
    kv                 = PowerLimiter::instance().calculateVelocityScale(idealDriveSpd, filteredTorque, dynamicLimit);


    float rawOutputCurrent[4] = {};

    // =========================================================
    // 4. 应用速度缩放与 PID 计算
    // =========================================================
    for (int i = 0; i < 4; i++) {
        uint8_t id           = motorIdx[i];
        float scaledDriveSpd = idealDriveSpd[i] * kv; // 等比例缩小目标速度，保底盘不偏航！

        // 【抗积分饱和】如果在严重压制状态，清空 PID，防止暴冲 (调用 PYRo 的 clear() 方法)
        if (kv < 0.1f) {
            driveSpdPid[id].clear();
        }

        rawOutputCurrent[i] = driveSpdPid[id].calculate(scaledDriveSpd, realDriveVel[i]);
    }

    // =========================================================
    // 5. 第二层防御：微观硬件电流钳位 (绝对零延时)
    // =========================================================
    float ki = 1.0f;
    ki       = PowerLimiter::instance().calculateCurrentScale(rawOutputCurrent, realDriveVel, dynamicLimit);

    for (int i = 0; i < 4; i++) {
        uint8_t id              = motorIdx[i];

        // 最终暴力限流，强行保证绝对不掉血
        output.driveCurrent[id] = rawOutputCurrent[i] * ki;

        // 如果触发了底层切断，代表执行层未达预期，也要清空积分
        if (ki < 0.99f) {
            driveSpdPid[id].clear();
        }

        // 装填遥测
        telem.targetDriveSpd[i] = idealDriveSpd[i] * kv;
    }

    Blackboard::instance().chassisOut.write(output);
    Blackboard::instance().chassisTelem.write(telem);
}

#elif defined(GIMBAL) // 标准预编译宏

// =================================================================================
// 云台控制逻辑
// =================================================================================
void MovtionCtrlApp::init() { /* driver object initialize */ }

void MovtionCtrlApp::run() {
    GimbalCmd cmd{};
    ImuState imuState{};
    GimbalState gimbalState{};
    GimbalTelemetry telem{};
    GimbalOutput output{};


    static uint32_t dwtCnt;
    float dt = pyro::dwt_drv_t::get_delta_t(&dwtCnt);


    Blackboard::instance().gimbalCmd.read(cmd);
    Blackboard::instance().imuState.read(imuState);
    Blackboard::instance().gimbalState.read(gimbalState);
    Blackboard::instance().gimbalTelem.read(telem);



    // 1. 无力模式判断
    if (cmd.mode == GIMBAL_RELAX) {
        telem.targetYawRad    = imuState.yaw;
        telem.targetPitchRad  = imuState.pitch;
        output.targetPitchPos = gimbalState.pitch.pos;
        output.pitchEn        = false;

        Blackboard::instance().gimbalTelem.write(telem);
        Blackboard::instance().gimbalOut.write(output);
        yawPosPid.clear();

        yawSpdPid.clear();
        pitchPosPid.clear();
        return;
    }
Blackboard::instance().gimbalOut.read(output);

    // 前馈分量初始化
    float ffYawTorque = 0.0f;

    // 2. 模式处理与期望值计算
    if (cmd.mode == GIMBAL_AUTO && abs(cmd.targetYaw) < M_PI) {
        telem.targetYawRad   = cmd.targetYaw;
        telem.targetPitchRad = cmd.targetPitch;

        // 加速度转化为前馈力矩 (需在 config.h 中标定转动惯量系数 INERTIA_K)
        ffYawTorque   = Config::Algorithm::Gimbal::YAW_INERTIA_K * cmd.targetYawSpeed;

    } else {
        // 手动模式：遥控器输入的是速度
        telem.targetYawRad = wrapAngle(telem.targetYawRad + cmd.yawVel * dt);
        telem.targetPitchRad += cmd.pitchVel * dt;
        // 自瞄数据无效的时候使用遥控器的数据
        ffYawTorque   = Config::Algorithm::Gimbal::YAW_INERTIA_K * cmd.yawVel;

        // 手动模式下也可以利用遥控器指令做简单的速度前馈
    }

    // =================================================================
    // 3. Pitch 轴解算 (针对类似 MIT 模式或内置位置环的电机)
    // =================================================================
    float offsetPitch    = imuState.pitch - gimbalState.pitch.pos;
    float targetMotorRaw = telem.targetPitchRad - offsetPitch;

    // 物理限位裁切
    if (targetMotorRaw > Config::Algorithm::Gimbal::PITCH_DEPRESSION_LIMIT) {
        targetMotorRaw = Config::Algorithm::Gimbal::PITCH_DEPRESSION_LIMIT;
    } else if (targetMotorRaw < Config::Algorithm::Gimbal::PITCH_ELEVATION_LIMIT) {
        targetMotorRaw = Config::Algorithm::Gimbal::PITCH_ELEVATION_LIMIT;
    }

    telem.targetPitchRad = targetMotorRaw + offsetPitch;

    // 重力补偿前馈 + PID 力矩 + 视觉动态加速度前馈
    static float kGravity = 0.0f;
    //float gravityFf           = Config::Algorithm::Gimbal::PITCH_K_GRAVITY * std::cos(imuState.pitch);
    float gravityFf = kGravity * arm_cos_f32(imuState.pitch);
    float pitchIntegralTorque = pitchPosPid.calculate(telem.targetPitchRad, imuState.pitch);
    float totalFf             = gravityFf + pitchIntegralTorque; // 【融合前馈力矩】

    output.targetPitchPos         = targetMotorRaw;
    output.pitchFeedforwardTorque = totalFf;
    output.pitchEn                = true;

    // 如果你的底层 Pitch 电机支持传入速度期望(如 MIT 模式的 v_des)，可以在此传入 cmd.targetPitchSpeed

    // =================================================================
    // 4. Yaw 轴解算 (标准双环串级 PID)
    // =================================================================
    float alignedTgtYaw = imuState.yaw + wrapAngle(telem.targetYawRad - imuState.yaw);

    // 位置环计算 (反馈分量)
    float yawPosOut = yawPosPid.calculate(alignedTgtYaw, imuState.yaw);

    // 【复合速度】：位置环修正输出 + 视觉预测速度前馈
    float tgtYawSpd = yawPosOut;
    telem.targetYawRotate = tgtYawSpd;

    // 速度环计算 (反馈分量)
    float yawSpdOut = yawSpdPid.calculate(tgtYawSpd, imuState.gyro[2]);

    // 【复合力矩】：速度环修正输出 + 视觉预测加速度前馈
    // static float test_ffk_yaw;
    // static float test_yaw_angle;
    // static float test_yaw_speed;
    // float t = pyro::dwt_drv_t::get_timeline_s();
    // test_yaw_angle = M_PI_4 * arm_sin_f32(2 * M_PI * t);
    // test_yaw_speed = M_PI * M_PI_2 * arm_cos_f32(2 * M_PI * t);
    output.yawVoltage = yawSpdOut + ffYawTorque;


    // 数据回写
    Blackboard::instance().gimbalOut.write(output);
    Blackboard::instance().gimbalTelem.write(telem);
}
#endif