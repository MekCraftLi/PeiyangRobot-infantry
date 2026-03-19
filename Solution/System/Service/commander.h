/**
 *******************************************************************************
 * @file    commander.h
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
 * @date    2026/2/27
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_CHASSIS_INPUT_H_H
#define INFANTRY_CHASSIS_INPUT_H_H




/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#ifdef __cplusplus

/* I. interface */

#include "../DataHub/blackboard.h"
#include "System/Thread/application-base.h"
#include "tools/crtp.h"
/* II. OS */


/* III. middlewares */
#include "Board-Support-Pack/DR16/dr16.h"
#include "System/Input/ControlImpl/control-impl-axis.h"
#include "System/Input/ControlImpl/control-impl-switch.h"
#include "System/Input/TriggerImpl/trigger-impl-hold.h"
#include "System/Input/TriggerImpl/trigger-impl-linear.h"
#include "System/Input/TriggerImpl/trigger-impl-match.h"
#include "System/Input/action.h"


/* IV. drivers */
#ifdef GIMBAL
#if REMOTE_DEVICE == REMOTE_VIDEO_LINK
#include "Board-Support-Pack/VideoLink/video-link-remote.h"
#endif
#endif


#include "System/Input/TriggerImpl/trigger-decorator-toggle.h"
#include "System/Input/TriggerImpl/trigger-impl-click.h"
#include "System/Input/TriggerImpl/trigger-impl-edge.h"
#include "usart.h"

/* V. standard lib */





/*-------- 2. enum ---------------------------------------------------------------------------------------------------*/




/*-------- 3. interface ---------------------------------------------------------------------------------------------*/

class CommanderSrvc final : public PeriodicApp, public Singleton<CommanderSrvc> {
public:
    CommanderSrvc();

    void init() override;

    void run() override;

    void onUartRxEventCallback(size_t);
    void onUartErrCallback();

    /************ setter & getter ***********/



private:
#ifdef GIMBAL
#if REMOTE_DEVICE == REMOTE_DR16
    RemoteBase& remote = RemoteDR16::instance();
#elif REMOTE_DEVICE == REMOTE_VIDEO_LINK
    RemoteBase& remote = VideoLinkRemote::instance();
#endif
#endif
    /* message interface */

    // 1. message queue

    // 2. mutex

    // 3. semphr

    // 4. notify

    // 5. stream or message

    // 6. event group
    InputAction _actions[21];


#if REMOTE_DEVICE != REMOTE_GAMEPAD || defined(CHASSIS)

    TriggerLinear _joystickDeadzone; // 摇杆死区触发器
    TriggerHold _work;
    TriggerHold _trigFricToggle;
    TriggerHold _triggerBurst;
    TriggerHold _triggerMouseBurst;
    TriggerHold _trigPress;
    TriggerHold _trigSingleRelease;
    TriggerHold _trigQToggleBase;
    TriggerToggle _trigQToggle;
    TriggerHold _trigShiftHold;
    TriggerHold _trigMouseRelease;
    TriggerHold _trigMouseBurst;
    TriggerHold baseTrigger = TriggerHold(0.5f, 0.001, true, HoldCondition::GreaterOrEqual);
    TriggerHold baseTriggerCap = TriggerHold(0.5f, 0.001, true, HoldCondition::GreaterOrEqual);
    TriggerToggle _trigSpin;
    TriggerToggle _trigCap;
    TriggerHold _trigVision;

    TriggerEdge _trigMouseFricEdge{0.5f, EdgeType::Rising};
    TriggerEdge _trigMouseBurstEdge{0.5f, EdgeType::Both};
    TriggerClick _trigMouseSingle{0.5f, 0.7f};
    TriggerEdge _visionBurstEdge{0.5f, EdgeType::Both};



    // 1. 系统级意图 (仲裁器专用)

    InputAction& actionCtrlMode     = _actions[0]; // 控制源切换 (DR16, 视觉, 键盘)
    InputAction& actionSateStop     = _actions[1]; // 物理急停

    // 2. 底盘意图 (底盘任务专用)
    InputAction& actionMoveX        = _actions[2];
    InputAction& actionMoveY        = _actions[3];
    InputAction& actionSpin         = _actions[4];
    InputAction& actionMoveXKey     = _actions[5];
    InputAction& actionMoveYKey     = _actions[6];

    // 3. 云台意图 (云台任务专用)
    InputAction& actionYaw          = _actions[7];
    InputAction& actionPitch        = _actions[8];
    InputAction& actionMouseYaw     = _actions[9];
    InputAction& actionMousePitch   = _actions[10];
    InputAction& actionMouseBurst = _actions[11];
    InputAction& actionMouseVision = _actions[20];

    // 1. 定义 Action (意图)
    InputAction& actionFricToggle   = _actions[12];
    InputAction& actionShootBurst   = _actions[13];
    InputAction& actionShootSingle  = _actions[14];
    InputAction& actionSpinMode     = _actions[15];
    InputAction& actionKeySpin = _actions[19];

    InputAction& actionMouseSingle   = _actions[16];
    InputAction& actionKeyboardFric = _actions[17];

    InputAction& actionCapSwitch    = _actions[18];

#else
    TriggerLinear _joystickDeadzone;
    TriggerHold _handbreak;
    TriggerHold _aTest;
    TriggerToggle _relax;

    InputAction& actionRelax          = _actions[0];
    InputAction& actionHandbrakeDepth = _actions[1];
    InputAction& actionMoveX          = _actions[2];
    InputAction& actionYaw            = _actions[3];
    InputAction& actionBreak          = _actions[4];
#endif
};
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* C Interface */

#ifdef __cplusplus
}
#endif




/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/

#endif
