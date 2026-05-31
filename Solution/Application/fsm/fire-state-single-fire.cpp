/**
 *******************************************************************************
 * @file    fire-state-single-fire.cpp
 * @brief   火控 FSM — SingleFire 状态实现
 *
 * 状态角色:
 *   单发态 — 由当前位置通过位置环向前推进一发, 到达后自动回到 Ready
 *
 * Entry actions:
 *   - 清零堵转计时器
 *   - 目标编码器设为当前循环编码器前进一发
 *   - 切回位置环模式
 *
 * Exit condition (transitions OUT):
 *   - FRIC_TOGGLE / EMERGENCY_STOP -> Passive
 *   - 推进一发到位 -> Ready
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

namespace {
using Config::Algorithm::Gimbal::TRIGGER_ECD_CIRCLE;
using Config::Algorithm::Gimbal::TRIGGER_ECD_PER_BULLET;

constexpr int32_t SINGLE_FIRE_REACH_ECD = 3000;

int32_t wrapTriggerEcd(int32_t ecd) {
    ecd %= TRIGGER_ECD_CIRCLE;
    if (ecd < 0) {
        ecd += TRIGGER_ECD_CIRCLE;
    }
    return ecd;
}

int32_t shortestTriggerEcdDistance(int32_t lhs, int32_t rhs) {
    int32_t diff = wrapTriggerEcd(lhs) - wrapTriggerEcd(rhs);
    if (diff < 0) {
        diff = -diff;
    }
    return (diff > TRIGGER_ECD_CIRCLE / 2) ? (TRIGGER_ECD_CIRCLE - diff) : diff;
}
} // namespace

/**
 * @brief 进入 SingleFire 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateSingleFire::enter(FireCtrlCtx& ctx) {
    // --- 从当前位置直接推进一发 ---
    ctx.blockStartTick          = 0;
    if (ctx.heatController.canShootSingle()) {
        static uint8_t debug_singleFire;
        debug_singleFire = !debug_singleFire;
        ctx.targetTriggerEcd        = wrapTriggerEcd(ctx.currentTriggerEcd + TRIGGER_ECD_PER_BULLET);
    }

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
    //这段代码是单发状态中的角度误差计算逻辑，用于精确控制拨弹盘到达目标位置。
    float targetAngle = (float)(ctx.targetTriggerEcd) / (float)TRIGGER_ECD_CIRCLE * 2.0f * (float)M_PI;

    int32_t ecd = ctx.currentTriggerEcd;
    float realAngle = (float)(ecd) / (float)TRIGGER_ECD_CIRCLE * 2.0f * (float)M_PI;

    float err = targetAngle - realAngle;
    while (err >  (float)M_PI) err -= 2.0f * (float)M_PI;
    while (err < -(float)M_PI) err += 2.0f * (float)M_PI;

    // --- 堵转检测 ---
    if (std::abs(err) > (float)M_PI / 16.0f && std::abs(ctx.fdb.trigger.vel) < 10.0f) {
        if (ctx.blockStartTick == 0) {
            ctx.blockStartTick = xTaskGetTickCount();
        } else if (xTaskGetTickCount() - ctx.blockStartTick >= pdMS_TO_TICKS(800)) {
            ctx.isCalibrated   = false;
            ctx.jamSourceState = FireState::SingleFire;
            ctx.reversePurpose = ReversePurpose::JamClear;
            request_switch(&instance()._stateCaliReverse);
            return;
        }
    } else {
        ctx.blockStartTick = 0;
    }

    // --- 到达目标 ---
    if (shortestTriggerEcdDistance(ctx.currentTriggerEcd, ctx.targetTriggerEcd) < SINGLE_FIRE_REACH_ECD) {
        request_switch(&instance()._stateReady);
    }
}
