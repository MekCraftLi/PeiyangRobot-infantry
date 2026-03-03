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

#ifdef CHASSIS
void MovtionCtrlApp::init() {
    /* driver object initialize */
    for (uint8_t i = 0; i < 4; i++) {
        motorIdx[i] = Config::Hardware::MotorTopo::DRIVE_MOTOR_IDS[i] - 1;
    }
}


void MovtionCtrlApp::run() {

    static ChassisCmd cmd;
    static ChassisState state;
    Blackboard::instance().chassisCmd.read(cmd);
    Blackboard::instance().chassisState.read(state);

    ChassisOutput output   = {0}; // 物理电流输出
    ChassisTelemetry telem = {0}; // 遥测数据计算中间量

    // 2. 状态机：处理急停/无力模式
    if (cmd.mode == CHASSIS_RELAX) {
        // RELAX 模式下，直接输出全 0，底层 CAN 会发送 0 电流，电机软掉
        Blackboard::instance().chassisOut.write(output);
        return;
    }


    // 3. 舵轮运动学逆解计算 (经典 A-B-C-D 算法)
    // 【防呆提示】如果推前进摇杆，车是横着走的，请把你控制端的 vx 和 vy 传参互换！
    float vx       = cmd.vx; // 车头正前方速度 (X)
    float vy       = cmd.vy; // 车身正左方速度 (Y)
    float vw;                // 逆时针旋转角速度 (W)



    if (cmd.mode == CHASSIS_NORMAL) {

        vw = yawPosPid.calculate(0, state.yaw.pos);
    } else {
        vw = 0;
    }


    float halfL = Config::Hardware::Chassis::WHEEL_BASE / 2.0f;
    float halfW = Config::Hardware::Chassis::TRACK_WIDTH / 2.0f;

    // 计算底盘前后左右边缘的绝对速度分量
    float A     = vy + vw * halfL; // 前排轮子的横向(Y)速度
    float B     = vy - vw * halfL; // 后排轮子的横向(Y)速度
    float C     = vx - vw * halfW; // 左排轮子的纵向(X)速度
    float D     = vx + vw * halfW; // 右排轮子的纵向(X)速度

    float targetVx[4], targetVy[4];

    // 依据索引严格映射：RF=0, LF=1, LB=2, RB=3
    targetVx[0] = D;
    targetVy[0] = A; // RF (右侧D, 前排A)
    targetVx[1] = C;
    targetVy[1] = A; // LF (左侧C, 前排A)
    targetVx[2] = C;
    targetVy[2] = B; // LB (左侧C, 后排B)
    targetVx[3] = D;
    targetVy[3] = B; // RB (右侧D, 后排B)

    // 4. 计算每个模块的期望转速与期望打角，并执行 PID
    for (int i = 0; i < 4; i++) {
        // (1) 求极坐标系下的目标角度和目标速度 (此时是线速度 m/s)
        float tgtAngle = atan2f(targetVy[i], targetVx[i]);
        float tgtSpeed = sqrtf(targetVx[i] * targetVx[i] + targetVy[i] * targetVy[i]);

        // 注: 由于 3508 是对称摆放的，因此有一侧的速度需要取反
        // 假设你的物理机构中，LF(1) 和 LB(2) 这一侧由于对称安装导致转动方向相反
        if (i == 0 || i == 3) {
            tgtSpeed = -tgtSpeed;
        }

        // 线速度(m/s) 转换为 角速度(rad/s)
        tgtSpeed *= (Config::Hardware::Chassis::DRIVE_GEAR_RATIO / Config::Hardware::Chassis::WHEEL_RADIUS);

        // (2) 获取当前舵轮的真实反馈角度
        float realAngle = state.modules[i].steer.pos;

        // (3) 就近优选算法 (Angle Optimization)
        // 算出目标角度与当前角度的差值，规整到 [-PI, PI] 之间
        float errAngle  = tgtAngle - realAngle;
        while (errAngle > pyro::PI)
            errAngle -= 2.0f * pyro::PI;
        while (errAngle < -pyro::PI)
            errAngle += 2.0f * pyro::PI;

        // 如果差值超过 90 度 (PI/2)，说明转大弯不如直接把轮子反转
        if (errAngle > pyro::PI / 2.0f) {
            errAngle -= pyro::PI;
            tgtSpeed = -tgtSpeed; // 动力轮反转
        } else if (errAngle < -pyro::PI / 2.0f) {
            errAngle += pyro::PI;
            tgtSpeed = -tgtSpeed; // 动力轮反转
        }

        // 更新优化后的最终目标角度 (用于遥测观察)
        float finalTgtAngle       = realAngle + errAngle;
        telem.targetSteerAngle[i] = finalTgtAngle;
        telem.targetDriveSpd[i]   = tgtSpeed;

        // (4) 航向舵：位置-速度 串级 PID 控制
        // 外环：输入目标角度，反馈真实角度，输出目标角速度
        float tgtSteerSpd = steerPosPid[motorIdx[i]].calculate(finalTgtAngle, state.modules[motorIdx[i]].steer.pos);
        telem.targetSteerVelocity[i] = tgtSteerSpd;

        // 内环：输入目标角速度，反馈真实角速度，输出电流指令
        output.steerVoltage[motorIdx[i]] =
            steerSpdPid[motorIdx[i]].calculate(tgtSteerSpd, state.modules[motorIdx[i]].steer.vel);

        // (5) 动力轮：单环速度 PID 控制
        output.driveCurrent[motorIdx[i]] =
            driveSpdPid[motorIdx[i]].calculate(tgtSpeed, state.modules[motorIdx[i]].drive.vel);
    }

    // 5. 将算出的 8 个电流值和遥测数据写入黑板
    Blackboard::instance().chassisOut.write(output);
    Blackboard::instance().chassisTelem.write(telem);
}
#elifdef GIMBAL

void MovtionCtrlApp::init() { /* driver object initialize */ }

void MovtionCtrlApp::run() {
    GimbalCmd cmd;
    ImuState imuState;
    GimbalState gimbalState;
    GimbalTelemetry telem;
    static uint32_t dwtCnt;
    float dt = pyro::dwt_drv_t::get_delta_t(&dwtCnt);

    // 准备输入的数据
    Blackboard::instance().gimbalCmd.read(cmd);
    Blackboard::instance().imuState.read(imuState);
    Blackboard::instance().gimbalState.read(gimbalState);
    Blackboard::instance().telem.read(telem);
    GimbalOutput output = {0}; // 物理电流输出

    // 2. 状态机：处理急停/无力模式
    if (cmd.mode == GIMBAL_RELAX) {
        // RELAX 模式下，直接输出全 0，底层 CAN 会发送 0 电流，电机软掉

        telem.targetYawRad    = imuState.yaw;
        telem.targetPitchRad  = imuState.pitch;
        output.targetPitchPos = gimbalState.pitch.pos;
        output.pitchEn        = false;

        Blackboard::instance().telem.write(telem);
        Blackboard::instance().gimbalOut.write(output);
        return;
    }

    Blackboard::instance().gimbalOut.read(output);

    // 外环：输入目标角度，反馈真实角度，输出目标角速度

    telem.targetYawRad += cmd.yawVel * dt;

    while (telem.targetYawRad > pyro::PI)
        telem.targetYawRad -= 2.0f * pyro::PI;
    while (telem.targetYawRad < -pyro::PI)
        telem.targetYawRad += 2.0f * pyro::PI;

    float err = telem.targetYawRad - imuState.yaw;

    while (err > M_PI) {
        err -= 2.0 * M_PI;
    }
    while (err < -M_PI) {
        err += 2.0 * M_PI;
    }

    float alignedTgtYaw   = imuState.yaw + err;

    float tgtYawSpd       = yawPosPid.calculate(alignedTgtYaw, imuState.yaw);

    // 内环：输入目标角速度，反馈真实角速度，输出电流指令
    telem.targetYawRotate = tgtYawSpd;
    output.yawVoltage     = yawSpdPid.calculate(tgtYawSpd, imuState.gyro[2]);



    /*=========================Pitch计算===========================*/
    // =========================================================
    // Pitch 轴控制计算 (MIT 模式: 目标规划 + 混合前馈)
    // =========================================================


    // --- 步骤 1：动态坐标映射 (绝对期望转相对期望) ---
    // offset = 绝对仰角 - 电机相对角度
    float offsetPitch =
        imuState.pitch -
        gimbalState.pitch.pos; // 注意：由于你之前定义了 IMU 也在 state 里，这里应为 imu_pitch - motor_pitch

    telem.targetPitchRad += cmd.pitchVel * dt;

    float targetMotorRaw = telem.targetPitchRad - offsetPitch;

    // --- 步骤 2：基于方案一的物理限位裁切 ---

    if (targetMotorRaw > Config::Algorithm::Gimbal::PITCH_DEPRESSION_LIMIT) {
        targetMotorRaw = Config::Algorithm::Gimbal::PITCH_DEPRESSION_LIMIT;
    } else if (targetMotorRaw < Config::Algorithm::Gimbal::PITCH_ELEVATION_LIMIT) {
        targetMotorRaw = Config::Algorithm::Gimbal::PITCH_ELEVATION_LIMIT;
    }

    telem.targetPitchRad          = targetMotorRaw + offsetPitch;


    // --- 步骤 3：基于方案二的混合前馈计算 (物理重力 + 软积分) ---
    // 3.1 纯物理重力前馈 (假设水平时下坠力矩最大，随俯仰角余弦变化)
    float gravity_ff              = Config::Algorithm::Gimbal::PITCH_K_GRAVITY * std::cos(imuState.pitch);

    // 3.2 外环软积分补偿 (消除摩擦力、线束阻力造成的静态误差)
    float pitchIntegralTorque     = pitchPosPid.calculate(telem.targetPitchRad, imuState.pitch);

    // 3.3 统合前馈总力矩
    float t_ff                    = gravity_ff + pitchIntegralTorque;

    // --- 步骤 4：装填发给 MIT 模式的参数 ---
    // 注意：在你的 GimbalOutput 中，如果还没定义 targetPos 和 targetVel，需要去 data-def.h 补充

    output.targetPitchPos         = targetMotorRaw;
    output.pitchFeedforwardTorque = t_ff; // 借助原有的 Current 字段下发前馈 Torque
    output.pitchEn                = true;

    // 数据输出
    Blackboard::instance().gimbalOut.write(output);
    Blackboard::instance().telem.write(telem);
}
#endif