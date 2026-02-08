/**
 *******************************************************************************
 * @file    app-can.cpp
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
 * @date    2026/2/7
 * @version 1.0
 *******************************************************************************
 */




/* ------- define ----------------------------------------------------------------------------------------------------*/





/* ------- include ---------------------------------------------------------------------------------------------------*/



/* I. header */

#include "app-can.h"

/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

pyro::can_drv_t candrv1(&hfdcan1);
pyro::can_drv_t candrv2(&hfdcan2);
pyro::can_drv_t candrv3(&hfdcan3);





/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "Can"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

#define APPLICATION_PERIOD_MS  100

static StackType_t appStack[APPLICATION_STACK_SIZE];

static CanApp canApp;




/* ------- message interface attribute -------------------------------------------------------------------------------*/






/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/


CanApp::CanApp()
    : StaticAppBase(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE,  appStack, APPLICATION_PRIORITY, APPLICATION_PERIOD_MS, 0, nullptr){
}


CanApp& CanApp::instance() {
    return canApp;
}


void CanApp::init() {
    /* driver object initialize */

    candrv1.init().start();
    candrv2.init().start();
    candrv3.init().start();


}

uint8_t can_data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
void CanApp::run() {

    candrv1.send_msg(0x202, can_data);
 
}



uint8_t CanApp::rxMsg(void* msg, uint16_t size) {

    return 0;
}

uint8_t CanApp::rxMsg(void* msg, uint16_t size, TickType_t timeout) {

    return 0;
}



