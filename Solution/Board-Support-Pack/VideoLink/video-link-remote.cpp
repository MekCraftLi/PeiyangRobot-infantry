/**
 *******************************************************************************
 * @file    video-link-remote.cpp
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
 * @date    2026/3/4
 * @version 1.0
 *******************************************************************************
 */


/* ------- define ----------------------------------------------------------------------------------------------------*/





/* ------- include ---------------------------------------------------------------------------------------------------*/

#include "video-link-remote.h"

#include "task.h"
#include "projdefs.h"



/* ------- class prototypes-------------------------------------------------------------------------------------------*/





/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/

// 构造函数：初始化所有原子控件及其硬件参数映射
VideoLinkRemote::VideoLinkRemote()
    : // --- 1. 初始化 11-bit 模拟摇杆与拨轮 (参数依据典型大疆协议) ---
      // Min=364, Max=1684, Center=1024, Deadzone=20
      _axisRightX({364, 1684, 1024, 20, false}),
      _axisRightY({364, 1684, 1024, 20, false}),
      _axisLeftX({364, 1684, 1024, 20, false}),
      _axisLeftY({364, 1684, 1024, 20, false}),
      _wheel({364, 1684, 1024, 20, false}),

      // --- 2. 初始化开关 (离散控制) ---
      // 假设 modeSw 为三档开关：1->Up(0), 3->Mid(1), 2->Down(2)
      _modeSw({{0, 0},{1, 1}, {2, 2}}, 0),

      // 单 Bit 按键，通常为 0->松开(0), 1->按下(1)
      _pause({{0, 0}, {1, 1}}, 0),
      _fn1({{0, 0}, {1, 1}}, 0),
      _fn2({{0, 0}, {1, 1}}, 0),
      _trigger({{0, 0}, {1, 1}}, 0),

      // --- 3. 初始化鼠标映射 ---
      // 鼠标通常以增量形式发送，将其中心设为 0，最大值根据实际情况标定 (假定32768)
      _mouseX({-32768, 32767, 0, 5, false}),
      _mouseY({-32768, 32767, 0, 5, false}),
      _mouseZ({-32768, 32767, 0, 5, false}),

      // 鼠标按键通常为 0->松开，1->按下
      _mouseLeft({{0, 0}, {1, 1}}, 0),
      _mouseRight({{0, 0}, {1, 1}}, 0),
      _mouseMiddle({{0, 0}, {1, 1}}, 0),

      _lastUpdateTick(0)
{}

// 数据更新：将物理帧分配给各个独立控件
void VideoLinkRemote::updateRaw(const VideoLinkRawData& raw) {
    _axisRightX.updateRaw(raw.ch0);
    _axisRightY.updateRaw(raw.ch1);
    _axisLeftX.updateRaw(raw.ch2);
    _axisLeftY.updateRaw(raw.ch3);

    _modeSw.updateRaw(raw.modeSw);
    _pause.updateRaw(raw.pause);
    _fn1.updateRaw(raw.fn1);
    _fn2.updateRaw(raw.fn2);
    _wheel.updateRaw(raw.wheel);
    _trigger.updateRaw(raw.trigger);

    _mouseX.updateRaw(raw.mouseX);
    _mouseY.updateRaw(raw.mouseY);
    _mouseZ.updateRaw(raw.mouseZ);

    _mouseLeft.updateRaw(raw.mouseLeft);
    _mouseRight.updateRaw(raw.mouseRight);
    _mouseMiddle.updateRaw(raw.mouseMiddle);
}

// 统一路由映射 (通过枚举获取对应的控件对象指针)
IInputControl* VideoLinkRemote::getAxis(AxisID id) {
    switch (id) {
        case AxisID::MoveX:
            return &_axisLeftY;   // 左摇杆Y轴控制底盘前后
        case AxisID::MoveY:
            return &_axisLeftX;   // 左摇杆X轴控制底盘左右
        case AxisID::ViewYaw:
            return &_axisRightX;  // 右摇杆X轴控制云台Yaw
        case AxisID::ViewPitch:
            return &_axisRightY;  // 右摇杆Y轴控制云台Pitch
        case AxisID::Aux1:
            return &_wheel;       // 辅助拨轮
        default:
            return nullptr;
    }
}

IInputControl* VideoLinkRemote::getButton(ButtonID id) {
    switch (id) {
        case ButtonID::ModeSwitch:
            return &_modeSw;      // 主模式切换
        case ButtonID::FuncSwitch:
            return &_trigger;     // 发射触发
        case ButtonID::KeyA:
            return &_fn1;         // 自定义功能键 A
        case ButtonID::KeyB:
            return &_fn2;         // 自定义功能键 B
        default:
            return nullptr;
    }
}

bool VideoLinkRemote::isConnected() const {
    // 50ms 超时判定看门狗
    return (xTaskGetTickCount() - _lastUpdateTick) <= pdMS_TO_TICKS(50);
}

void VideoLinkRemote::onDataReceived() {
    _lastUpdateTick = xTaskGetTickCount();
}