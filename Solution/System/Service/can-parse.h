/**
 *******************************************************************************
 * @file    can-parse.h
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

#ifndef INFANTRY_CAN_PARSE_H
#define INFANTRY_CAN_PARSE_H




/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#ifdef __cplusplus

/* I. interface */

#include "../../tools/crtp.h"
#include "../Thread/application-base.h"
#include "pyro_dji_motor_drv.h"

/* II. OS */


/* III. middlewares */


/* IV. drivers */


/* V. standard lib */





/*-------- 2. enum ---------------------------------------------------------------------------------------------------*/




/*-------- 3. interface ---------------------------------------------------------------------------------------------*/

class CanParseApp final : public PeriodicApp, public Singleton<CanParseApp> {
  public:
    CanParseApp();

    void init() override;

    void run() override;

    /************ setter & getter ***********/
    




    pyro::dji_m3508_motor_drv_t* drive[4];

    pyro::dji_gm_6020_motor_drv_t* steer[4];

    uint16_t steerEcdOffset[4] = {1122, 7202, 4052, 2474};


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