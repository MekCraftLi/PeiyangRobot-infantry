/**
 *******************************************************************************
 * @file    app-led.cpp
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
 * @date    2026/2/4
 * @version 1.0
 *******************************************************************************
 */




/* ------- define ----------------------------------------------------------------------------------------------------*/





/* ------- include ---------------------------------------------------------------------------------------------------*/



/* I. header */

#include "app-led.h"

/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/





/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "Led"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

#define APPLICATION_PERIOD_MS  1

static StackType_t appStack[APPLICATION_STACK_SIZE];

static LedApp ledApp;




/* ------- message interface attribute -------------------------------------------------------------------------------*/





/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/


LedApp::LedApp()
    : StaticAppBase(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY,
                    APPLICATION_PERIOD_MS, 0, nullptr) {}


LedApp& LedApp::instance() { return ledApp; }


void LedApp::init() { /* driver object initialize */ }


void LedApp::run() {}



uint8_t LedApp::rxMsg(void* msg, uint16_t size) { return 0; }

uint8_t LedApp::rxMsg(void* msg, uint16_t size, TickType_t timeout) { return 0; }
