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

#define WHEEL_BASE 0.36f
#define TRACK_WIDTH 0.33f




/* ------- include ---------------------------------------------------------------------------------------------------*/



/* I. header */

#include "movtion-ctrl-app.h"
#include "../System/DataHub/blackboard.h"

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
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE,  appStack, APPLICATION_PRIORITY, 1){
}


void MovtionCtrlApp::init() {
    /* driver object initialize */

    for (int i = 0; i < 4; i++) {
            // 动力轮单环 (速度->电流) | 假设积分限幅为 5000.0f
            drive_spd_pid[i]  = new pyro::pid_t(10.0f, 0.1f, 0.0f, 5000.0f, 16384.0f);

            // 航向舵外环 (角度->角速度) | 假设积分限幅为 10.0f (外环通常无积分或小积分)
            steer_pos_pid[i]  = new pyro::pid_t(5.0f,  0.0f, 0.0f, 10.0f, 50.0f);

            // 航向舵内环 (角速度->电流) | 假设积分限幅为 5000.0f
            steer_spd_pid[i]  = new pyro::pid_t(8.0f,  0.1f, 0.0f, 5000.0f, 25000.0f);
        }
}


void MovtionCtrlApp::run() {
// 1. 无锁极速读取意图和状态
        static ChassisCmd cmd;
        static ChassisState state;
        Blackboard::instance().chassisCmd.Read(cmd);
        Blackboard::instance().chassisState.Read(state);

        static ChassisOutput output = {0};       // 物理电流输出
        static ChassisTelemetry telem = {0};     // 遥测数据计算中间量

        // 2. 状态机：处理急停/无力模式
        if (cmd.mode == CHASSIS_RELAX) {
            // RELAX 模式下，直接输出全 0，底层 CAN 会发送 0 电流，电机软掉
            Blackboard::instance().chassisOut.Write(output);
            return;
        }

        // 3. 舵轮运动学逆解计算 (以右前为 0，左前为 1，左后为 2，右后为 3 为例)
        // 计算每个轮子的等效 X/Y 速度分量
        // Vxi = Vx - W * Yi;  Vyi = Vy + W * Xi
        float vx = cmd.vx;
        float vy = cmd.vy;
        float vw = cmd.vw;
        float halfL = WHEEL_BASE / 2.0f;
        float halfW = TRACK_WIDTH / 2.0f;

        float targetVx[4], targetVy[4];
        targetVx[0] = vx - vw * (-halfW);  targetVy[0] = vy + vw * (halfL);  // RF
        targetVx[1] = vx - vw * (halfW);   targetVy[1] = vy + vw * (halfL);  // LF
        targetVx[2] = vx - vw * (halfW);   targetVy[2] = vy + vw * (-halfL); // LB
        targetVx[3] = vx - vw * (-halfW);  targetVy[3] = vy + vw * (-halfL); // RB

        // 4. 计算每个模块的期望转速与期望打角，并执行 PID
        for (int i = 0; i < 4; i++) {
            // (1) 求极坐标系下的目标角度和目标速度
            float tgtAngle = atan2f(targetVy[i], targetVx[i]);
            float tgtSpeed = sqrtf(targetVx[i] * targetVx[i] + targetVy[i] * targetVy[i]);

            // (2) 获取当前舵轮的真实反馈角度
            float realAngle = state.modules[i].steer.pos;

            // (3) 就近优选算法 (Angle Optimization)
            // 算出目标角度与当前角度的差值，规整到 [-PI, PI] 之间
            float errAngle = tgtAngle - realAngle;
            while (errAngle > pyro::PI)  errAngle -= 2.0f * pyro::PI;
            while (errAngle < -pyro::PI) errAngle += 2.0f * pyro::PI;

            // 如果差值超过 90 度 (PI/2)，说明转大弯不如直接把轮子反转
            if (errAngle > pyro::PI / 2.0f) {
                errAngle -= pyro::PI;
                tgtSpeed = -tgtSpeed; // 动力轮反转
            } else if (errAngle < -pyro::PI / 2.0f) {
                errAngle += pyro::PI;
                tgtSpeed = -tgtSpeed; // 动力轮反转
            }

            // 更新优化后的最终目标角度 (用于遥测观察)
            float finalTgtAngle = realAngle + errAngle;
            telem.targetSteerAngle[i] = finalTgtAngle;
            telem.targetDriveSpd[i]   = tgtSpeed;

            // (4) 航向舵：位置-速度 串级 PID 控制
            // 外环：输入目标角度，反馈真实角度，输出目标角速度
            float tgtSteerSpd = steer_pos_pid[i]->calculate(finalTgtAngle, state.modules[i].steer.pos);
            // 内环：输入目标角速度，反馈真实角速度，输出电流指令
            output.steerCurrent[i] = steer_spd_pid[i]->calculate(tgtSteerSpd, state.modules[i].steer.vel);

            // (5) 动力轮：单环速度 PID 控制
            output.driveCurrent[i] = drive_spd_pid[i]->calculate(tgtSpeed, state.modules[i].drive.vel);
        }

        // 5. 将算出的 8 个电流值和遥测数据写入黑板
        Blackboard::instance().chassisOut.Write(output);
        Blackboard::instance().chassisTelem.Write(telem);

 
}
