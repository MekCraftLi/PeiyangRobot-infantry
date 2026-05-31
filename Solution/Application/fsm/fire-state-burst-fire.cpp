/**
 *******************************************************************************
 * @file    fire-state-burst-fire.cpp
 * @brief   火控 FSM — BurstFire 状态实现
 *
 * 状态角色:
 *   连发态 — 拨弹盘纯速度环连发, 热量不足时停止拨弹
 *
 * Entry actions:
 *   - 清零堵转计时器
 *   - 切换到纯速度环模式
 *
 * Exit actions:
 *   - 清空速度环积分项
 *   - 将位置环目标锁定到当前位置
 *   - 切回位置环模式
 *
 * Exit condition (transitions OUT):
 *   - SINGLE_FIRE -> SingleFire / CaliReverse (未校准时先校准)
 *   - BURST_STOP / burstShot==0 -> Ready
 *   - FRIC_TOGGLE / EMERGENCY_STOP -> Passive
 *   - 堵转超时 2000 ms -> CaliReverse (记录来源为 BurstFire)
 *
 * Context modifications:
 *   - Writes: blockStartTick, useTriggerSpeedLoopOnly,
 *             targetTriggerSpeed, targetTriggerEcd, jamSourceState, state
 *   - Clears: _triggerSpdPid (in exit)
 *
 *******************************************************************************
 * @attention
 *
 * 连发使用纯速度环, 每 tick 根据热量余量决定全速拨弹或停止拨弹。
 *
 *******************************************************************************
 * @note
 *
 * 退出时不再主动推进目标, 避免停火后继续拨弹。
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
    ctx.blockStartTick          = 0;    // 堵转检测起始时刻 (0=未堵转)
    ctx.useTriggerSpeedLoopOnly = true; // 绕过位置环, 仅速度环 (连发/校准)
    ctx.state                   = FireState::BurstFire;
}

/**
 * @brief 执行 BurstFire 状态逻辑
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateBurstFire::execute(FireCtrlCtx& ctx) {


    // --- 紧急退出 ---
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE || ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    if (ctx.cmd.state.burstShot == 0) {
        request_switch(&instance()._stateReady);
        return;
    }


    // --- 热控判断: 单发安全则全速连发, 否则停转等待冷却 ---
    if (ctx.heatController.canShootSingle()) {
        ctx.targetTriggerSpeed = Config::Hardware::MotorTopo::TRIGGER_SPEED;
    } else {
        // request_switch(&instance()._stateSingleFire);
        ctx.targetTriggerSpeed = 0;
        return;
    }



    // targetTriggerSpeed 会在 calculateCurrents() 中应用
    // --- 堵转检测: 目标速度大但实际极低 ---
    float speedErr = std::abs(ctx.targetTriggerSpeed) - std::abs(ctx.fdb.trigger.vel);
    // 功能: 检测拨弹盘是否发生机械卡死
    if (speedErr > 50.0f && std::abs(ctx.fdb.trigger.vel) < 10.0f) {
        if (ctx.blockStartTick == 0) {
            ctx.blockStartTick = xTaskGetTickCount();
        } else if (xTaskGetTickCount() - ctx.blockStartTick >= pdMS_TO_TICKS(800)) {
            ctx.jamSourceState = FireState::BurstFire;
            ctx.reversePurpose = ReversePurpose::JamClear;
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
    ctx.targetTriggerSpeed = 0;
    ctx.useTriggerSpeedLoopOnly = false; // 回到角度环控制
}
