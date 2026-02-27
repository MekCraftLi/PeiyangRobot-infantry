/**
 *******************************************************************************
 * @file    movtion-ctrl-app.cpp
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




/* ------- define ----------------------------------------------------------------------------------------------------*/





/* ------- include ---------------------------------------------------------------------------------------------------*/



/* I. header */

#include "movtion-ctrl-app.h"

/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = MovtionCtrlApp::instance();



/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "MovtionCtrl"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/






/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/


MovtionCtrlApp::MovtionCtrlApp()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE,  appStack, APPLICATION_PRIORITY, 1){
}


void MovtionCtrlApp::init() {
    /* driver object initialize */
}


void MovtionCtrlApp::run() {
 
}
