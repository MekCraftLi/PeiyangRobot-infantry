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
#include "System/DataHub/blackboard.h"




void MovtionCtrlApp::StateManual::enter(GimbalMotionCtx& ctx) {
    ctx.motionState = static_cast<uint8_t>(MotionState::Manual);
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
    #if defined(DOG_1) || defined(DOG_2)
    ChassisToGimbalComm c2gData{};
    Blackboard::instance().c2gComm.read(c2gData);
    if(!c2gData.msg.chassisready)
    {
        if (ctx.cmd.mode == GIMBAL_RELAX) 
        {
            request_switch(&instance()._stateRelax);
            return;
        }
        ctx.telem.targetPitchRad = PITCH_ALIGN_TARGET_RAD+0.2f;
        int32_t curEcd = MotActSrvc::instance().yaw.get_current_ecd();
        //角度归一化
        float diff = (_YAW_OFFSET - curEcd) % 8192;
        while (diff > 4096) diff -= 8192;
        while (diff < -4096) diff += 8192;
        //转化弧度制
        diff = (float)diff / 8192.0f * 2.0f * M_PI;
        ctx.telem.targetYawRad= ctx.imu.yaw;
        //计算
        float yawSpdCmd     = instance()._alignPosPid.calculate(0.0f, -diff);
        ctx.output.yawVoltage = instance()._alignSpdPid.calculate(yawSpdCmd, ctx.imu.gyro[2]);
        //等待底盘准备就绪

        ctx.telem.targetPitchRad = PITCH_LIMIT_MIN-0.3f; 
        instance().updatePitch(ctx);
        
        return;
    }
    #endif
    
    instance().updatePitch(ctx);
    instance().updateYaw(ctx);
}
