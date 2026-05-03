/**
 *******************************************************************************
 * @file    motion-state-auto.cpp
 * @brief   云台运动 FSM — Auto 状态 (视觉自瞄)
 *
 * Entry actions:
 *   - pitch.enable()
 *
 * 每 tick:
 *   - 视觉目标覆盖 telem
 *   - updatePitch + updateYaw 闭环
 *
 * Exit condition:
 *   - mode == RELAX -> Relax
 *   - mode != AUTO -> Manual
 *******************************************************************************
 * @author  MekLi
 * @date    2026/5/3
 * @version 1.0
 *******************************************************************************
 */

#include "../movtion-ctrl-app.h"
#include "System/Service/motor-actuator.h"

void MovtionCtrlApp::StateAuto::enter(GimbalMotionCtx& ctx) {
    ctx.motionState = static_cast<uint8_t>(MotionState::Auto);
    ctx.output.pitchEn = true;
    MotActSrvc::instance().pitch.enable();
}

void MovtionCtrlApp::StateAuto::execute(GimbalMotionCtx& ctx) {
    if (ctx.cmd.mode == GIMBAL_RELAX) {
        request_switch(&instance()._stateRelax);
        return;
    }
    if (ctx.cmd.mode != GIMBAL_AUTO) {
        request_switch(&instance()._stateManual);
        return;
    }
    if (std::abs(ctx.cmd.targetYaw) < M_PI)
        ctx.telem.targetYawRad = ctx.cmd.targetYaw;
    if (std::abs(ctx.cmd.targetPitch) < M_PI)
        ctx.telem.targetPitchRad = ctx.cmd.targetPitch;
    instance().updatePitch(ctx);
    instance().updateYaw(ctx);
}
