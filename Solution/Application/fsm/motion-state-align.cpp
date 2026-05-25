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
static constexpr float   ALIGN_TOLERANCE  = 15.0f * M_PI / 180.0f;
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
    ctx.output.pitchEn    = false;
    ctx.output.yawVoltage = 0.0f;
    MotActSrvc::instance().pitch.disable();
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

    // 编码器差值 → 弧度 (与 IMU 无关)
    int32_t curEcd = MotActSrvc::instance().yaw.get_current_ecd();
    float errorRad = (float)ecdShortestError(ALIGN_TARGET_ECD, curEcd)
                     / (float)ECD_PER_REV * 2.0f * M_PI;

    // 编码器速度 10Hz 一阶低通滤波: alpha = 2π*fc*dt / (2π*fc*dt + 1)
    constexpr float TWO_PI_FC = 2.0f * M_PI * 10.0f;  // 314.16 rad/s
    float alpha = TWO_PI_FC * ctx.dt / (TWO_PI_FC * ctx.dt + 1.0f);
    instance()._alignVelFilt = alpha * ctx.state.yaw.vel + (1.0f - alpha) * instance()._alignVelFilt;

    // 位置环: 编码器误差 → 速度指令
    // 速度环: 编码器速度经 50Hz LPF 后作为反馈
    float yawSpdCmd     = instance()._alignPosPid.calculate(0.0f, -errorRad);
    ctx.output.yawVoltage = instance()._alignSpdPid.calculate(yawSpdCmd, instance()._alignVelFilt);
    ctx.output.pitchEn    = false;

    // 误差 < ±5° → 累计稳定时间
    if (std::abs(errorRad) < ALIGN_TOLERANCE) {
        instance()._alignStableMs += ctx.dt * 1000.0f;
        if (instance()._alignStableMs >= ALIGN_STABLE_MS) {
            ctx.telem.targetYawRad = ctx.imu.yaw;
            //只有接收到底盘发到的底盘准备标志位才可以开始切换状态
            ChassisToGimbalComm cmd;
            Blackboard::instance().c2gComm.read(cmd);
            #ifdef STEER
             request_switch(&instance()._stateManual);
            #endif
            #if defined(DOG_1)||defined(DOG_2)
            if (cmd.msg.chassisready) 
            {
                request_switch(&instance()._stateManual);
            }
            #endif
        }
    } else {
        instance()._alignStableMs = 0.0f;
    }
}
