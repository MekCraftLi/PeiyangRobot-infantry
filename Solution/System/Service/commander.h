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
#include "Config/Chassis/algo-config.h"
#include "usart.h"

/* V. standard lib */





/*-------- 2. enum ---------------------------------------------------------------------------------------------------*/




/*-------- 3. interface ---------------------------------------------------------------------------------------------*/

class CommanderSrvc final : public PeriodicApp, public Singleton<CommanderSrvc> {
  public:
    CommanderSrvc();

    void init() override;

    void run() override;


    /************ setter & getter ***********/



  private:
    /* message interface */

    // 1. message queue

    // 2. mutex

    // 3. semphr

    // 4. notify

    // 5. stream or message

    // 6. event group


    TriggerLinear _joystickDeadzone; // 摇杆死区触发器
    TriggerHold _work;
    TriggerHold _trigFricToggle;
    TriggerHold _triggerBurst;
    TriggerHold _trigSingleRelease;

    InputAction _actions[11];

    // 1. 系统级意图 (仲裁器专用)
    InputAction& actionCtrlMode          = _actions[0]; // 控制源切换 (DR16, 视觉, 键盘)
    InputAction& actionSateStop          = _actions[1]; // 物理急停

    // 2. 底盘意图 (底盘任务专用)
    InputAction& actionMoveX             = _actions[2];
    InputAction& actionMoveY             = _actions[3];
    InputAction& actionSpin              = _actions[4];

    // 3. 云台意图 (云台任务专用)
    InputAction& actionYaw         = _actions[5];
    InputAction& actiongPitch       = _actions[6];

    // 1. 定义 Action (意图)
    InputAction& actionFricToggle  = _actions[8];
    InputAction& actionShootBurst  = _actions[9];
    InputAction& actionShootSingle = _actions[10];
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