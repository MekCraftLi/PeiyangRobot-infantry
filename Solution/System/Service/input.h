/**
 *******************************************************************************
 * @file    input.h.h
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

#include "../../../正常运行/System/Thread/application-base.h"
#include "../../../正常运行/tools/crtp.h"

/* II. OS */


/* III. middlewares */
#include "../../../正常运行/Board-Support-Pack/DR16/dr16.h"
#include "../../../正常运行/System/Input/ControlImpl/control-impl-axis.h"
#include "../../../正常运行/System/Input/ControlImpl/control-impl-switch.h"
#include "../../../正常运行/System/Input/TriggerImpl/trigger-impl-hold.h"
#include "../../../正常运行/System/Input/TriggerImpl/trigger-impl-linear.h"
#include "../../../正常运行/System/Input/TriggerImpl/trigger-impl-match.h"
#include "../../../正常运行/System/Input/TriggerImpl/trigger-impl-pulse.h"
#include "../../../正常运行/System/Input/action.h"


/* IV. drivers */
#include "usart.h"

/* V. standard lib */





/*-------- 2. enum ---------------------------------------------------------------------------------------------------*/




/*-------- 3. interface ---------------------------------------------------------------------------------------------*/

class InputApp final : public NotifyApp, public Singleton<InputApp> {
  public:
    InputApp();

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