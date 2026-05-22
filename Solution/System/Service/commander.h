/**
 *******************************************************************************
 * @file    commander.h
 * @brief   输入指挥服务 — 桥接物理控件 → 触发器 → Action → 黑板指令
 *
 * 架构层次 (4 层输入管线):
 *
 *   Layer I:   IInputControl   物理控件 (摇杆/开关/鼠标/键盘)
 *                                 ↓ get() 归一化值 [-1.0, 1.0]
 *   Layer II:  InputTrigger    触发器 (死区/保持/边沿/点击/切换)
 *                                 ↓ update(value, dt) → TriggerState
 *   Layer III: InputAction     绑定层 (控件 + 可选触发器)
 *                                 ↓ isTriggered() / getValue()
 *   Layer IV:  CommanderSrvc   仲裁器 (控制源裁决 → 指令填充 → 写黑板)
 *
 * 数据流:
 *   Remote → CommanderSrvc::run()
 *          → Blackboard (gimbalCmd / shootCmd / g2cOutput)
 *
 * 控件映射 (编译时由 REMOTE_DEVICE 宏选择):
 *   DR16:       左右摇杆 + 左右开关 + 拨轮
 *   VideoLink:  摇杆 + 鼠标 + 键盘 + 扳机
 *   Gamepad:    扳机 + 摇杆 + 按键
 *******************************************************************************
 * @author  MekLi
 * @date    2026/2/27
 * @version 3.0
 *******************************************************************************
 */

#ifndef INFANTRY_COMMANDER_H
#define INFANTRY_COMMANDER_H

/*-------- includes
 * ---------------------------------------------------------------------------------------------------*/

#ifdef __cplusplus

#include "../DataHub/blackboard.h"
#include "System/Thread/application-base.h"
#include "tools/crtp.h"

#include "System/Input/action.h"
#include "System/Input/trigger-config.h"

#include "Board-Support-Pack/DR16/dr16.h"
#include "System/Input/ControlImpl/control-impl-axis.h"
#include "System/Input/ControlImpl/control-impl-switch.h"

#ifdef GIMBAL
#if REMOTE_DEVICE == REMOTE_VIDEO_LINK
#include "Board-Support-Pack/VideoLink/video-link-remote.h"
#elif REMOTE_DEVICE == REMOTE_GAMEPAD
#include "Board-Support-Pack/GamePad/bluetooth-gamepad.h"
#endif
#endif

#include "usart.h"

/*-------- Debug: 全局 Action 触发状态 (供调试器 live-watch) --------------------------------*/

struct ActionDebug {
    // 标准 Action (DR16 / VideoLink)
    bool ctrlMode;
    bool moveX;
    bool moveY;
    bool moveXKey;
    bool moveYKey;
    bool yaw;
    bool pitch;
    bool mouseYaw;
    bool mousePitch;
    bool mouseBurst;
    bool mouseSingle;
    bool mouseVision;
    bool fricToggle;
    bool shootBurst;
    bool shootSingle;
    bool spinMode;
    bool keySpin;
    bool capSwitch;
    bool keyboardFric;
    bool fn1Switch;
    bool turboMode;
    bool stepClimb;
    bool selfRescue;
    bool manualRescue;
    bool gimbalReverse;
    bool jump;
    bool aimMode;
    bool legLength;
    bool reverseEdge;

    // Gamepad
    bool gpRelax;
    bool gpHandbrake;
    bool gpMoveX;
    bool gpYaw;
    bool gpBrake;
};

/*-------- 标准 Action 槽位 (DR16 / VideoLink 共用) -----------------------------------------*/

enum StdActionSlot : uint8_t {
    CTRL_MODE,     // 控制源切换开关
    MOVE_X,        // 底盘前后平移 (摇杆/扳机)
    MOVE_Y,        // 底盘左右平移
    MOVE_X_KEY,    // 底盘前后平移 (键盘 W/S)
    MOVE_Y_KEY,    // 底盘左右平移 (键盘 A/D)
    YAW,           // 云台偏航 (摇杆)
    PITCH,         // 云台俯仰 (摇杆)
    MOUSE_YAW,     // 云台偏航 (鼠标 X)
    MOUSE_PITCH,   // 云台俯仰 (鼠标 Y)
    MOUSE_BURST,   // 鼠标左键连发
    MOUSE_SINGLE,  // 鼠标左键单发
    MOUSE_VISION,  // 鼠标右键视觉瞄准
    FRIC_TOGGLE,   // 摩擦轮开关切换
    SHOOT_BURST,   // 连发触发
    SHOOT_SINGLE,  // 单发触发
    SPIN_MODE,     // 小陀螺模式切换
    KEY_SPIN,      // 键盘 Shift 旋转
    CAP_SWITCH,    // 超级电容开关
    KEYBOARD_FRIC, // 键盘 Q 摩擦轮切换
    FN1_SWITCH,    // Fn1 开关 (过0上升沿 toggle)
    TURBO_MODE,    // 极速模式 [R]
    STEP_CLIMB,    // 上台阶 [E]
    SELF_RESCUE,   // 自救 [G]
    MANUAL_RESCUE, // 手动自救 [Ctrl]
    GIMBAL_REVERSE,// 云台反向 [X]
    JUMP,          // 跳跃 [V]
    AIM_MODE,      // 自瞄模式切换 [B]
    LEG_LENGTH,    // 腿长切换 [Z] (三档循环)
    REVERSE_EDGE,  // 调头脉冲 [X] (上升沿)
    STD_ACTION_COUNT
};

/*-------- 触发器配置 (供 Remote::bindActions 使用) -----------------------------------------*/

struct TriggerConfig {
    // 通用
    TriggerLinear joystickDeadzone{TriggerCfg::JOYSTICK_DEADZONE};
    // 射击系统
    TriggerEdge fricToggleSw{TriggerCfg::BTN_THRESHOLD, EdgeType::Rising};
    TriggerEdge fricToggleFn2{TriggerCfg::BTN_THRESHOLD, EdgeType::Rising};
    TriggerEdge fricToggleKeyQ{TriggerCfg::BTN_THRESHOLD, EdgeType::Rising};
    TriggerHold burstFire{TriggerCfg::BTN_THRESHOLD, TriggerCfg::BURST_HOLD_TIME, false,
                          HoldCondition::GreaterOrEqual};
    TriggerHold mouseBurstFire{TriggerCfg::BTN_THRESHOLD, TriggerCfg::BURST_HOLD_TIME, false,
                          HoldCondition::GreaterOrEqual};
    TriggerEdge singleReleaseSw{TriggerCfg::BTN_THRESHOLD, EdgeType::Falling};
    TriggerEdge singleReleaseMouse{TriggerCfg::BTN_THRESHOLD, EdgeType::Falling};
    TriggerEdge singleReleaseTrigger{TriggerCfg::BTN_THRESHOLD, EdgeType::Falling};
    TriggerEdge visionSingleShotRise{TriggerCfg::BTN_THRESHOLD, EdgeType::Rising};
    TriggerHold visionAim{TriggerCfg::BTN_THRESHOLD, TriggerCfg::INSTANT_HOLD_TIME, false,
                          HoldCondition::GreaterOrEqual};
    // 运动/切换
    TriggerHold instantTrigger{TriggerCfg::BTN_THRESHOLD, TriggerCfg::INSTANT_HOLD_TIME, true,
                               HoldCondition::GreaterOrEqual};
    TriggerHold shiftHold{TriggerCfg::BTN_THRESHOLD, TriggerCfg::INSTANT_HOLD_TIME, true,
                          HoldCondition::GreaterOrEqual};
    // Toggle 装饰器
    TriggerToggle spinToggle{instantTrigger, false};
    TriggerToggle spinKeyToggle{shiftHold, false};
    TriggerEdge fn1Rise{TriggerCfg::BTN_THRESHOLD, EdgeType::Rising};
    TriggerToggle fn1Toggle{fn1Rise, false};
    // vt03遥控器单点触发 (电容)
    TriggerToggle capToggle{fn1Rise, false};
    //键盘C键长按触发（电容）
    TriggerHold continuousTrigger{TriggerCfg::BTN_THRESHOLD, TriggerCfg::INSTANT_HOLD_TIME, false,
                                  HoldCondition::GreaterOrEqual};
    // 按键 Toggle (各自独立上升沿基座)
    TriggerEdge turboRise{TriggerCfg::BTN_THRESHOLD, EdgeType::Rising};
    TriggerToggle turboToggle{turboRise, false};
    TriggerEdge stepClimbRise{TriggerCfg::BTN_THRESHOLD, EdgeType::Rising};
    TriggerToggle stepClimbToggle{stepClimbRise, false};
    TriggerEdge selfRescueRise{TriggerCfg::BTN_THRESHOLD, EdgeType::Rising};
    TriggerToggle selfRescueToggle{selfRescueRise, false};
    TriggerEdge manualRescueRise{TriggerCfg::BTN_THRESHOLD, EdgeType::Rising};
    TriggerToggle manualRescueToggle{manualRescueRise, false};
    TriggerEdge gimbalReverseRise{TriggerCfg::BTN_THRESHOLD, EdgeType::Rising};
    TriggerToggle gimbalReverseToggle{gimbalReverseRise, false};
    TriggerEdge jumpEdge{0.5f, EdgeType::Rising};
    // 按键 Cycle (多值循环)
    TriggerEdge aimModeRise{TriggerCfg::BTN_THRESHOLD, EdgeType::Rising};
    TriggerCycle aimModeCycle{aimModeRise, 4};       // [B] 车辆/前哨站/大能量/小能量
    TriggerEdge legLengthRise{TriggerCfg::BTN_THRESHOLD, EdgeType::Rising};
    TriggerCycle legLengthCycle{legLengthRise, 3};    // [Z] 三种腿长
    TriggerEdge reverseEdge{0.0f, EdgeType::Rising};  // [X] 调头脉冲
};

/*-------- class
 * ------------------------------------------------------------------------------------------------------*/

class CommanderSrvc final : public PeriodicApp, public Singleton<CommanderSrvc> {
  public:
    CommanderSrvc();

    void init() override;
    void run() override;

#ifdef GIMBAL
    void onUartRxEventCallback(size_t size);
    void onUartErrCallback();
#endif

    TriggerConfig triggers;
    ActionDebug debug{};

#if REMOTE_DEVICE != REMOTE_GAMEPAD || defined(CHASSIS)

    InputAction _actions[STD_ACTION_COUNT];

    // --- 便捷别名 (指向 _actions 槽位) ---
    InputAction& actionCtrlMode     = _actions[CTRL_MODE];
    InputAction& actionMoveX        = _actions[MOVE_X];
    InputAction& actionMoveY        = _actions[MOVE_Y];
    InputAction& actionMoveXKey     = _actions[MOVE_X_KEY];
    InputAction& actionMoveYKey     = _actions[MOVE_Y_KEY];
    InputAction& actionYaw          = _actions[YAW];
    InputAction& actionPitch        = _actions[PITCH];
    InputAction& actionMouseYaw     = _actions[MOUSE_YAW];
    InputAction& actionMousePitch   = _actions[MOUSE_PITCH];
    InputAction& actionMouseBurst   = _actions[MOUSE_BURST];
    InputAction& actionMouseSingle  = _actions[MOUSE_SINGLE];
    InputAction& actionMouseVision  = _actions[MOUSE_VISION];
    InputAction& actionFricToggle   = _actions[FRIC_TOGGLE];
    InputAction& actionShootBurst   = _actions[SHOOT_BURST];
    InputAction& actionShootSingle  = _actions[SHOOT_SINGLE];
    InputAction& actionSpinMode     = _actions[SPIN_MODE];
    InputAction& actionKeySpin      = _actions[KEY_SPIN];
    InputAction& actionCapSwitch    = _actions[CAP_SWITCH];
    InputAction& actionKeyboardFric = _actions[KEYBOARD_FRIC];
    InputAction& actionFn1Switch    = _actions[FN1_SWITCH];
    InputAction& actionTurboMode    = _actions[TURBO_MODE];
    InputAction& actionStepClimb    = _actions[STEP_CLIMB];
    InputAction& actionSelfRescue   = _actions[SELF_RESCUE];
    InputAction& actionManualRescue = _actions[MANUAL_RESCUE];
    InputAction& actionGimbalReverse= _actions[GIMBAL_REVERSE];
    InputAction& actionJump         = _actions[JUMP];
    InputAction& actionAimMode      = _actions[AIM_MODE];
    InputAction& actionLegLength    = _actions[LEG_LENGTH];
    InputAction& actionReverseEdge  = _actions[REVERSE_EDGE];

#ifdef GIMBAL
    // Vision fireCommand edge detector: 0 -> 1 triggers single-shot event when isSingleShot=1.
    ControlSwitch _visionFireControl{{{0, -1.0f}, {1, 1.0f}}, 0};


    InputAction actionVisionSingle;
#endif

#else
    // ── Gamepad 专用 ──────────────────────────────────────────────
    InputAction _gpActions[5];

    enum GpSlot : uint8_t { GP_RELAX, GP_HANDBRAKE, GP_MOVE_X, GP_YAW, GP_BRAKE, GP_COUNT };

    TriggerLinear _joystickDeadzone{TriggerCfg::JOYSTICK_DEADZONE};
    TriggerHold _handbreak{0.0f, 0.001f, false, HoldCondition::GreaterOrEqual};
    TriggerHold _aTest{0.0f, 0.001f, true, HoldCondition::GreaterOrEqual};
    TriggerToggle _relax{_aTest, true};

    InputAction& actionRelax          = _gpActions[GP_RELAX];
    InputAction& actionHandbrakeDepth = _gpActions[GP_HANDBRAKE];
    InputAction& actionMoveX          = _gpActions[GP_MOVE_X];
    InputAction& actionYaw            = _gpActions[GP_YAW];
    InputAction& actionBreak          = _gpActions[GP_BRAKE];
#endif

    // --- 私有辅助方法 ---
    void resolveChassisMode(bool spinRequested, GimbalToChassisComm& comm);
    void resolveMovement(GimbalCmd& gCmd, GimbalToChassisComm& comm);
    void resolveShootEvents(ShootCmd& sCmd, bool fireAllowed);

};

#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
