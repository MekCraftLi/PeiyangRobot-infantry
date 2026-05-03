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
#include "System/Service/commander.h"
#include "System/Service/motor-actuator.h"
#include "System/DataHub/referee-data-hub.h"
#include "dsp/fast_math_functions.h"
#include "pyro_dwt_drv.h"
/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = MovtionCtrlApp::instance();

// --- 对准目标: 6020 编码器 0x5400 对应的角度 (rad) ---
static constexpr float ALIGN_TARGET_RAD = (5120.0f / 8192.0f) * 2.0f * M_PI;
static constexpr float ALIGN_TOLERANCE  = 5.0f * M_PI / 180.0f;

static pyro::fsm_t<GimbalMotionCtx> motionFsm;
static GimbalMotionCtx motionCtx;

// 专供 Ozone 示波器实时采样的底盘功率观测探针
volatile struct PowerDebugOzone {
    float ref_power_limit;   // 裁判系统：当前功率上限 (W)
    float ref_real_power;    // 裁判系统：底盘实际消耗功率 (W)
    float ref_buffer_energy; // 裁判系统：剩余缓冲能量 (J)

    float cap_voltage; // 超级电容：当前电压 (V)
    float cap_power;   // 超级电容：当前输出功率 (W)

    float pre_limit_torque;  // 算法：限幅前，底盘四大电机期望扭矩/电流绝对值之和
    float post_limit_torque; // 算法：限幅后，实际下发的总扭矩/电流
    float scale_factor;      // 算法：功率控制器算出的缩放系数 (通常在 0.0 ~ 1.0 之间)
} g_power_debug;

// 专供 Ozone 示波器实时采样的 S曲线观测探针
volatile struct SCurveDebugOzone {
    float target_vx;   // 遥控器输入的原始阶跃速度期望 (m/s)
    float smooth_vx;   // S曲线规划器输出的平滑速度 (m/s)
    float current_ax;  // S曲线规划器当前计算出的实际加速度 (m/s^2)

    // 如果你想同时看 Vy，可以继续加
    // float target_vy;
    // float smooth_vy;
} g_scurve_debug;


/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "MovtionCtrl"

#define APPLICATION_STACK_SIZE 1024

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/





/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/

MovtionCtrlApp::MovtionCtrlApp()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 1) {}

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
    static SuperCapState capState;

    Blackboard::instance().chassisCmd.read(cmd);
    Blackboard::instance().chassisState.read(state);
    Blackboard::instance().capState.read(capState);
    RefereeDataHub::instance().robotStatus.read(refState); // [新增]
    RefereeDataHub::instance().powerHeat.read(powerHeatState);


    ChassisOutput output   = {};
    ChassisTelemetry telem = {};

    // 1. 获取准确的 dt 周期 (S曲线积分和防震荡强依赖 dt)
    static uint32_t dwtCnt;
    float dt = pyro::dwt_drv_t::get_delta_t(&dwtCnt);

    if ((cmd.mode & 0x03) == CHASSIS_RELAX) {
        // [新增] 模式切换时清空内部状态，防止切回正常模式时暴走
        vxPlanner.reset();
        vyPlanner.reset();
        vwPlanner.reset();
        Blackboard::instance().chassisOut.write(output);
        return;
    }



    // =========================================================
    // 1. 【核心修复】：S型速度曲线规划必须作用于云台/世界坐标系
    // =========================================================
    // 过滤掉操作手摇杆的无限大加速度和 Jerk，生成平滑的云台系期望速度
    float smoothCmdVx  = vxPlanner.calculate(cmd.vx, dt);
    float smoothCmdVy  = vyPlanner.calculate(cmd.vy, dt);
    // 【新增埋点】：将 S 曲线的输入、输出和加速度送入探针
    g_scurve_debug.target_vx  = cmd.vx;                 // 遥控器的原始猛烈突变
    g_scurve_debug.smooth_vx  = smoothCmdVx;            // 也就是 vxPlanner.getCurrentV()
    g_scurve_debug.current_ax = vxPlanner.getCurrentA(); // 观察真实的物理加速度变化
    // 【强烈建议解锁】：给小陀螺的旋转也加上 S 曲线，防止开启/关闭小陀螺瞬间电机狂抽
    // float smoothCmdVw = vwPlanner.calculate(cmd.vw, dt);

    // --- 2. 坐标转换 (将平滑后的云台期望速度纯数学投影到底盘系) ---
    float theta        = -state.yaw.pos;
    float cosTheta     = std::cos(theta);
    float sinTheta     = std::sin(theta);

    // 纯数学映射，这之后绝对不能再加任何改变向量方向和比例的滤波器！
    float chassisVx    = smoothCmdVx * cosTheta - smoothCmdVy * sinTheta;
    float chassisVy    = smoothCmdVx * sinTheta + smoothCmdVy * cosTheta;

    float rawChassisVw = ((cmd.mode & 0x03) == CHASSIS_NORMAL) ? yawPosPid.calculate(0.0f, state.yaw.pos) : cmd.vw;
    float chassisVw    = rawChassisVw; // 如果上面启用了 smoothCmdVw，这里替换为对应变量


    // 2. 赋值裁判系统与物理状态
    g_power_debug.ref_power_limit   = refState.chassisPowerLimit;
    g_power_debug.ref_real_power    = capState.chassisPower;
    g_power_debug.ref_buffer_energy = powerHeatState.bufferEnergy;
    g_power_debug.cap_voltage       = capState.voltage;
    g_power_debug.cap_power         = capState.capPower;


    // --- 2. 运动学解算 (仅算出目标速度，不跑PID) ---
    float halfL                     = Config::Hardware::Chassis::WHEEL_BASE / 2.0f;
    float halfW                     = Config::Hardware::Chassis::TRACK_WIDTH / 2.0f;
    float vyFront                   = chassisVy + chassisVw * halfL;
    float vyRear                    = chassisVy - chassisVw * halfL;
    float vxLeft                    = chassisVx - chassisVw * halfW;
    float vxRight                   = chassisVx + chassisVw * halfW;

    float targetVx[4]               = {vxRight, vxLeft, vxLeft, vxRight};
    float targetVy[4]               = {vyFront, vyFront, vyRear, vyRear};

    // 存储中间状态数组
    float idealDriveSpd[4]          = {0};
    float realDriveVel[4]           = {0};
    float filteredTorque[4]         = {0}; // 低通滤波后的负载电流

    static float s_lpfTorque[4]     = {0}; // 静态滤波器记忆

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


    float kv            = 1.0f;

    uint16_t powerLimit = refState.chassisPowerLimit * 0.5f;

    if (cmd.mode & 0x04) {
        powerLimit += 40;
    }

    float dynamicLimit = PowerLimiter::getDynamicPowerLimit(powerLimit, powerHeatState.bufferEnergy);
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

MovtionCtrlApp::MotionState MovtionCtrlApp::getMotionState() {
    return MotionState::Manual; // CHASSIS 无 FSM, 始终视为 Manual
}

#elif defined(GIMBAL) // 标准预编译宏

// =================================================================================
// 云台控制逻辑
// =================================================================================
void MovtionCtrlApp::init() {
    MotActSrvc::instance().waitInit();
    motionFsm.change_state(&_stateRelax);
    motionFsm.enter(motionCtx);
}

void MovtionCtrlApp::run() {
    static uint32_t dwtCnt;
    motionCtx.dt = pyro::dwt_drv_t::get_delta_t(&dwtCnt);

    Blackboard::instance().gimbalCmd.read(motionCtx.cmd);
    Blackboard::instance().imuState.read(motionCtx.imu);
    Blackboard::instance().gimbalState.read(motionCtx.state);
    Blackboard::instance().gimbalTelem.read(motionCtx.telem);
    Blackboard::instance().gimbalOut.read(motionCtx.output);

    motionFsm.execute(motionCtx);



    // 数据回写
    Blackboard::instance().gimbalOut.write(motionCtx.output);
    Blackboard::instance().gimbalTelem.write(motionCtx.telem);
}

void MovtionCtrlApp::updatePitch(GimbalMotionCtx& ctx) {
    if (!ctx.state.pitch.online) {
        pitchPosPid.clear();
        ctx.output.targetPitchSpeed       = 0.0f;
        ctx.output.pitchFeedforwardTorque = 0.0f;
        ctx.output.pitchEn                = false;
        return;
    }

    if (ctx.cmd.mode == GIMBAL_AUTO && abs(ctx.cmd.targetYaw) < M_PI)
        ctx.telem.targetPitchRad = ctx.cmd.targetPitch;
    else
        ctx.telem.targetPitchRad += ctx.cmd.pitchVel * ctx.dt;

    float offsetPitch    = ctx.imu.pitch - ctx.state.pitch.pos;
    float targetMotorRaw = ctx.telem.targetPitchRad - offsetPitch;

    if (targetMotorRaw > Config::Algorithm::Gimbal::PITCH_DEPRESSION_LIMIT)
        targetMotorRaw = Config::Algorithm::Gimbal::PITCH_DEPRESSION_LIMIT;
    else if (targetMotorRaw < Config::Algorithm::Gimbal::PITCH_ELEVATION_LIMIT)
        targetMotorRaw = Config::Algorithm::Gimbal::PITCH_ELEVATION_LIMIT;

    ctx.telem.targetPitchRad = targetMotorRaw + offsetPitch;

    static float gravityK;
    float gravityFf           = gravityK * arm_cos_f32(ctx.imu.pitch);
    float pitchIntegralTorque = pitchPosPid.calculate(ctx.telem.targetPitchRad, ctx.imu.pitch);
    float totalFf             = gravityFf + pitchIntegralTorque;

    if (abs(ctx.cmd.pitchVel) > 0.1f)
        ctx.cmd.pitchVel = (ctx.cmd.pitchVel / ctx.cmd.pitchVel) * 0.1f;

    ctx.output.targetPitchPos         = targetMotorRaw;
    ctx.output.targetPitchSpeed       = ctx.cmd.pitchVel;
    ctx.output.pitchFeedforwardTorque = totalFf;
    ctx.output.pitchEn                = true;
}

void MovtionCtrlApp::updateYaw(GimbalMotionCtx& ctx) {
    if (!ctx.state.yaw.online) {
        ctx.telem.targetYawRad = ctx.imu.yaw;
        yawPosPid.clear();
        yawSpdPid.clear();
        ctx.output.yawVoltage = 0.0f;
        return;
    }

    float ffYawTorque = 0.0f;

    if (ctx.cmd.mode == GIMBAL_AUTO && abs(ctx.cmd.targetYaw) < M_PI) {
        ctx.telem.targetYawRad = ctx.cmd.targetYaw;
        ffYawTorque            = Config::Algorithm::Gimbal::YAW_INERTIA_K * ctx.cmd.targetYawSpeed;
    } else {
        ctx.telem.targetYawRad = wrapAngle(ctx.telem.targetYawRad + ctx.cmd.yawVel * ctx.dt);
        ffYawTorque            = 0.0f;
    }

    // 调头: X 键上升沿 → yaw 目标 +π
    if (CommanderSrvc::instance().actionReverseEdge.isTriggered()) {
        ctx.telem.targetYawRad = wrapAngle(ctx.telem.targetYawRad + M_PI);
    }

    ffYawTorque = Config::Algorithm::Gimbal::YAW_INERTIA_K * ctx.cmd.yawVel;
    ChassisToGimbalComm c2g{};
    Blackboard::instance().c2gComm.read(c2g);
    if (abs(c2g.msg.chassisYawSpeed) > 0.5f)
        ffYawTorque += -0.11f * c2g.msg.chassisYawSpeed;

    float alignedTgtYaw   = ctx.imu.yaw + wrapAngle(ctx.telem.targetYawRad - ctx.imu.yaw);
    float yawPosOut       = yawPosPid.calculate(alignedTgtYaw, ctx.imu.yaw);
    float tgtYawSpd       = yawPosOut;
    ctx.telem.targetYawRotate = tgtYawSpd;
    float yawSpdOut       = yawSpdPid.calculate(tgtYawSpd, ctx.imu.gyro[2]);
    ctx.output.yawVoltage = yawSpdOut + ffYawTorque;
}

MovtionCtrlApp::MotionState MovtionCtrlApp::getMotionState() {
    return static_cast<MotionState>(motionCtx.motionState);
}
#endif