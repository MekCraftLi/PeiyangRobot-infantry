/**
 *******************************************************************************
 * @file    fire-state-cali-reverse.cpp
 * @brief   火控 FSM — CaliReverse 状态实现
 *
 * 状态角色:
 *   校准反转态 — 拨弹盘高速反转, 直到碰到机械死区 (堵转) 后切换到 CaliForward
 *
 * Entry actions:
 *   - 清零堵转计时器
 *   - 切换到纯速度环模式
 *
 * Exit actions:
 *   - 清空速度环积分项 (避免残留积分驱动电机)
 *   - 记录当前编码器位置为零点偏移 (triggerOffset)
 *
 * Exit condition (transitions OUT):
 *   - FRIC_TOGGLE / EMERGENCY_STOP -> Passive
 *   - 堵转检测超时 700 ms (速度误差 > 50%) -> CaliForward
 *
 * Context modifications:
 *   - Writes: blockStartTick, useTriggerSpeedLoopOnly, targetTriggerSpeed,
 *             triggerOffset, jamSourceState, targetStateAfterCali, state
 *   - Clears: _triggerSpdPid (in exit)
 *
 *******************************************************************************
 * @attention
 *
 * 堵转判定使用速度误差百分比: 当实际速度偏离目标超过 50% 时开始计时。
 *
 *******************************************************************************
 * @note
 *
 * 校准流程: CaliReverse (反转堵转) -> CaliForward (正转回零) -> Ready/SingleFire
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
 * @brief 根据堵转来源状态决定校准完成后进入的目标状态
 * @param ctx FSM 上下文引用
 */
static void decideTargetStateAfterCali(FireCtrlApp::FireCtrlCtx& ctx) {
    switch (ctx.jamSourceState) {
        case FireCtrlApp::FireState::SingleFire:
            // 单发堵转 → 回到 Ready 等待下次指令
            ctx.targetStateAfterCali = FireCtrlApp::FireState::Ready;
            break;

        case FireCtrlApp::FireState::BurstFire:
            // 连发堵转 → 根据热量恢复
            if (ctx.heatController.isApproachingHeatLimit()) {
                if (ctx.heatController.canShootSingle()) {
                    ctx.targetStateAfterCali = FireCtrlApp::FireState::SafeBurst;
                } else {
                    ctx.targetStateAfterCali = FireCtrlApp::FireState::Ready;
                }
            } else {
                ctx.targetStateAfterCali = FireCtrlApp::FireState::BurstFire;
            }
            break;

        case FireCtrlApp::FireState::SafeBurst:
            // 安全连发堵转 → 根据热量恢复
            if (ctx.heatController.canShootSingle()) {
                ctx.targetStateAfterCali = FireCtrlApp::FireState::SafeBurst;
            } else {
                ctx.targetStateAfterCali = FireCtrlApp::FireState::Ready;
            }
            break;

        default:
            // 首次校准 → 回到就绪
            ctx.targetStateAfterCali = FireCtrlApp::FireState::Ready;
            break;
    }
}

/**
 * @brief 进入 CaliReverse 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateCaliReverse::enter(FireCtrlCtx& ctx) {
    // --- 初始化 ---
    ctx.blockStartTick          = 0;
    ctx.useTriggerSpeedLoopOnly = true;
    ctx.state                   = FireState::CaliReverse;
}

/**
 * @brief 执行 CaliReverse 状态逻辑
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateCaliReverse::execute(FireCtrlCtx& ctx) {
    // --- 高速反转寻找机械死区 ---
    ctx.targetTriggerSpeed = -Config::Hardware::MotorTopo::TRIGGER_SPEED;

    // --- 紧急退出 ---
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE ||
        ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    // --- 堵转检测: 实际速度远小于目标 → 碰到机械限位 ---
    if (std::abs(ctx.fdb.trigger.vel - ctx.targetTriggerSpeed) > std::abs(ctx.targetTriggerSpeed) * 0.5f) {
        if (ctx.blockStartTick == 0) {
            ctx.blockStartTick = xTaskGetTickCount();
        } else if (xTaskGetTickCount() - ctx.blockStartTick >= pdMS_TO_TICKS(700)) {
            decideTargetStateAfterCali(ctx);
            request_switch(&instance()._stateCaliForward);
        }
    } else {
        ctx.blockStartTick = 0;
    }
}

/**
 * @brief 退出 CaliReverse 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateCaliReverse::exit(FireCtrlCtx& ctx) {
    // --- 清空速度环积分, 防止残留积分继续驱动电机 ---
    instance()._triggerSpdPid.clear();

    // --- 以当前编码器位置作为新的零点 ---
    ctx.triggerOffset = ctx.rawTriggerEcd;
}