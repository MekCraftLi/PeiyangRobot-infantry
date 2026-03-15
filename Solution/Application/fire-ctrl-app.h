/**
 *******************************************************************************
 * @file    fire-ctrl-app.h
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
 * @date    2026/3/3
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_FIRE_CTRL_APP_H
#define INFANTRY_FIRE_CTRL_APP_H




/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#ifdef __cplusplus

/* I. interface */

#include "./System/Thread/application-base.h"

/* II. OS */


/* III. middlewares */
#include "Algorithm/Shoot/speed-compensater.h"
#include "System/DataHub/data-def.h"
#include "./tools/crtp.h"
#include "pyro_algo_pid.h"
#include "pyro_core_fsm.h"


/* IV. drivers */


/* V. standard lib */





/*-------- 2. enum ---------------------------------------------------------------------------------------------------*/




/*-------- 3. interface ---------------------------------------------------------------------------------------------*/

class FireCtrlApp final : public PeriodicApp, public Singleton<FireCtrlApp> {
  public:

    FireCtrlApp();
    enum class FireState {
        Passive,
        SpinUp,
        Ready,
        CaliReverse,
        CaliForward,
        SingleFire,
        BurstFire,
        JamClear,
    };

    // ==========================================
    // 1. 火控上下文 (FSM Context)
    // ==========================================
    struct FireCtrlCtx {
        // 黑板数据缓存
        ShootCmd cmd;
        ShootEvent transientEvent;         // 边沿触发的瞬态事件
        SpeedCompensator speedCompensator; // 弹速补偿器
        struct {
            uint32_t burstShot : 1;
        } inputState;

        BoosterState fdb; // 电机实时反馈
        FireState state;

        // FSM 决定的目标运动量 (供后续 PID 运算)
        float targetFricSpeed;
        int32_t targetTriggerEcd;
        uint32_t triggerOffset;
        float targetTriggerSpeed;
        bool useTriggerSpeedLoopOnly; // 连发/校准时，绕过位置环直接使用速度环

        // 状态机内部控制变量
        bool isCalibrated   = false;
        uint32_t stateTimer = 0;
        uint32_t blockTimer = 0;
    };

    // ==========================================
    // 2. FSM 状态类声明 (严格生命周期)
    // ==========================================
    struct StatePassive : public pyro::state_t<FireCtrlCtx> {
        void enter(FireCtrlCtx& ctx) override;
        void execute(FireCtrlCtx& ctx) override;
        void exit(FireCtrlCtx& ctx) override {}
    };
    struct StateSpinUp : public pyro::state_t<FireCtrlCtx> {
        void enter(FireCtrlCtx& ctx) override;
        void execute(FireCtrlCtx& ctx) override;
        void exit(FireCtrlCtx& ctx) override {}
    };
    struct StateReady : public pyro::state_t<FireCtrlCtx> {
        void enter(FireCtrlCtx& ctx) override;
        void execute(FireCtrlCtx& ctx) override;
        void exit(FireCtrlCtx& ctx) override {}
    };
    struct StateCaliReverse : public pyro::state_t<FireCtrlCtx> {
        void enter(FireCtrlCtx& ctx) override;
        void execute(FireCtrlCtx& ctx) override;
        void exit(FireCtrlCtx& ctx) override;
    };
    struct StateCaliForward : public pyro::state_t<FireCtrlCtx> {
        void enter(FireCtrlCtx& ctx) override;
        void execute(FireCtrlCtx& ctx) override;
        void exit(FireCtrlCtx& ctx) override {}
    };
    struct StateSingleFire : public pyro::state_t<FireCtrlCtx> {
        void enter(FireCtrlCtx& ctx) override;
        void execute(FireCtrlCtx& ctx) override;
        void exit(FireCtrlCtx& ctx) override {}
    };
    struct StateBurstFire : public pyro::state_t<FireCtrlCtx> {
        void enter(FireCtrlCtx& ctx) override;
        void execute(FireCtrlCtx& ctx) override;
        void exit(FireCtrlCtx& ctx) override;
    };
    struct StateJamClear : public pyro::state_t<FireCtrlCtx> {
        void enter(FireCtrlCtx& ctx) override;
        void execute(FireCtrlCtx& ctx) override;
        void exit(FireCtrlCtx& ctx) override;
    };


    void init() override;

    void run() override;

    /************ setter & getter ***********/



  private:
    void updateTransientEvent();
    void calculateCurrents(BoosterOutput& out);

    // FSM 实例与上下文
    pyro::fsm_t<FireCtrlCtx> _fsm;
    FireCtrlCtx _ctx;

    // 静态状态单例 (内存连续，无动态分配)
    StatePassive _statePassive;
    StateSpinUp _stateSpinUp;
    StateReady _stateReady;
    StateCaliReverse _stateCaliReverse;
    StateCaliForward _stateCaliForward;
    StateSingleFire _stateSingleFire;
    StateBurstFire _stateBurstFire;
    StateJamClear _stateJamClear;

    // 独立维护的算法组件 (PID 控制器)
    pyro::pid_t _fricLeftSpdPid  = pyro::pid_t(0.22f, 0.0f, 0.0f, 0.0f, 20.0f);
    pyro::pid_t _fricRightSpdPid = pyro::pid_t(0.22f, 0.0f, 0.0f, 0.0f, 20.0f);
    ;
    pyro::pid_t _triggerPosPid = pyro::pid_t(1000.0f, 0.0f, 0.0f, 100.0f, 1000.0f);
    ;
    pyro::pid_t _triggerSpdPid = pyro::pid_t(0.05f, 0.02f, 0.0f, 5.0f, 20.0f);
    ;

    ShootEvent _lastEvent = ShootEvent::NONE;

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