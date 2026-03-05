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
    Blackboard::instance().chassisCmd.read(cmd);
    Blackboard::instance().chassisState.read(state);

    ChassisOutput output   = {};
    ChassisTelemetry telem = {};

    if (cmd.mode == CHASSIS_RELAX) {
        Blackboard::instance().chassisOut.write(output);
        return;
    }

    // ---------------------------------------------------------
    // 1. 坐标系转换 (云台系 -> 底盘系)
    // ---------------------------------------------------------
    // 假设云台 yaw 向左偏为正。这里的 cmd.vx 是云台正前方，cmd.vy 是云台正左方。
    float theta = -state.yaw.pos;
    float cosTheta = std::cos(theta);
    float sinTheta = std::sin(theta);

    // 2D 旋转矩阵，将云台系速度投影到底盘系
    float chassisVx = cmd.vx * cosTheta - cmd.vy * sinTheta;
    float chassisVy = cmd.vx * sinTheta + cmd.vy * cosTheta;
    float chassisVw = 0.0f;

    // ---------------------------------------------------------
    // 2. 底盘跟随与旋转逻辑仲裁
    // ---------------------------------------------------------
    if (cmd.mode == CHASSIS_NORMAL) {
        // 底盘跟随云台模式：目标夹角为 0，反馈当前夹角
        chassisVw = yawPosPid.calculate(0.0f, state.yaw.pos);
    } else if (cmd.mode == CHASSIS_SPIN) {
        // 小陀螺模式：直接使用独立下发的旋转速度
        chassisVw = cmd.vw;

    }

    // ---------------------------------------------------------
    // 3. 舵轮运动学逆解计算
    // ---------------------------------------------------------
    float halfL = Config::Hardware::Chassis::WHEEL_BASE / 2.0f;
    float halfW = Config::Hardware::Chassis::TRACK_WIDTH / 2.0f;

    // 计算底盘前后左右四个边缘的绝对速度分量
    float vyFront = chassisVy + chassisVw * halfL;
    float vyRear  = chassisVy - chassisVw * halfL;
    float vxLeft  = chassisVx - chassisVw * halfW;
    float vxRight = chassisVx + chassisVw * halfW;

    // 严格映射：RF=0, LF=1, LB=2, RB=3
    float targetVx[4] = {vxRight, vxLeft, vxLeft, vxRight};
    float targetVy[4] = {vyFront, vyFront, vyRear, vyRear};

    // ---------------------------------------------------------
    // 4. 计算每个模块的期望转速与期望打角
    // ---------------------------------------------------------
    for (int i = 0; i < 4; i++) {
        uint8_t id = motorIdx[i]; // 获取对应的物理映射ID


        float tgtSpeed = std::hypot(targetVx[i], targetVy[i]); // 使用 std::hypot 避免溢出且性能更好
        float tgtAngle = 0.0f;


        float realAngle = state.modules[id].steer.pos;

        // 【防抽搐保护】如果目标速度极小，保持当前角度不变，防止轮子回正
        if (tgtSpeed < 0.05f) {
            tgtAngle = realAngle;
            tgtSpeed = 0.0f; // 彻底切断微小抖动
        } else {
            tgtAngle = std::atan2(targetVy[i], targetVx[i]);
        }

        if (i == 0 || i == 3) {
            tgtSpeed = -tgtSpeed; // 处理电机对称反向安装
        }
        tgtSpeed *= (Config::Hardware::Chassis::DRIVE_GEAR_RATIO / Config::Hardware::Chassis::WHEEL_RADIUS);

        // 就近优选算法 (Angle Optimization)
        float errAngle = wrapAngle(tgtAngle - realAngle);

        if (errAngle > M_PI_2) {
            errAngle -= M_PI;
            tgtSpeed = -tgtSpeed;
        } else if (errAngle < -M_PI_2) {
            errAngle += M_PI;
            tgtSpeed = -tgtSpeed;
        }

        float finalTgtAngle = realAngle + errAngle;

        // 遥测与 PID 赋值
        telem.targetSteerAngle[i] = finalTgtAngle;
        telem.targetDriveSpd[i]   = tgtSpeed;



        float tgtSteerSpd = steerPosPid[id].calculate(finalTgtAngle, state.modules[id].steer.pos);
        telem.targetSteerVelocity[i] = tgtSteerSpd;

        output.steerVoltage[id] = steerSpdPid[id].calculate(tgtSteerSpd, state.modules[id].steer.vel);
        output.driveCurrent[id] = driveSpdPid[id].calculate(tgtSpeed, state.modules[id].drive.vel);
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
    GimbalCmd cmd;
    ImuState imuState;
    GimbalState gimbalState;
    GimbalTelemetry telem;
    static uint32_t dwtCnt;
    float dt = pyro::dwt_drv_t::get_delta_t(&dwtCnt);

    Blackboard::instance().gimbalCmd.read(cmd);
    Blackboard::instance().imuState.read(imuState);
    Blackboard::instance().gimbalState.read(gimbalState);
    Blackboard::instance().telem.read(telem);

    GimbalOutput output = {0};

    // 1. 无力模式判断
    if (cmd.mode == GIMBAL_RELAX) {
        telem.targetYawRad    = imuState.yaw;
        telem.targetPitchRad  = imuState.pitch;
        output.targetPitchPos = gimbalState.pitch.pos;
        output.pitchEn        = false;

        Blackboard::instance().telem.write(telem);
        Blackboard::instance().gimbalOut.write(output);
        return;
    }

    Blackboard::instance().gimbalOut.read(output);

    // ---------------------------------------------------------
    // 2. Yaw 轴串级 PID 计算
    // ---------------------------------------------------------
    telem.targetYawRad = wrapAngle(telem.targetYawRad + cmd.yawVel * dt);

    // 使用新的规整函数，一行代码解决
    float alignedTgtYaw = imuState.yaw + wrapAngle(telem.targetYawRad - imuState.yaw);

    float tgtYawSpd = yawPosPid.calculate(alignedTgtYaw, imuState.yaw);
    telem.targetYawRotate = tgtYawSpd;
    output.yawVoltage     = yawSpdPid.calculate(tgtYawSpd, imuState.gyro[2]);

    // ---------------------------------------------------------
    // 3. Pitch 轴前馈与限幅控制 (MIT模式)
    // ---------------------------------------------------------
    float offsetPitch = imuState.pitch - gimbalState.pitch.pos;

    // 目标规划
    telem.targetPitchRad += cmd.pitchVel * dt;
    float targetMotorRaw = telem.targetPitchRad - offsetPitch;

    // 物理限位裁切
    if (targetMotorRaw > Config::Algorithm::Gimbal::PITCH_DEPRESSION_LIMIT) {
        targetMotorRaw = Config::Algorithm::Gimbal::PITCH_DEPRESSION_LIMIT;
    } else if (targetMotorRaw < Config::Algorithm::Gimbal::PITCH_ELEVATION_LIMIT) {
        targetMotorRaw = Config::Algorithm::Gimbal::PITCH_ELEVATION_LIMIT;
    }

    // 状态反写回，防止积分风暴和卡限位
    telem.targetPitchRad = targetMotorRaw + offsetPitch;

    // 前馈力矩计算
    float gravityFf = Config::Algorithm::Gimbal::PITCH_K_GRAVITY * std::cos(imuState.pitch);
    float pitchIntegralTorque = pitchPosPid.calculate(telem.targetPitchRad, imuState.pitch);
    float totalFf = gravityFf + pitchIntegralTorque;

    // 参数装填下发
    output.targetPitchPos         = targetMotorRaw;
    output.pitchFeedforwardTorque = totalFf;
    output.pitchEn                = true;

    Blackboard::instance().gimbalOut.write(output);
    Blackboard::instance().telem.write(telem);
}
#endif