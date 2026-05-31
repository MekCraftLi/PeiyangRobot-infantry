/**
 *******************************************************************************
 * @file    fire-state-jam-clear.cpp
 * @brief   火控 FSM — JamClear 状态实现 (DEPRECATED)
 *
 * 状态角色:
 *   堵转清除态 — 强制反转退弹 (已废弃)
 *
 * 此状态已废弃, 不再从任何其他状态进入。
 * 卡弹清除功能已由 CaliReverse → CaliForward 校准流程替代。
 * 保留此文件仅供参考, 未来可安全删除。
 *
 * Entry actions:
 *   - 记录状态进入时刻
 *   - 切换到纯速度环模式
 *
 * Exit actions:
 *   - 将位置环目标锁定到当前位置
 *   - 清除校准标志
 *   - 切回位置环模式
 *
 * Exit condition (transitions OUT):
 *   - 反转持续 150 ms -> Ready
 *
 * Context modifications:
 *   - Writes: stateStartTick, useTriggerSpeedLoopOnly, targetTriggerSpeed,
 *             targetTriggerEcd, isCalibrated, state
 *
 *******************************************************************************
 * @attention
 *
 * 此状态已废弃, 不应在生产代码中使用。
 *
 *******************************************************************************
 * @note
 *
 * 原始逻辑: 强制反转 150ms 后回到 Ready, 用于清除卡弹。
 * 现已被 CaliReverse/CaliForward 校准流程取代。
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/3/3
 * @version 2.0
 *******************************************************************************
 */

#include "../fire-ctrl-app.h"

/**
 * @brief 进入 JamClear 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateJamClear::enter(FireCtrlCtx& ctx) {
    ctx.stateStartTick          = xTaskGetTickCount();
    ctx.useTriggerSpeedLoopOnly = true;
    ctx.state                   = FireState::JamClear;
}

/**
 * @brief 执行 JamClear 状态逻辑
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateJamClear::execute(FireCtrlCtx& ctx) {
    ctx.targetTriggerSpeed = -300.0f; // 强行反转退弹
    if (xTaskGetTickCount() - ctx.stateStartTick >= pdMS_TO_TICKS(150)) {
        request_switch(&instance()._stateReady);
    }
}

/**
 * @brief 退出 JamClear 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateJamClear::exit(FireCtrlCtx& ctx) {
    ctx.targetTriggerEcd        = ctx.currentTriggerEcd;
    ctx.isCalibrated            = false;
    ctx.useTriggerSpeedLoopOnly = false;
}
