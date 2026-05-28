/**
 *******************************************************************************
 * @file    motion-state-relax.cpp
 * @brief   云台运动 FSM — Relax 状态 (无力模式)
 *
 * Entry actions:
 *   - pitch.disable(), PID 清零, 输出归零
 *
 * Exit condition:
 *   - mode != RELAX -> Align
 *******************************************************************************
 * @author  MekLi
 * @date    2026/5/3
 * @version 1.0
 *******************************************************************************
 */

#include "../movtion-ctrl-app.h"
#include "System/Service/motor-actuator.h"

void MovtionCtrlApp::StateRelax::enter(GimbalMotionCtx& ctx) {
    ctx.motionState = static_cast<uint8_t>(MotionState::Relax);
    ctx.output.targetPitchPos         = ctx.state.pitch.pos;
    ctx.output.targetPitchSpeed       = 0.0f;
    ctx.output.pitchFeedforwardTorque = 0.0f;
    ctx.output.pitchEn                = false;
    ctx.output.yawVoltage             = 0.0f;
    ctx.telem.targetYawRad            = ctx.imu.yaw;
    ctx.telem.targetPitchRad          = ctx.imu.pitch;

    
    instance().yawPosPid.clear();
    instance().yawSpdPid.clear();
    instance().pitchPosPid.clear();
    MotActSrvc::instance().pitch.disable();
}

void MovtionCtrlApp::StateRelax::execute(GimbalMotionCtx& ctx) {
    //应某位舵轮底盘调试者的要求
    #ifdef STEER
    ctx.telem.targetYawRad            = ctx.imu.yaw;
    ctx.telem.targetPitchRad          = ctx.imu.pitch;
    #endif
    if (ctx.cmd.mode != GIMBAL_RELAX)
        request_switch(&instance()._stateAlign);
}
