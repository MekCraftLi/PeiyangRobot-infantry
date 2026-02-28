/**
 *******************************************************************************
 * @file    heart-beat.cpp
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
 * @date    2026/2/28
 * @version 1.0
 *******************************************************************************
 */




/* ------- define ----------------------------------------------------------------------------------------------------*/





/* ------- include ---------------------------------------------------------------------------------------------------*/



/* I. header */

#include "heart-beat.h"

#include "pyro_dwt_drv.h"

/* II. other application */
#include "../DataHub/blackboard.h"

/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/



[[maybe_unused]] static auto& forceInit = HeartBeatApp::instance();



/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "HeartBeat"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/






/* ------- function prototypes ---------------------------------------------------------------------------------------*/



Blackboard* bb = &Blackboard::instance();

/* ------- function implement ----------------------------------------------------------------------------------------*/


HeartBeatApp::HeartBeatApp()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE,  appStack, APPLICATION_PRIORITY, 1000){
}


void HeartBeatApp::init() {
    /* driver object initialize */
    pyro::dwt_drv_t::init(550);
}


void HeartBeatApp::run() {
 pyro::dwt_drv_t::get_timeline();
}
