/**
 *******************************************************************************
 * @file    fire-state-spin-up.cpp
 * @brief   火控 FSM — SpinUp 状态实现
 *
 * 状态角色:
 *   摩擦轮启动态 — 设定摩擦轮目标转速, 等待双轮达到目标后进入 Ready
 *
 * Entry actions:
 *   - 设定摩擦轮目标转速 (来自硬件配置)
 *   - 拨弹盘切回位置环模式 (为 Ready 做准备)
 *
 * Exit condition (transitions OUT):
 *   - FRIC_TOGGLE / EMERGENCY_STOP -> Passive
 *   - 双摩擦轮速度均接近目标 -> Ready
 *
 * Context modifications:
 *   - Writes: targetFricSpeed, useTriggerSpeedLoopOnly, state
 *
 *******************************************************************************
 * @attention
 *
 * 双摩擦轮需要均达到目标转速的 ±0.1 rad/s 范围内才判定启动完成。
 *
 *******************************************************************************
 * @note
 *
 * 左摩擦轮正向旋转, 右摩擦轮反向旋转 (反向安装)。
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
 * @brief 进入 SpinUp 状态
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateSpinUp::enter(FireCtrlCtx& ctx) {
    // --- 设定摩擦轮目标 ---
    ctx.targetFricSpeed         = Config::Hardware::MotorTopo::FRIC_TARGET_SPEED;
    ctx.useTriggerSpeedLoopOnly = false;
    ctx.state                   = FireState::SpinUp;
}

/**
 * @brief 执行 SpinUp 状态逻辑
 * @param ctx FSM 上下文引用
 */
void FireCtrlApp::StateSpinUp::execute(FireCtrlCtx& ctx) {
    // --- 紧急退出 ---
    if (ctx.transientEvent == ShootEvent::FRIC_TOGGLE ||
        ctx.transientEvent == ShootEvent::EMERGENCY_STOP) {
        request_switch(&instance()._statePassive);
        return;
    }

    // --- 启动完成检测: 双摩擦轮速度均接近目标 ---
    if (std::abs(ctx.fdb.fric[0].vel - ctx.targetFricSpeed) < 10 &&
        std::abs(ctx.fdb.fric[1].vel + ctx.targetFricSpeed) < 10) {
        request_switch(&instance()._stateReady);
    }
}