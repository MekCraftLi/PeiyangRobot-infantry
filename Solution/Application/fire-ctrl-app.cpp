/**
 *******************************************************************************
 * @file    fire-ctrl-app.cpp
 * @brief   拨弹摩擦火控 FSM 应用 — 主循环、PID 运算、物理发弹检测
 *
 * 主循环 (run) 执行顺序:
 *   1. 读取黑板输入 (指令、反馈、裁判数据)
 *   2. 热量/弹速同步 & 物理发弹检测
 *   3. 边沿事件提取
 *   4. 驱动 FSM (仅设定目标, 不做 PID)
 *   5. 统一 PID 计算输出电流
 *   6. 写入黑板输出
 *
 *******************************************************************************
 * @attention
 *
 * FSM 状态实现拆分到 fsm/ 目录下独立文件, 本文件仅包含主循环逻辑。
 *
 *******************************************************************************
 * @note
 *
 * 物理发弹检测使用 M2006 编码器跨越一发跨度 (36864 counts) 作为判定条件。
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/3/3
 * @version 2.0
 *******************************************************************************
 */

/* ------- include ---------------------------------------------------------------------------------------------------*/

#include "fire-ctrl-app.h"
#include "Config/Gimbal/hw-config.h"
#include "System/DataHub/blackboard.h"
#include "pyro_dwt_drv.h"
#include "System/DataHub/data-def.h"

/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = FireCtrlApp::instance();

/**
 * @brief Ozone 调试探针: 热量状态观测
 */
struct HeatDebugOzone {
    float local_heat;       ///< 算法预估的本地实时热量
    float referee_heat;     ///< 裁判系统真实下发的热量 (带延迟)
    float heat_limit;       ///< 热量上限
    float safe_margin_line; ///< 安全警戒线 (limit - safe_margin)
    float target_rpm;       ///< 拨弹电机目标转速
    uint8_t physical_shot;  ///< 物理发弹脉冲 (每发一次跳变)
} g_heat_debug;

/**
 * @brief Ozone 调试探针: 弹速补偿观测
 */
volatile struct SpeedDebugOzone {
    float ref_bullet_speed;    ///< 裁判系统回传的真实弹速 (m/s)
    float target_bullet_speed; ///< 期望压制弹速 (m/s)
    float base_fric_target;    ///< 基础摩擦轮设定转速
    float final_fric_target;   ///< 经闭环补偿后的最终目标转速
    float fric_left_real;      ///< 左摩擦轮实际反馈转速
    float comp_integration;    ///< 补偿增量
    uint8_t physical_shot;     ///< 物理发弹脉冲
    bool firecommn;
    ShootEvent shootcommandtelem;

    

} g_speed_debug;

/**
 * @brief Ozone 调试探针: 拨弹盘编码器状态
 */
debug_trigger_t debug_trigger;

/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true
#define APPLICATION_NAME       "FireCtrl"
#define APPLICATION_STACK_SIZE 512
#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE]; //为FreeRTOS任务分配栈内存

/* ------- 构造 & 生命周期 -------------------------------------------------------------------------------------------*/

FireCtrlApp::FireCtrlApp()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 1),
      _ctx() {}

void FireCtrlApp::init() {
    _fsm.change_state(&_statePassive);
    _fsm.enter(_ctx);
    _ctx.heatController.setHeatLimitEnabled(true);
} 

/* ------- 主循环 ----------------------------------------------------------------------------------------------------*/

void FireCtrlApp::run() {
    static uint32_t dwtCnt = 0;
    float dt = pyro::dwt_drv_t::get_delta_t(&dwtCnt);

    // ── 1. 读取黑板输入 ──
    ChassisToGimbalComm c2gData{};
    VisionCommand viscmd{};
    Blackboard::instance().shootCmd.read(_ctx.cmd);
    Blackboard::instance().boosterState.read(_ctx.fdb);
    Blackboard::instance().c2gComm.read(c2gData);
    Blackboard::instance().visionCmd.read(viscmd);
    
    uint32_t nowMs = xTaskGetTickCount();

    // ── 2a. 同步裁判系统 → 热量控制器 ──
    _ctx.heatController.syncWithReferee(
        c2gData.msg.shooter17mmBarrelHeat, c2gData.msg.heatLimit,
        c2gData.msg.coolingRate, nowMs);

    // ── 2b. 喂弹速补偿器 ──（最终没有启用）
    _ctx.speedCompensator.update((float)c2gData.msg.initialSpeedX100 / 100.0f);

    // ── 2c. 本地冷却推演 ──//
    _ctx.heatController.tickCooling(dt);

    // ── 2d. 物理发弹检测 (编码器跨越一发跨度 → 注册热量) ──
    const int32_t ECD_PER_BULLET = 8192 * 36 / 8; // M2006 单发编码器跨度 (36864)

    _ctx.rawTriggerEcd = _ctx.fdb.triggerEcd + _ctx.fdb.triggerRound * 8192;
    _ctx.currentTriggerEcd = (_ctx.rawTriggerEcd - _ctx.triggerOffset + 8192 * 36) % (8192 * 36);

    static int32_t lastShotContinuousEcd = _ctx.rawTriggerEcd;

    debug_trigger.currentTrigger = _ctx.currentTriggerEcd;
    debug_trigger.targetTrigger  = _ctx.targetTriggerEcd;
    debug_trigger.state           = (uint8_t)_ctx.state;
    debug_trigger.offset          = _ctx.triggerOffset;

    // 防抖: 差距过大 (如刚开机 / 校准后) → 直接对齐//避免刚开机时误判为连续发射了多发子弹//
    /* 问题场景分析
    场景1: 系统刚启动
    _ctx.rawTriggerEcd = 100000（拨弹盘当前位置）
    lastShotContinuousEcd = 0（静态变量初始值）
    差值 = 100000 - 0 = 100000
    而 ECD_PER_BULLET * 10 = 368640
    如果不处理，会误判为发射了 100000 / 36864 ≈ 2.7 发子弹！
    场景2: 拨弹盘校准完成
    校准过程中拨弹盘可能快速转动很多圈
    校准前: lastShotContinuousEcd = 50000
    校准后: _ctx.rawTriggerEcd = 200000
    差值 = 150000，会误判为发射了4发子弹
    */
    if (std::abs(_ctx.rawTriggerEcd - lastShotContinuousEcd) > ECD_PER_BULLET * 10) {
        lastShotContinuousEcd = _ctx.rawTriggerEcd;
    }

    if (_ctx.rawTriggerEcd - lastShotContinuousEcd >= ECD_PER_BULLET) {//检测到发弹
        _ctx.heatController.recordBulletShot(nowMs);//向热量控制器注册一次发弹事件
        lastShotContinuousEcd += ECD_PER_BULLET;//发弹检测的基准向前移动一发跨度
        g_heat_debug.physical_shot  = 50;//调试信号: 在Ozone示波器上产生一个50的脉冲信号，用于可视化发弹时刻
        g_speed_debug.physical_shot = 50;
    } else {
        g_heat_debug.physical_shot  = 0;
        g_speed_debug.physical_shot = 0;
    }

    // ── 3. 边沿事件提取 ──////供状态机读取
    updateTransientEvent();

    // ── 4. 驱动 FSM ──
    _fsm.execute(_ctx);

    // ── 5. 统一 PID 计算输出电流 ──
    BoosterOutput finalOut{};
    calculateCurrents(finalOut);

    // ── 6a. Ozone 探针赋值 ──
    g_heat_debug.local_heat       = _ctx.heatController.getLocalHeat();
    g_heat_debug.referee_heat     = c2gData.msg.shooter17mmBarrelHeat;
    g_heat_debug.heat_limit       = c2gData.msg.heatLimit;
    g_heat_debug.safe_margin_line = c2gData.msg.heatLimit - HeatController::SAFE_MARGIN;
    g_heat_debug.target_rpm       = _ctx.targetTriggerSpeed;
    g_speed_debug.ref_bullet_speed    = c2gData.msg.initialSpeedX100 / 100.0f;
    g_speed_debug.target_bullet_speed = 23.5f;
    g_speed_debug.base_fric_target    = _ctx.targetFricSpeed;
    g_speed_debug.final_fric_target   = _ctx.speedCompensator.getCompensatedRadPerSec(_ctx.targetFricSpeed);
    g_speed_debug.comp_integration    = g_speed_debug.final_fric_target - _ctx.targetFricSpeed;
    g_speed_debug.fric_left_real      = _ctx.fdb.fric[(uint8_t)Config::Hardware::MotorTopo::FRIC_LEFT_ID].vel;
    g_speed_debug.firecommn           = viscmd.fireCommand;
    g_speed_debug.shootcommandtelem   = _ctx.cmd.event;

    // ── 6b. 写入黑板输出 ──
    Blackboard::instance().boosterOut.write(finalOut);
}

/* ------- 内部方法 --------------------------------------------------------------------------------------------------*/

/**
 * @brief 边沿检测: cmd.event 发生变化时, 将其作为瞬态事件传递给 FSM.
 *        瞬态事件仅存活 1 tick, 下次调用自动归零.
 */
void FireCtrlApp::updateTransientEvent() {
    _ctx.transientEvent = ShootEvent::NONE;
    if (_ctx.cmd.event != _lastEvent) {
        _ctx.transientEvent = _ctx.cmd.event;
        _lastEvent          = _ctx.cmd.event;
    }
}

/**
 * @brief 统一 PID 电流计算.
 *
 * 摩擦轮: 双速度环 (左正右反).
 * 拨弹盘: 根据 useTriggerSpeedLoopOnly 选择:
 *   true  → 纯速度环 (连发/校准反转)
 *   false → 位置外环 + 速度内环 (单发/就绪锁位)
 *
 * 安全锁: 摩擦轮目标 < 10 rad/s 时, 强制拨弹电流归零并清空 PID 积分.
 */

 
void FireCtrlApp::calculateCurrents(BoosterOutput& out) {
    // ── 摩擦轮: 弹速闭环补偿 ──
    #ifdef STEER
    float finalFricTargetSpeed = _ctx.targetFricSpeed;
    #endif

    #if defined(DOG_1)||defined(DOG_2)
    float finalFricTargetSpeed = _ctx.speedCompensator.getCompensatedRadPerSec(_ctx.targetFricSpeed);
    #endif
    
    float bullet_L_speed = _ctx.fdb.fric[(uint8_t)Config::Hardware::MotorTopo::FRIC_LEFT_ID].vel;
    float bullet_R_speed = _ctx.fdb.fric[(uint8_t)Config::Hardware::MotorTopo::FRIC_RIGHT_ID].vel;
    // 左摩擦轮

    out.fricLeftCurrent = _fricLeftSpdPid.calculate(
        finalFricTargetSpeed,
        bullet_L_speed);

    // 右摩擦轮 (反向安装, 目标取反)
    out.fricRightCurrent = _fricRightSpdPid.calculate(
        -finalFricTargetSpeed,
        bullet_R_speed);

    // if(_ctx.state == FireState::Passive){
    //     out.fricLeftCurrent  = 0.0f;
    //     out.fricRightCurrent = 0.0f;
    // }


    // ── 拨弹盘 ──
    if (_ctx.targetFricSpeed < 10.0f ) {
            //目标值和实际值都检测一遍
        // 安全模式: 摩擦轮未启动 → 彻底断开拨弹盘动力
        out.triggerCurrent = 0.0f;
        _triggerPosPid.clear();
        _triggerSpdPid.clear();
        return;
    }

    float spdTarget = 0.0f;

    if (_ctx.useTriggerSpeedLoopOnly) {
        // 纯速度环 (连发 / 校准反转)
        spdTarget = _ctx.targetTriggerSpeed;
    } else {
        // 位置外环 → 速度内环 (单发 / 就绪锁位)
        float targetTriggerAngle = (float)(_ctx.targetTriggerEcd) / (float)(8192 * 36) * 2.0f * (float)M_PI;
        float realTriggerAngle   = (float)(_ctx.currentTriggerEcd) / (float)(8192 * 36) * 2.0f * (float)M_PI;

        float err = targetTriggerAngle - realTriggerAngle;
        while (err >  (float)M_PI) err -= 2.0f * (float)M_PI;
        while (err < -(float)M_PI) err += 2.0f * (float)M_PI;

        spdTarget = _triggerPosPid.calculate(realTriggerAngle + err, realTriggerAngle);
    }

    // 速度内环 → 电流
    out.triggerCurrent = _triggerSpdPid.calculate(spdTarget, _ctx.fdb.trigger.vel);
}

/* ------- 访问器 ----------------------------------------------------------------------------------------------------*/

FireCtrlApp::FireState FireCtrlApp::getFireState() {
    return _ctx.state;
}