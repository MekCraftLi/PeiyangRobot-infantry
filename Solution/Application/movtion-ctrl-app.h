/**
 *******************************************************************************
 * @file    movtion-ctrl-app.h
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

#ifndef INFANTRY_CHASSIS_MOVTION_CTRL_APP_H
#define INFANTRY_CHASSIS_MOVTION_CTRL_APP_H




/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#ifdef __cplusplus

/* I. interface */

#include "../System/Thread/application-base.h"
#include "../tools/crtp.h"
#include "Config/config.h"
#include "pyro_algo_pid.h"

/* II. OS */


/* III. middlewares */

#include "Algorithm/Motion/s-curve-planner.h"
#include "Algorithm/Power/power-limiter.h"
#include "System/DataHub/data-def.h"
#include "pyro_core_fsm.h"


/* IV. drivers */


/* V. standard lib */





/*-------- 2. enum ---------------------------------------------------------------------------------------------------*/

inline float wrapAngle(float angle) {
    angle = std::fmod(angle + M_PI, 2.0f * M_PI);
    if (angle < 0) angle += 2.0f * M_PI;
    return angle - M_PI;
}

struct PIDParam {
    float kp;
    float ki;
    float kd;
    float integralLimit;
    float outputLimit;
};



/*-------- 3. interface ---------------------------------------------------------------------------------------------*/

#ifdef GIMBAL
struct GimbalMotionCtx {
    GimbalCmd cmd;
    ImuState imu;
    GimbalState state;
    GimbalOutput output;
    GimbalTelemetry telem;
    float dt;
    uint8_t motionState; // MovtionCtrlApp::MotionState, 用 uint8_t 避免前向引用
};
#endif

class MovtionCtrlApp final : public PeriodicApp, public Singleton<MovtionCtrlApp> {
public:
    MovtionCtrlApp();

    void init() override;

    void run() override;

    /************ setter & getter ***********/

    enum class MotionState { Relax, Align, Manual, Auto };
    MotionState getMotionState();

#ifdef CHASSIS

    // [新增] 实例化三个轴的 S 曲线规划器
    // 这里的参数基于 RM 步兵常见调车经验，你可以通过 algo-config.h 去宏定义
    // 1. 前后平移 (Vx)：通常底盘较长，抗前倾能力较强，可以略微激进
    // 最大速度 3m/s，最大加速度 4.5m/s²(约0.45G)，加加速度 25m/s³
    SCurveVelocityPlanner vxPlanner{3.0f, 4.5f, 500.0f};

    // 2. 左右平移 (Vy)：麦轮左右平移效率低，且轮距通常较短，最容易侧翻！必须保守！
    // 最大速度 2.5m/s，最大加速度 3.0m/s²(约0.3G)，加加速度 15m/s³ (强制柔和起步)
    SCurveVelocityPlanner vyPlanner{2.5f, 4.5f, 500.0f};

    // 3. 自旋 (Vw)：“小陀螺”模式。自旋通常不会翻车，但会引起极大的离心力和功率尖峰。
    // 最大速度 6.28rad/s(约1圈/秒)，角加速度 15rad/s²，角加加速度 80rad/s³
    SCurveVelocityPlanner vwPlanner{6.28f, 15.0f, 80.0f};

    pyro::pid_t driveSpdPid[4] = {
        pyro::pid_t(0.2f, 0.000f, 0.0f, 1.0f, 20.0f),
        pyro::pid_t(0.2f, 0.000f, 0.0f, 1.0f, 20.0f),
        pyro::pid_t(0.2f, 0.000f, 0.0f, 1.0f, 20.0f),
        pyro::pid_t(0.2f, 0.000f, 0.0f, 1.0f, 20.0f),
    };
    pyro::pid_t steerPosPid[4] = {
        pyro::pid_t(58.0f,  5.0f, 0.0f, 10.0f, 50.0f),
        pyro::pid_t(58.0f,  5.0f, 0.0f, 10.0f, 50.0f),
        pyro::pid_t(58.0f,  5.0f, 0.0f, 10.0f, 50.0f),
        pyro::pid_t(58.0f,  5.0f, 0.0f, 10.0f, 50.0f),
    };
    pyro::pid_t steerSpdPid[4] = {
        pyro::pid_t(0.87f,  0.0f, 0.0f, 1.0f, 24.0f),
        pyro::pid_t(0.87f,  0.0f, 0.0f, 1.0f, 24.0f),
        pyro::pid_t(0.87f,  0.0f, 0.0f, 1.0f, 24.0f),
        pyro::pid_t(0.87f,  0.0f, 0.0f, 1.0f, 24.0f)
    };

    pyro::pid_t yawPosPid = pyro::pid_t(11.0f, 0.1f, 0.1f, 1.0f, 100.0f);

    uint8_t motorIdx[4] = {0};



#elif defined(GIMBAL)

    pyro::pid_t yawPosPid = pyro::pid_t(YAW_POS_PID_KP, YAW_POS_PID_KI, YAW_POS_PID_KD, 10.0f, 20.0f);
    pyro::pid_t yawSpdPid = pyro::pid_t(YAW_SPEED_PID_KP, YAW_SPEED_PID_KI, YAW_SPEED_PID_KD, 0.0f, 24.0f);
    pyro::pid_t pitchPosPid = pyro::pid_t(PITCH_DM_MOT_KP, PITCH_DM_MOT_KI, PITCH_DM_MOT_KD, 12.0f, 12.0f);
    //25 0 1

    void updatePitch(GimbalMotionCtx& ctx);
    void updateYaw(GimbalMotionCtx& ctx);

    // ── FSM 状态声明 (实现拆分到 fsm/) ──
    struct StateRelax : public pyro::state_t<GimbalMotionCtx> {
        void enter(GimbalMotionCtx& ctx) override;
        void execute(GimbalMotionCtx& ctx) override;
        void exit(GimbalMotionCtx&) override {}
    };
    struct StateAlign : public pyro::state_t<GimbalMotionCtx> {
        void enter(GimbalMotionCtx& ctx) override;
        void execute(GimbalMotionCtx& ctx) override;
        void exit(GimbalMotionCtx&) override {}
    };
    struct StateManual : public pyro::state_t<GimbalMotionCtx> {
        void enter(GimbalMotionCtx& ctx) override;
        void execute(GimbalMotionCtx& ctx) override;
        void exit(GimbalMotionCtx&) override {}
    };
    struct StateAuto : public pyro::state_t<GimbalMotionCtx> {
        void enter(GimbalMotionCtx& ctx) override;
        void execute(GimbalMotionCtx& ctx) override;
        void exit(GimbalMotionCtx&) override {}
    };

    // ── FSM 状态实例 ──
    StateRelax  _stateRelax;
    StateAlign  _stateAlign;
    StateManual _stateManual;
    StateAuto   _stateAuto;

    // ── Align 状态 ──
    float _alignStableMs = 0.0f;
    float _alignVelFilt  = 0.0f;  // 编码器速度 50Hz 低通滤波输出
    pyro::pid_t _alignPosPid = pyro::pid_t(6.0f, 0.2f, 0.0f, 100.0f, 500.0f);
    pyro::pid_t _alignSpdPid = pyro::pid_t(4.0f, 0.007f, 0.0f, 5.0f, 20.0f);

#endif



  private:
    /* message interface */
    
    // 1. message queue
    
    // 2. mutex
    
    // 3. semphr
    
    // 4. notify
    
    // 5. stream or message
    
    // 6. event group

};
#endif


#ifdef __cplusplus
extern "C" {
#endif

    /* C Interface */

#ifdef __cplusplus
}
#endif




/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/

#endif

