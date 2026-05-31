/**
 *******************************************************************************
 * @file    fire-state-passive.cpp
 * @brief   火控 FSM — Passive 状态实现
 *
 * 状态角色:
 *   休眠态 — 摩擦轮停转, 拨弹盘位置环锁位防止溜弹
 *
 * Entry actions:
 *   - 清除校准标志 (isCalibrated = false)
 *   - 摩擦轮目标转速归零
 *   - 拨弹盘切回位置环模式
 *   - 重置弹速补偿器
 *
 * Exit condition (transitions OUT):
 *   - FRIC_TOGGLE -> SpinUp (启动摩擦轮)
 *
 * Context modifications:
 *   - Writes: isCalibrated, targetFricSpeed, useTriggerSpeedLoopOnly,
 *             targetTriggerEcd, state
 *   - Calls:  speedCompensator.reset()
 *
 *******************************************************************************
 * @attention
 *
 * 此状态为 FSM 初始状态, 摩擦轮关闭时拨弹盘通过位置环锁位防止断电滑转。
 *
 *******************************************************************************
 * @note
 *
 * 进入此状态后, 摩擦轮停止转动, 拨弹盘保持当前位置不动。
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/3/3
 * @version 2.0
 *******************************************************************************
 */

#include "../fire-ctrl-app.h"

/**
 * @brief 进入 Passive 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StatePassive::enter(FireCtrlCtx& ctx) {
    // --- 初始化状态 ---
    ctx.isCalibrated            = false;
    ctx.targetFricSpeed         = 0.0f;
    ctx.speedCompensator.reset();
    ctx.state                   = FireState::Passive;
    ctx.useTriggerSpeedLoopOnly = true;
    ctx.targetTriggerSpeed = 0;
}

/**
 * @brief 执行 Passive 状态逻辑
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StatePassive::execute(FireCtrlCtx& ctx) {

    // --- 转移条件 ---
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE) {
        request_switch(&instance()._stateSpinUp);
    }
}