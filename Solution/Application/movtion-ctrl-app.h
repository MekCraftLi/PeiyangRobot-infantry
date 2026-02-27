/**
 *******************************************************************************
 * @file    movtion-ctrl-app.h
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

#ifndef INFANTRY_CHASSIS_MOVTION_CTRL_APP_H
#define INFANTRY_CHASSIS_MOVTION_CTRL_APP_H




/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#ifdef __cplusplus

/* I. interface */

#include "../System/Thread/application-base.h"
#include "../tools/crtp.h"
#include "pyro_algo_pid.h"

/* II. OS */


/* III. middlewares */


/* IV. drivers */


/* V. standard lib */





/*-------- 2. enum ---------------------------------------------------------------------------------------------------*/

struct PIDParam {
    float kp;
    float ki;
    float kd;
    float integralLimit;
    float outputLimit;
};



/*-------- 3. interface ---------------------------------------------------------------------------------------------*/

class MovtionCtrlApp final : public PeriodicApp, public Singleton<MovtionCtrlApp> {
  public:
    MovtionCtrlApp();

    void init() override;

    void run() override;

    /************ setter & getter ***********/



    pyro::pid_t* drive_spd_pid[4];
    pyro::pid_t* steer_pos_pid[4];
    pyro::pid_t* steer_spd_pid[4];

    PIDParam drivePidParam[12];
    PIDParam steerPosPidParam[12];
    PIDParam steerSpdPidParam[12];




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