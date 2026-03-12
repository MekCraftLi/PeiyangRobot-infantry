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
#include "./tools/crtp.h"
#include "System/DataHub/data-def.h"
#include "pyro_algo_pid.h"
#include "pyro_core_fsm.h"

/* II. OS */


/* III. middlewares */


/* IV. drivers */


/* V. standard lib */





/*-------- 2. enum ---------------------------------------------------------------------------------------------------*/




/*-------- 3. interface ---------------------------------------------------------------------------------------------*/

class FireCtrlApp final : public PeriodicApp, public Singleton<FireCtrlApp> {
  public:

    class SpeedCompensator {
    public:
        void update(float newInitialSpeed) {
            // 1. 过滤无效数据或异常子弹 (如卡弹碎弹导致的个位数弹速)
            if (newInitialSpeed < 15.0f || newInitialSpeed > 30.0f) {
                return;
            }

            // 2. 检测是否是新的一发有效测速
            if (std::abs(newInitialSpeed - _lastInitialSpeed) > 0.01f) {

                // 3. 计算误差
                float error = _targetSpeed - newInitialSpeed;

                // 4. 离散积分补偿 (I 控制)
                _radsCompensation += _ki * error;

                // 5. 严格限幅防暴走
                if (_radsCompensation > _maxCompensation)  _radsCompensation = _maxCompensation;
                if (_radsCompensation < -_maxCompensation) _radsCompensation = -_maxCompensation;

                _lastInitialSpeed = newInitialSpeed;
            }
        }

        // 获取补偿后的最终角速度指令
        float getCompensatedRadPerSec(float baseRadPerSec) const {
            return baseRadPerSec + _radsCompensation;
        }

        void reset() {
            _radsCompensation = 0.0f;
        }

    private:
        float _targetSpeed = 23.5f;   // 期望压制的安全弹速 (m/s)

        // ==========================================================
        // 关键参数 (已换算为 rad/s)
        // ==========================================================
        // 积分增益: 1m/s 的误差，每发补偿约 5.0 rad/s (折合原来约 50 RPM)
        // 假设摩擦轮半径约 30mm，纯物理无滑差换算是 33 rad/s，但为了平滑和防止延迟超调，I参数应远小于物理值
        float _ki = 5.0f;

        // 极限补偿幅度上限: 最多允许上下浮动 63 rad/s (折合原来约 600 RPM)
        float _maxCompensation = 63.0f;

        float _radsCompensation = 0.0f;
        float _lastInitialSpeed = 0.0f;
    };


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
    } ;

    // ==========================================
    // 1. 火控上下文 (FSM Context)
    // ==========================================
    struct FireCtrlCtx {
        // 黑板数据缓存
        ShootCmd cmd;
        ShootEvent transientEvent; // 边沿触发的瞬态事件
        SpeedCompensator speedCompensator; // 弹速补偿器
        struct {
            uint32_t burstShot:1;
        }inputState;

        BoosterState fdb;          // 电机实时反馈
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