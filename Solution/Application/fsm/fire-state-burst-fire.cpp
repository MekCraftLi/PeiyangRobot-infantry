/**
 *******************************************************************************
 * @file    fire-state-burst-fire.cpp
 * @brief   火控 FSM — BurstFire 状态实现
 *
 * 状态角色:
 *   连发态 — 拨弹盘纯速度环全速连发, 由 HeatController 动态调节安全射频
 *
 * Entry actions:
 *   - 清除校准标志 (下次单发前需重新校准)
 *   - 清零堵转计时器
 *   - 切换到纯速度环模式
 *
 * Exit actions:
 *   - 清空速度环积分项
 *   - 向上取整对齐到最近的拨弹槽位 (targetTriggerEcd)
 *   - 切回位置环模式
 *
 * Exit condition (transitions OUT):
 *   - BURST_STOP / SINGLE_FIRE / burstShot==0 -> Ready
 *   - FRIC_TOGGLE / EMERGENCY_STOP -> Passive
 *   - 堵转超时 2000 ms -> CaliReverse (记录来源为 BurstFire)
 *
 * Context modifications:
 *   - Writes: isCalibrated, blockStartTick, useTriggerSpeedLoopOnly,
 *             targetTriggerSpeed, targetTriggerEcd, jamSourceState, state
 *   - Clears: _triggerSpdPid (in exit)
 *
 *******************************************************************************
 * @attention
 *
 * 连发使用纯速度环, HeatController 根据热量余量动态调节射频。
 *
 *******************************************************************************
 * @note
 *
 * 退出时向上取整对齐槽位, 确保正在出膛的半颗子弹能完整打出。
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/3/3
 * @version 2.0
 *******************************************************************************
 */

#include "../fire-ctrl-app.h"
#include "Config/Gimbal/hw-config.h"

/**
 * @brief 进入 BurstFire 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateBurstFire::enter(FireCtrlCtx& ctx) {
    // --- 初始化 ---
    ctx.isCalibrated            = false;//取消拨弹盘校准
    ctx.blockStartTick          = 0;//堵转检测起始时刻 (0=未堵转)
    ctx.useTriggerSpeedLoopOnly = true;//绕过位置环, 仅速度环 (连发/校准)
    ctx.state                   = FireState::BurstFire;
}

/**
 * @brief 执行 BurstFire 状态逻辑
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateBurstFire::execute(FireCtrlCtx& ctx) {
    // --- 停止条件 ---
    if (ctx.transientEvent == ShootEvent::BURST_STOP ||
        ctx.transientEvent == ShootEvent::SINGLE_FIRE ||
        ctx.cmd.state.burstShot == 0) {
        request_switch(&instance()._stateReady);
        return;
    }

    // --- 紧急退出 ---
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE ||
        ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    // --- 热控器动态调节安全射频 ---
    /*返回值范围:
    热量充足: 返回最大转速（高速连发）
    热量紧张: 返回降低的转速（降速连发）
    热量超限: 直接停转*/
    ctx.targetTriggerSpeed = ctx.heatController.getSafeBurstRpm(Config::Hardware::MotorTopo::TRIGGER_SPEED, 36.0f);
    //targetTriggerSpeed会在calculateCurrents() 中应用
    // --- 堵转检测: 目标速度大但实际极低 ---
    float speedErr = std::abs(ctx.targetTriggerSpeed) - std::abs(ctx.fdb.trigger.vel);
    //功能: 检测拨弹盘是否发生机械卡死
    if (speedErr > 50.0f && std::abs(ctx.fdb.trigger.vel) < 10.0f) {
        if (ctx.blockStartTick == 0) {
            ctx.blockStartTick = xTaskGetTickCount();
        } else if (xTaskGetTickCount() - ctx.blockStartTick >= pdMS_TO_TICKS(800)) {
            ctx.jamSourceState = FireState::BurstFire;
            request_switch(&instance()._stateCaliReverse);
            return;
        }
    } else {
        ctx.blockStartTick = 0;
    }
    /*判断条件:
    目标转速与实际转速差 > 50 RPM
    实际转速 < 10 RPM（几乎静止）
    处理逻辑:
    开始计时（2000ms = 2秒）
    超时后切换到 CaliReverse 状态进行校准恢复
    记录堵转来源为 FireState::BurstFire*/
}

/**
 * @brief 退出 BurstFire 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateBurstFire::exit(FireCtrlCtx& ctx) {
    // --- 清空速度环积分 ---
    instance()._triggerSpdPid.clear();

    // --- 对齐到前方最近的槽位, 为位置环锁位做准备 ---
    int32_t currentEcd   = ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 - ctx.triggerOffset;
    int32_t ecdPerBullet = 8192 * 36 / 8;
    ctx.targetTriggerEcd = ((currentEcd + ecdPerBullet - 1) / ecdPerBullet) * ecdPerBullet;

    ctx.useTriggerSpeedLoopOnly = false;
}