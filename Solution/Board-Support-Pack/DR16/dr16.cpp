/**
 *******************************************************************************
 * @file    dr16.cpp
 * @brief   简要描述
 *******************************************************************************
 * @attention
 *
 * none
 *
 *******************************************************************************
 * @note
 *
 * none
 *
 *******************************************************************************
 * @author  MekLi
 * @date    2026/2/9
 * @version 1.0
 *******************************************************************************
 */


/* ------- define ----------------------------------------------------------------------------------------------------*/





/* ------- include ---------------------------------------------------------------------------------------------------*/

#include "dr16.h"

#include "System/Input/action.h"
#include "System/Service/commander.h"
#include "task.h"

#include <cstdint>




/* ------- class prototypes-------------------------------------------------------------------------------------------*/





/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/



RemoteDR16::RemoteDR16()
    : // --- A. 初始化摇杆 (插入控件) ---
      // 这里的参数对应 DR16 的硬件特性: Min=364, Max=1684, Center=1024, Deadzone=20
      _axislx({364, 1684, 1024, 20, false}), _axisly({364, 1684, 1024, 20, false}),
      _axisrx({364, 1684, 1024, 20, false}), _axisry({364, 1684, 1024, 20, false}),
      _wheel({364, 1684, 1024, 20, false}),
      // --- B. 初始化开关 (插入控件) ---
      // 映射表: 硬件值 1->Up, 3->Mid, 2->Down
      _swleft({{1, 0}, {3, 1}, {2, 2}}, 2),         // 默认 Dwn
      _swright({{1, 0}, {3, 1}, {0, 2}, {2, 2}}, 2) // 默认Dwn
{}

void RemoteDR16::updateRaw(const Dr16Data& raw) {
    _axislx.updateRaw(raw.ch2);
    _axisly.updateRaw(raw.ch3);
    _axisrx.updateRaw(raw.ch0);
    _axisry.updateRaw(raw.ch1);
    _swleft.updateRaw(raw.s1);
    _swright.updateRaw(raw.s2);
    _wheel.updateRaw(raw.wheel);
}

bool RemoteDR16::isConnected() const {
    return (xTaskGetTickCount() - _lastUpdateTick) <= pdMS_TO_TICKS(50);
}

void RemoteDR16::onDataReceived() {
    _lastUpdateTick = xTaskGetTickCount();
}

void RemoteDR16::bindActions(InputAction* actions, TriggerConfig& triggers) {
    //
    //  DR16 遥控器布局:
    //    左摇杆 Y → 前后 (MoveX)      左摇杆 X → 左右 (MoveY)
    //    右摇杆 X → 偏航 (Yaw)        右摇杆 Y → 俯仰 (Pitch)
    //    右开关   → 控制源仲裁         左开关   → 射击系统
    //    拨轮     → 小陀螺切换
    //
    // 底盘运动
    actions[MOVE_X].bind(getLeftY(), &triggers.joystickDeadzone);
    actions[MOVE_Y].bind(getLeftX(), &triggers.joystickDeadzone);
    // 云台控制
    actions[YAW].bind(getRightX(), &triggers.joystickDeadzone);
    actions[PITCH].bind(getRightY(), &triggers.joystickDeadzone);
    // 控制源仲裁 (模式开关, 无触发器 → 直接值比较)
    actions[CTRL_MODE].bind(getSwRight());
    // 射击: 左开关 → 摩擦轮 / 连发 / 单发
    actions[FRIC_TOGGLE].bind(getSwLeft(), &triggers.fricToggle);
    actions[SHOOT_BURST].bind(getSwLeft(), &triggers.burstFire);
    actions[SHOOT_SINGLE].bind(getSwLeft(), &triggers.singleRelease);
    // 小陀螺: 拨轮 toggle
    actions[SPIN_MODE].bind(getWheel(), &triggers.spinToggle);
}
