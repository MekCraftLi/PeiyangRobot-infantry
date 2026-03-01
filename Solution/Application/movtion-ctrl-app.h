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


#ifdef CHASSIS
    pyro::pid_t driveSpdPid[4] = {
        pyro::pid_t(0.2f, 0.000f, 0.0f, 1.0f, 20.0f),
        pyro::pid_t(0.2f, 0.000f, 0.0f, 1.0f, 20.0f),
        pyro::pid_t(0.2f, 0.000f, 0.0f, 1.0f, 20.0f),
        pyro::pid_t(0.2f, 0.000f, 0.0f, 1.0f, 20.0f),
    };
    pyro::pid_t steerPosPid[4] = {
        pyro::pid_t(58.0f,  5.0f, 0.0f, 10.0f, 50.0f),
        pyro::pid_t(58.0f,  5.0f, 0.0f, 10.0f, 50.0f),
        pyro::pid_t(58.0f,  5.0f, 0.0f, 10.0f, 50.0f),
        pyro::pid_t(58.0f,  5.0f, 0.0f, 10.0f, 50.0f),
    };
    pyro::pid_t steerSpdPid[4] = {
        pyro::pid_t(0.87f,  0.0f, 0.0f, 1.0f, 24.0f),
        pyro::pid_t(0.87f,  0.0f, 0.0f, 1.0f, 24.0f),
        pyro::pid_t(0.87f,  0.0f, 0.0f, 1.0f, 24.0f),
        pyro::pid_t(0.87f,  0.0f, 0.0f, 1.0f, 24.0f)
    };


#elifdef GIMBAL

    pyro::pid_t yawPosPid = pyro::pid_t(60.0f, 3.0f, 0.0f, 10.0f, 500.0f);
    pyro::pid_t yawSpdPid = pyro::pid_t(10.0f, 0.0f, 0.0f, 1.0f, 24.0f);

#endif



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