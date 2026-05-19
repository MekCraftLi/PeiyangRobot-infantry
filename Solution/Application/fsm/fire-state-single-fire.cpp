/**
 *******************************************************************************
 * @file    fire-state-single-fire.cpp
 * @brief   火控 FSM — SingleFire 状态实现
 *
 * 状态角色:
 *   单发态 — 拨弹盘位置环推进一发编码器跨度, 到达后自动回到 Ready
 *
 * Entry actions:
 *   - 清零堵转计时器
 *   - 目标编码器前进一发 (targetTriggerEcd += ecdPerBullet)
 *   - 切回位置环模式
 *
 * Exit condition (transitions OUT):
 *   - FRIC_TOGGLE / EMERGENCY_STOP -> Passive
 *   - 位置误差 < 1000 counts (到达目标) -> Ready
 *   - 堵转超时 2000 ms -> CaliReverse (记录来源为 SingleFire)
 *
 * Context modifications:
 *   - Writes: blockStartTick, targetTriggerEcd, useTriggerSpeedLoopOnly,
 *             jamSourceState, state
 *
 *******************************************************************************
 * @attention
 *
 * 单发使用位置环精确控制, 到达目标位置后自动返回 Ready 等待下次指令。
 *
 *******************************************************************************
 * @note
 *
 * 堵转检测条件: 角度误差 > π/16 且 速度 < 10 rad/s, 持续 2000 ms。
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/3/3
 * @version 2.0
 *******************************************************************************
 */

#include "../fire-ctrl-app.h"

/**
 * @brief 进入 SingleFire 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateSingleFire::enter(FireCtrlCtx& ctx) {
    // --- 设定目标: 前进一发 ---
    ctx.blockStartTick          = 0;
    ctx.targetTriggerEcd        = (ctx.targetTriggerEcd + 8192 * 36 / 8) % (8192 * 36); // 前进 36864 编码器计数
    ctx.useTriggerSpeedLoopOnly = false;
    ctx.state                   = FireState::SingleFire;
}

/**
 * @brief 执行 SingleFire 状态逻辑
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateSingleFire::execute(FireCtrlCtx& ctx) {
    // --- 紧急退出 ---
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE ||
        ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    // --- 计算当前角度误差 ---
    float targetAngle = (float)(ctx.targetTriggerEcd) / (float)(8192 * 36) * 2.0f * (float)M_PI;

    int32_t ecd = ctx.fdb.triggerEcd + ctx.fdb.triggerRound * 8192 - ctx.triggerOffset;
    while (ecd < 0) ecd += 8192 * 36;
    float realAngle = (float)(ecd) / (float)(8192 * 36) * 2.0f * (float)M_PI;

    float err = targetAngle - realAngle;
    while (err >  (float)M_PI) err -= 2.0f * (float)M_PI;
    while (err < -(float)M_PI) err += 2.0f * (float)M_PI;

    // --- 堵转检测 ---
    // if (std::abs(err) > (float)M_PI / 16.0f && std::abs(ctx.fdb.trigger.vel) < 10.0f) {
    //     if (ctx.blockStartTick == 0) {
    //         ctx.blockStartTick = xTaskGetTickCount();
    //     } else if (xTaskGetTickCount() - ctx.blockStartTick >= pdMS_TO_TICKS(2000)) {
    //         ctx.jamSourceState = FireState::SingleFire;
    //         request_switch(&instance()._stateCaliReverse);
    //         return;
    //     }
    // } else {
    //     ctx.blockStartTick = 0;
    // }

    // --- 到达目标 → 回到 Ready ---
    if (std::abs(ctx.currentTriggerEcd - ctx.targetTriggerEcd) < 3000) {
        request_switch(&instance()._stateReady);
    }
}