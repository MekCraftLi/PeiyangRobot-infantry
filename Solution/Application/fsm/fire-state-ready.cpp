/**
 *******************************************************************************
 * @file    fire-state-ready.cpp
 * @brief   火控 FSM — Ready 状态实现
 *
 * 状态角色:
 *   就绪态 — 摩擦轮已达标速, 拨弹盘位置环锁位, 等待射击指令
 *
 * Entry actions:
 *   - 拨弹盘切回位置环
 *
 * Exit condition (transitions OUT):
 *   - FRIC_TOGGLE / EMERGENCY_STOP -> Passive
 *   - SINGLE_FIRE + !isCalibrated  -> CaliReverse (先校准再打)
 *   - SINGLE_FIRE + isCalibrated + canShootSingle -> SingleFire
 *   - burstShot (持续按住)         -> BurstFire
 *
 * Context modifications:
 *   - Writes: useTriggerSpeedLoopOnly, targetTriggerEcd, isCalibrated, state
 *
 *******************************************************************************
 * @attention
 *
 * 进入 Ready 时不主动推进拨弹盘, 只切回位置环锁位。
 *
 *******************************************************************************
 * @note
 *
 * 未校准时单发指令会先进入校准流程, 校准完成后再执行单发。
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/3/3
 * @version 2.0
 *******************************************************************************
 */

#include "../fire-ctrl-app.h"

/**
 * @brief 进入 Ready 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateReady::enter(FireCtrlCtx& ctx) {
    // --- 切回位置环, 提供物理刚性防止溜弹 ---
    ctx.state            = FireState::Ready;

    ctx.useTriggerSpeedLoopOnly = true;
    ctx.targetTriggerSpeed = 0;

}

/**
 * @brief 执行 Ready 状态逻辑
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateReady::execute(FireCtrlCtx& ctx) {
    // --- 紧急退出 ---
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE ||
        ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    // --- 单发指令 ---
    if (ctx.transientEvent == ShootEvent::SINGLE_FIRE) {
        if (!ctx.isCalibrated) {
            // 未校准 → 先进入校准流程, 校准完成后继续执行本次单发
            if (ctx.heatController.canShootSingle()) {
                ctx.jamSourceState       = FireState::Passive;
                ctx.targetStateAfterCali = FireState::SingleFire;
                ctx.reversePurpose       = ReversePurpose::SingleFireCalibration;
                request_switch(&instance()._stateCaliReverse);
            }
        } else if (ctx.heatController.canShootSingle()) {
            // 已校准 + 热量允许 → 进入单发
            request_switch(&instance()._stateSingleFire);
        }
        // else: 热量不足, 留在 Ready 忽略本次指令
        return;
    }

    // --- 连发指令 (持续按住) ---
    if (ctx.cmd.state.burstShot) {
        request_switch(&instance()._stateBurstFire);
    }




}
