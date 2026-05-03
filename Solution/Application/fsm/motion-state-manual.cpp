/**
 *******************************************************************************
 * @file    motion-state-manual.cpp
 * @brief   云台运动 FSM — Manual 状态 (手动遥控)
 *
 * Entry actions:
 *   - pitch.enable()
 *
 * 每 tick:
 *   - updatePitch + updateYaw 闭环
 *
 * Exit condition:
 *   - mode == RELAX -> Relax
 *   - mode == AUTO -> Auto
 *******************************************************************************
 * @author  MekLi
 * @date    2026/5/3
 * @version 1.0
 *******************************************************************************
 */

#include "../movtion-ctrl-app.h"
#include "System/Service/motor-actuator.h"

void MovtionCtrlApp::StateManual::enter(GimbalMotionCtx& ctx) {
    ctx.output.pitchEn = true;
    MotActSrvc::instance().pitch.enable();
}

void MovtionCtrlApp::StateManual::execute(GimbalMotionCtx& ctx) {
    if (ctx.cmd.mode == GIMBAL_RELAX) {
        request_switch(&instance()._stateRelax);
        return;
    }
    if (ctx.cmd.mode == GIMBAL_AUTO) {
        request_switch(&instance()._stateAuto);
        return;
    }
    instance().updatePitch(ctx);
    instance().updateYaw(ctx);
}
