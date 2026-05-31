/**
 *******************************************************************************
 * @file    fire-state-safe-burst.cpp
 * @brief   火控 FSM — SafeBurst 状态实现
 *
 * 状态角色:
 *   安全连发态 — 位置环逐发推进, 每发前检查热量余量, 适合热量紧张时的受控连发
 *
 * Entry actions:
 *   - 切回位置环模式
 *   - 清零堵转计时器
 *   - 将位置环目标锁定到当前位置
 *
 * Exit condition (transitions OUT):
 *   - FRIC_TOGGLE / EMERGENCY_STOP -> Passive
 *   - SINGLE_FIRE -> SingleFire / CaliReverse (未校准时先校准)
 *   - BURST_STOP / burstShot==0 -> Ready
 *   - 热量不足 -> Ready
 *   - 堵转超时 2000 ms -> CaliReverse (记录来源为 SafeBurst)
 *
 * Context modifications:
 *   - Writes: useTriggerSpeedLoopOnly, blockStartTick,
 *             targetTriggerEcd, jamSourceState, state
 *
 *******************************************************************************
 * @attention
 *
 * SafeBurst 使用位置环逐发推进, 当前编码器沿正方向越过目标后检查热量决定是否继续。
 *
 *******************************************************************************
 * @note
 *
 * 此状态适用于热量逼近警戒线时, 提供更精细的热量控制。
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/3/3
 * @version 2.0
 *******************************************************************************
 */

#include "../fire-ctrl-app.h"

namespace {
using Config::Algorithm::Gimbal::TRIGGER_ECD_CIRCLE;
using Config::Algorithm::Gimbal::TRIGGER_ECD_PER_BULLET;

int32_t wrapTriggerEcd(int32_t ecd) {
    ecd %= TRIGGER_ECD_CIRCLE;
    if (ecd < 0) {
        ecd += TRIGGER_ECD_CIRCLE;
    }
    return ecd;
}

int32_t forwardTriggerEcdDistance(int32_t from, int32_t to) { return wrapTriggerEcd(to - from); }

bool hasPassedTriggerTarget(int32_t currentEcd, int32_t targetEcd) {
    int32_t passedDistance = forwardTriggerEcdDistance(targetEcd, currentEcd);
    return passedDistance > 0 && passedDistance < TRIGGER_ECD_CIRCLE / 2;
}
} // namespace

/**
 * @brief 进入 SafeBurst 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateSafeBurst::enter(FireCtrlCtx& ctx) {
    // --- 初始化 ---
    ctx.useTriggerSpeedLoopOnly = false;
    ctx.blockStartTick          = 0;
    ctx.targetTriggerEcd        = ctx.currentTriggerEcd;

    ctx.state                   = FireState::SafeBurst;
}

/**
 * @brief 执行 SafeBurst 状态逻辑
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateSafeBurst::execute(FireCtrlCtx& ctx) {
    // --- 紧急退出 ---
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE || ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    // --- 停止条件 ---
    if (ctx.transientEvent == ShootEvent::BURST_STOP || ctx.cmd.state.burstShot == 0) {
        request_switch(&instance()._stateReady);
        return;
    }

    // --- 续杯逻辑: 当前循环编码器沿正方向越过目标后, 检查热量决定是否继续 ---
    int32_t currentEcd = ctx.currentTriggerEcd;

    if (hasPassedTriggerTarget(currentEcd, ctx.targetTriggerEcd)) {
        if (ctx.heatController.canShootSingle()) {
            ctx.targetTriggerEcd = wrapTriggerEcd(ctx.targetTriggerEcd + TRIGGER_ECD_PER_BULLET);
        } else {
            return;
        }
    }

    if (ctx.heatController.canShootBurst()) {
        request_switch(&instance()._stateBurstFire);
    }


    // --- 堵转检测 ---
    float targetAngle = (float)(ctx.targetTriggerEcd) / (float)TRIGGER_ECD_CIRCLE * 2.0f * (float)M_PI;
    float realAngle   = (float)(currentEcd) / (float)TRIGGER_ECD_CIRCLE * 2.0f * (float)M_PI;
    float err         = targetAngle - realAngle;
    while (err > (float)M_PI)
        err -= 2.0f * (float)M_PI;
    while (err < -(float)M_PI)
        err += 2.0f * (float)M_PI;

    if (std::abs(err) > (float)M_PI / 16.0f && std::abs(ctx.fdb.trigger.vel) < 10.0f) {
        if (ctx.blockStartTick == 0) {
            ctx.blockStartTick = xTaskGetTickCount();
        } else if (xTaskGetTickCount() - ctx.blockStartTick >= pdMS_TO_TICKS(800)) {
            ctx.jamSourceState = FireState::SafeBurst;
            ctx.reversePurpose = ReversePurpose::JamClear;
            request_switch(&instance()._stateCaliReverse);
            return;
        }
    } else {
        ctx.blockStartTick = 0;
    }
}
