/**
 *******************************************************************************
 * @file    motion-state-align.cpp
 * @brief   云台运动 FSM — Align 状态 (yaw 对准编码器 0x5400)
 *
 * Entry actions:
 *   - 计算到目标角度的最短路径偏移
 *   - pitch.disable(), PID 清零
 *
 * 每 tick:
 *   - 累积 yaw 变化量 (跨 ±π 边界连续)
 *   - P + 速度环闭环驱动 yaw 对准
 *
 * Exit condition:
 *   - |offset| < ±5° -> Manual
 *   - mode == RELAX -> Relax
 *******************************************************************************
 * @author  MekLi
 * @date    2026/5/3
 * @version 1.0
 *******************************************************************************
 */

#include "../movtion-ctrl-app.h"
#include "System/Service/motor-actuator.h"
#include "System/DataHub/blackboard.h"

static constexpr int32_t ALIGN_TARGET_ECD = _YAW_OFFSET; 
static constexpr int32_t ECD_PER_REV      = 8192;
static constexpr float   ALIGN_TOLERANCE  = 5.0f * M_PI / 180.0f;
static constexpr float   ALIGN_STABLE_MS  = 800.0f;

// 编码器最短路径 (mod 8192)
static int32_t ecdShortestError(int32_t target, int32_t current) {
    int32_t diff = (target - current) % ECD_PER_REV;
    if (diff > ECD_PER_REV / 2) diff -= ECD_PER_REV;
    if (diff < -ECD_PER_REV / 2) diff += ECD_PER_REV;
    return diff;
}

void MovtionCtrlApp::StateAlign::enter(GimbalMotionCtx& ctx) {
    ctx.motionState = static_cast<uint8_t>(MotionState::Align);
    instance()._alignStableMs = 0.0f;
    instance()._alignVelFilt  = 0.0f;
    instance()._alignPosPid.clear();
    instance()._alignSpdPid.clear();
    ctx.output.pitchEn    = true;
    ctx.output.yawVoltage = 0.0f;
    MotActSrvc::instance().pitch.enable();
}

void MovtionCtrlApp::StateAlign::execute(GimbalMotionCtx& ctx) {
    if (ctx.cmd.mode == GIMBAL_RELAX) {
        request_switch(&instance()._stateRelax);
        return;
    }

    #ifdef STEER
    request_switch(&instance()._stateManual);
    return;
    #endif

    #if defined(DOG_1) || defined(DOG_2)

    //使能pitch
    ctx.output.pitchEn    = true;
    ctx.telem.targetPitchRad = PITCH_LIMIT_MIN+0.2f; 
    instance().updatePitch(ctx);


    //对准过程要让yaw稍微抬升一下并保持那个角度
    // 编码器差值 → 弧度 (与 IMU 无关)
    int32_t curEcd = MotActSrvc::instance().yaw.get_current_ecd();
    float errorRad = (float)ecdShortestError(ALIGN_TARGET_ECD, curEcd)
                     / (float)ECD_PER_REV * 2.0f * M_PI;

    if(ctx.state.pitch.pos<PITCH_LIMIT_MIN+0.3f)
    {
        
        //检测yaw轴是否发生堵转，如果发生堵转，选择另外一个方向转到目标位置
        static uint32_t yawblockStartTick = 0;
        if (std::abs(errorRad) > (float)M_PI / 16.0f && std::abs(ctx.imu.gyro[2]) < 10.0f) 
        {
            if (yawblockStartTick == 0) 
            {
                yawblockStartTick = xTaskGetTickCount();
            } 
            else if (xTaskGetTickCount() - yawblockStartTick >= pdMS_TO_TICKS(1000)) 
            {
                //选择另外一个方向转到目标位置
                instance()._alignPosPid.clear();
                instance()._alignSpdPid.clear();
                float yawSpdCmd     = instance()._alignPosPid.calculate(0.0f, -errorRad-2.0f * M_PI);
                ctx.output.yawVoltage = instance()._alignSpdPid.calculate(yawSpdCmd, ctx.imu.gyro[2]);
                
                return;
            }
        } 
        else 
        {
            yawblockStartTick = 0;
            
            float yawSpdCmd     = instance()._alignPosPid.calculate(0.0f, 2.0f * -errorRad);
            ctx.output.yawVoltage = instance()._alignSpdPid.calculate(yawSpdCmd, ctx.imu.gyro[2]);
        }
    }

    // 误差 < ±5° → 累计稳定时间
    if (std::abs(errorRad) < ALIGN_TOLERANCE) {
        instance()._alignStableMs += ctx.dt * 1000.0f;
        if (instance()._alignStableMs >= ALIGN_STABLE_MS) {
            ctx.telem.targetYawRad = ctx.imu.yaw;       
            request_switch(&instance()._stateManual);
            
        }
    } else {
        instance()._alignStableMs = 0.0f;
    }
    #endif
}
