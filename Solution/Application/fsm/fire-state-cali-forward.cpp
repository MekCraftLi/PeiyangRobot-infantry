/**
 *******************************************************************************
 * @file    fire-state-cali-forward.cpp
 * @brief   火控 FSM — CaliForward 状态实现
 *
 * 状态角色:
 *   校准正转态 — 从机械死区正转回到零点 (triggerOffset), 完成后进入 Ready
 *
 * Entry actions:
 *   - 切回位置环模式
 *   - 设定目标编码器为固定值 (8192 * 2)
 *
 * Exit condition (transitions OUT):
 *   - FRIC_TOGGLE / EMERGENCY_STOP -> Passive
 *   - 编码器到达目标附近 (误差 < 0.1 rad) -> Ready
 *
 * Context modifications:
 *   - Writes: useTriggerSpeedLoopOnly, targetTriggerEcd, isCalibrated, state
 *
 *******************************************************************************
 * @attention
 *
 * CaliForward 为 CaliReverse 的后续状态, 正转回零后完成校准流程。
 *
 *******************************************************************************
 * @note
 *
 * 进入此状态后设置 isCalibrated = true, 标记拨弹盘已完成校准。
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/3/3
 * @version 2.0
 *******************************************************************************
 */

#include "../fire-ctrl-app.h"

/**
 * @brief 进入 CaliForward 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateCaliForward::enter(FireCtrlCtx& ctx) {
    // --- 切回位置环, 目标为固定偏移量 ---
    ctx.useTriggerSpeedLoopOnly = false;
    ctx.targetTriggerEcd        = 41648;
    ctx.state                   = FireState::CaliForward;
}

/**
 * @brief 执行 CaliForward 状态逻辑
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateCaliForward::execute(FireCtrlCtx& ctx) {
    // --- 紧急退出 ---
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE ||
        ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    // --- 计算当前角度误差 ---
    float targetAngle = (float)(ctx.targetTriggerEcd) / (float)(8192 * 36) * 2.0f * (float)M_PI;
    float realAngle   = (float)(ctx.currentTriggerEcd) / (float)(8192 * 36) * 2.0f * (float)M_PI;

    float err = targetAngle - realAngle;
    while (err >  (float)M_PI) err -= 2.0f * (float)M_PI;
    while (err < -(float)M_PI) err += 2.0f * (float)M_PI;

    // --- 标记校准完成 ---
    ctx.isCalibrated = true;

    // --- 到达目标 → 进入 Ready ---
    if (std::abs(err) < 0.1f) {
        request_switch(&instance()._stateReady);
    }
}