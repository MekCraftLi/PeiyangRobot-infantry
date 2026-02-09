/**
 *******************************************************************************
 * @file    app-cmd.h
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


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_CHASSIS_APP_CMD_H
#define INFANTRY_CHASSIS_APP_CMD_H




/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#ifdef __cplusplus

/* I. interface */

#include "../Thread/application-base.h"

/* II. OS */


/* III. middlewares */
#include "../../Board-Support-Pack/DR16/dr16.h"
#include "../Input/ControlImpl/control-impl-axis.h"
#include "../Input/ControlImpl/control-impl-switch.h"
#include "../Input/TriggerImpl/trigger-impl-hold.h"
#include "../Input/TriggerImpl/trigger-impl-linear.h"
#include "../Input/TriggerImpl/trigger-impl-match.h"
#include "../Input/TriggerImpl/trigger-impl-pulse.h"
#include "../Input/action.h"

/* IV. drivers */

#include "usart.h"


/* V. standard lib */





/*-------- 2. enum ---------------------------------------------------------------------------------------------------*/




/*-------- 3. interface ---------------------------------------------------------------------------------------------*/

class CmdApp final : public StaticAppBase {
  public:
    CmdApp();

    void init() override;

    void run() override;

    uint8_t rxMsg(void* msg, uint16_t size = 0) override;

    uint8_t rxMsg(void *msg, uint16_t size, TickType_t timeout)  override;

    /************ setter & getter ***********/
    static CmdApp& instance();


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