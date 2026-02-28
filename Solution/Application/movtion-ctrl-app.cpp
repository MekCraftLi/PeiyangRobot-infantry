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
#define WHEEL_RADIUS 0.06f
#define DRIVE_GEAR_RATIO 19.2f // 减速比。如果状态反馈已经是轮端转速填1.0f；若是M3508原始转子反馈则填19.2f


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

    for (uint8_t i = 0; i < 4; i++) {
        motorsIdx[i] -= 1;
    }

}


void MovtionCtrlApp::run() {
// 1. 无锁极速读取意图和状态 (注意调用的是大写的 Read)
        static ChassisCmd cmd;
        static ChassisState state;
        Blackboard::instance().chassisCmd.read(cmd);
        Blackboard::instance().chassisState.read(state);

        ChassisOutput output = {0};       // 物理电流输出
        ChassisTelemetry telem = {0};     // 遥测数据计算中间量

        // 2. 状态机：处理急停/无力模式
        if (cmd.mode == CHASSIS_RELAX) {
            // RELAX 模式下，直接输出全 0，底层 CAN 会发送 0 电流，电机软掉
            Blackboard::instance().chassisOut.Write(output);
            return;
        }

        // 3. 舵轮运动学逆解计算 (经典 A-B-C-D 算法)
        // 【防呆提示】如果推前进摇杆，车是横着走的，请把你控制端的 vx 和 vy 传参互换！
        float vx = cmd.vx;  // 车头正前方速度 (X)
        float vy = cmd.vy;  // 车身正左方速度 (Y)
        float vw = cmd.vw;  // 逆时针旋转角速度 (W)

        float halfL = WHEEL_BASE / 2.0f;
        float halfW = TRACK_WIDTH / 2.0f;

        // 计算底盘前后左右边缘的绝对速度分量
        float A = vy + vw * halfL;   // 前排轮子的横向(Y)速度
        float B = vy - vw * halfL;   // 后排轮子的横向(Y)速度
        float C = vx - vw * halfW;   // 左排轮子的纵向(X)速度
        float D = vx + vw * halfW;   // 右排轮子的纵向(X)速度

        float targetVx[4], targetVy[4];

        // 依据索引严格映射：RF=0, LF=1, LB=2, RB=3
        targetVx[0] = D;  targetVy[0] = A;  // RF (右侧D, 前排A)
        targetVx[1] = C;  targetVy[1] = A;  // LF (左侧C, 前排A)
        targetVx[2] = C;  targetVy[2] = B;  // LB (左侧C, 后排B)
        targetVx[3] = D;  targetVy[3] = B;  // RB (右侧D, 后排B)

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
            tgtSpeed *= (DRIVE_GEAR_RATIO / WHEEL_RADIUS);

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
            float tgtSteerSpd = steerPosPid[motorsIdx[i]].calculate(finalTgtAngle, state.modules[motorsIdx[i]].steer.pos);
            telem.targetSteerVelocity[i] = tgtSteerSpd;

            // 内环：输入目标角速度，反馈真实角速度，输出电流指令
            output.steerCurrent[motorsIdx[i]] = steerSpdPid[motorsIdx[i]].calculate(tgtSteerSpd, state.modules[motorsIdx[i]].steer.vel);

            // (5) 动力轮：单环速度 PID 控制
            output.driveCurrent[motorsIdx[i]] = driveSpdPid[motorsIdx[i]].calculate(tgtSpeed, state.modules[motorsIdx[i]].drive.vel);
        }

        // 5. 将算出的 8 个电流值和遥测数据写入黑板
        Blackboard::instance().chassisOut.Write(output);
        Blackboard::instance().chassisTelem.Write(telem);
}
