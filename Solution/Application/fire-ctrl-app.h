/**
 *******************************************************************************
 * @file    fire-ctrl-app.h
 * @brief   拨弹摩擦火控 FSM 应用
 *
 * 状态机总览:
 *
 *   Passive ──[FRIC_TOGGLE]──> SpinUp ──[friction wheels at speed]──> Ready
 *       ^                                                                    |
 *       |                    +---[FRIC_TOGGLE / EMERGENCY_STOP]-------------+
 *       |                    |
 *       |              Ready +--[!calibrated + SINGLE_FIRE]--> CaliReverse --> CaliForward --> SingleFire
 *       |              Ready +--[calibrated + SINGLE_FIRE]---> SingleFire --> Ready
 *       |              Ready +--[burstShot]--> BurstFire --> Ready
 *       |              Ready +--[burstShot + heat warn]--> SafeBurst --> Ready
 *       |                    |
 *       |             Jam in fire states ──> CaliReverse ──[reverse done]──> Ready/BurstFire/SafeBurst
 *       |                    ^
 *       |                    +---[jam from SingleFire/BurstFire/SafeBurst]
 *       |
 *       +----[from any state: FRIC_TOGGLE / EMERGENCY_STOP]
 *
 * 数据流:
 *   Commander -> Blackboard(shootCmd) -> FireCtrlApp -> FSM -> PID -> Blackboard(boosterOut)
 *                                               ^                     |
 *                                         Blackboard(boosterState) ---+
 *******************************************************************************
 * @attention
 *
 * 本应用为 Gimbal 专属, 负责摩擦轮启停、拨弹盘校准、单发/连发控制。
 * FSM 状态实现拆分到 fsm/ 目录下的独立文件。
 * 这里有摩擦轮和拨弹盘的pid
 *
 *******************************************************************************
 * @note
 *
 * 状态切换使用 pyro::fsm_t 框架, 状态类需实现 enter/execute/exit 方法。
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/3/3
 * @version 2.0
 *******************************************************************************
 */

#ifndef INFANTRY_FIRE_CTRL_APP_H
#define INFANTRY_FIRE_CTRL_APP_H

/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#ifdef __cplusplus

/* I. interface */
#include "./System/Thread/application-base.h"

/* II. OS */
#include "FreeRTOS.h"
#include "task.h"

/* III. middlewares */
#include "Algorithm/Shoot/speed-compensater.h"
#include "Algorithm/Shoot/heat-controller.h"
#include "System/DataHub/data-def.h"
#include "./tools/crtp.h"
#include "pyro_algo_pid.h"
#include "pyro_core_fsm.h"
#include "../Config/Gimbal/algo-config.h"

/* IV. drivers */

/* V. standard lib */

/*-------- 2. enum ---------------------------------------------------------------------------------------------------*/

/*-------- 3. interface ---------------------------------------------------------------------------------------------*/

/**
 * @brief Ozone 调试探针: 拨弹盘编码器状态
 */
struct debug_trigger_t {
    int32_t currentTrigger;  ///< 当前编码器位置
    int32_t targetTrigger;   ///< 目标编码器位置
    uint8_t state;           ///< FSM 状态枚举值
    uint32_t offset;         ///< 编码器零点偏移
};

/**
 * @brief 火控应用 — 拨弹摩擦 FSM 控制器
 *
 * 继承 PeriodicApp (固定周期执行) + Singleton<FireCtrlApp> (全局单例)
 */
class FireCtrlApp final : public PeriodicApp, public Singleton<FireCtrlApp> {
  public:
    FireCtrlApp();

    /**
     * @brief FSM 状态枚举
     */
    enum class FireState {
        Passive,       ///< 0 休眠: 摩擦轮停, 拨弹锁位
        SpinUp,        ///< 1 摩擦轮启动中
        Ready,         ///< 2 就绪: 摩擦轮已达速, 等待开火指令
        CaliReverse,   ///< 3 校准: 反转寻找机械死区
        CaliForward,   ///< 4 校准: 正转回到零点
        SingleFire,    ///< 5 单发: 拨弹盘推进一发
        BurstFire,     ///< 6 连发: 速度环全速连发
        JamClear,      ///< 7 (废弃) 堵转清除
        SafeBurst,     ///< 8 安全连发: 位置环逐发受控连发
    };

    enum class ReversePurpose {
        JamClear,              ///< 堵转清除: 倒转结束后直接退出
        SingleFireCalibration, ///< 单发校准: 倒转找零点后进入 CaliForward
    };

    /**
     * @brief FSM 上下文 (黑板数据缓存)
     *
     * 包含输入指令、算法组件、电机反馈、FSM 输出目标、校准与堵转状态。
     */
    struct FireCtrlCtx {
        // --- 输入指令 (来自 Commander / Blackboard) ---
        ShootCmd cmd;                       ///< 最新射击指令
        ShootEvent transientEvent;          ///< 边沿检测后的瞬态事件 (仅 1 tick)

        // --- 算法组件 ---
        SpeedCompensator speedCompensator;  ///< 弹速闭环补偿器
        HeatController heatController;      ///< 热量管理控制器

        // --- 电机反馈 (来自 MotActuator) ---
        BoosterState fdb;                   ///< 电机实时状态

        // --- 状态标识 ---
        FireState state;                    ///< 当前 FSM 状态 (供外部遥测)

        // --- FSM 输出目标 (供 calculateCurrents PID 运算) ---
        float targetFricSpeed;              ///< 摩擦轮目标转速 (rad/s)
        int32_t targetTriggerEcd;           ///< 拨弹盘目标编码器位置 (用于位置环)
        uint32_t triggerOffset;             ///< 编码器零点偏移 (校准后确定)
        float targetTriggerSpeed;           ///< 拨弹盘目标转速 (用于速度环)
        bool useTriggerSpeedLoopOnly;       ///< true=绕过位置环, 仅速度环 (连发/校准)

        // --- 编码器计算 ---
        int32_t rawTriggerEcd;              ///< 原始编码器值 (含圈数)
        int32_t currentTriggerEcd;          ///< 处理后的当前循环编码器位置 (减偏移, 模36周)

        // --- 校准 & 堵转 ---
        bool isCalibrated              = false;  ///< 是否已完成拨弹盘校准
        TickType_t stateStartTick      = 0;      ///< 状态进入时刻 (FreeRTOS tick)
        TickType_t blockStartTick      = 0;      ///< 堵转检测起始时刻 (0=未堵转)
        FireState jamSourceState       = FireState::Passive; ///< 堵转来源状态
        FireState targetStateAfterCali = FireState::Ready;   ///< 校准完成后目标状态
        ReversePurpose reversePurpose  = ReversePurpose::JamClear; ///< 当前倒转状态用途
    };

    // ==========================================
    // FSM 状态类声明 (实现拆分到 fsm/ 目录)
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
    class StateSafeBurst : public pyro::state_t<FireCtrlCtx> {
    public:
        void enter(FireCtrlCtx& ctx) override;
        void execute(FireCtrlCtx& ctx) override;
        void exit(FireCtrlCtx& ctx) override {}
    };
    struct StateJamClear : public pyro::state_t<FireCtrlCtx> {
        void enter(FireCtrlCtx& ctx) override;
        void execute(FireCtrlCtx& ctx) override;
        void exit(FireCtrlCtx& ctx) override;
    };

    // -------------------------------------------
    // 生命周期
    // -------------------------------------------
    void init() override;
    void run() override;

    // -------------------------------------------
    // 访问器
    // -------------------------------------------
    FireState getFireState();

  private:
    void updateTransientEvent();
    void calculateCurrents(BoosterOutput& out);

    // --- FSM ---
    pyro::fsm_t<FireCtrlCtx> _fsm;
    FireCtrlCtx _ctx;

    // --- 状态单例 ---
    StatePassive    _statePassive;
    StateSpinUp     _stateSpinUp;
    StateReady      _stateReady;
    StateCaliReverse _stateCaliReverse;
    StateCaliForward _stateCaliForward;
    StateSingleFire _stateSingleFire;
    StateBurstFire  _stateBurstFire;
    StateSafeBurst  _stateSafeBurst;
    StateJamClear   _stateJamClear;

    // --- PID 控制器 ---
    pyro::pid_t _fricLeftSpdPid  = pyro::pid_t(FRIC_SPEED_PID_KP, FRIC_SPEED_PID_KI, FRIC_SPEED_PID_KD, 0.0f, 20.0f);
    pyro::pid_t _fricRightSpdPid = pyro::pid_t(FRIC_SPEED_PID_KP, FRIC_SPEED_PID_KI, FRIC_SPEED_PID_KD, 0.0f, 20.0f);
    pyro::pid_t _triggerPosPid   = pyro::pid_t(TRIGGER_SINGLE_POS_PID_KP, TRIGGER_SINGLE_POS_PID_KI, TRIGGER_SINGLE_POS_PID_KD, 100.0f, 1000.0f);
    pyro::pid_t _triggerSpdPid   = pyro::pid_t(TRIGGER_SINGLE_SPEED_PID_KP, TRIGGER_SINGLE_SPEED_PID_KI, TRIGGER_SINGLE_SPEED_PID_KD, 5.0f, 10.0f);

    ShootEvent _lastEvent = ShootEvent::NONE;
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
