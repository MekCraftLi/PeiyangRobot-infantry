/**
 *******************************************************************************
 * @file    real-time-comm.cpp
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
 * @date    2026/3/3
 * @version 1.0
 *******************************************************************************
 */




/* ------- define ----------------------------------------------------------------------------------------------------*/





/* ------- include ---------------------------------------------------------------------------------------------------*/



/* I. header */

#include "real-time-comm.h"

#include "System/DataHub/blackboard.h"
#include "System/DataHub/data-def.h"
#include "motor-actuator.h"

/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = RealTimeCommApp::instance();



/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "RealTimeComm"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/






/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/


RealTimeCommApp::RealTimeCommApp()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE,  appStack, APPLICATION_PRIORITY, 10){
}


void RealTimeCommApp::init() {
    /* driver object initialize */
    MotActSrvc::instance().waitInit();
}


void RealTimeCommApp::run() {
#ifdef GIMBAL
    static GimbalToChassisComm output;
#elifdef CHASSIS
#endif

    [[maybe_unused]]static FDCAN_TxHeaderTypeDef txHeader = {
#ifdef GIMBAL
        .Identifier = 0x0D000721,
#elifdef CHASSIS
        .Identifier = 0x0D000722,
#endif
        .IdType = FDCAN_EXTENDED_ID,
        .TxFrameType = FDCAN_DATA_FRAME,
        .DataLength = FDCAN_DLC_BYTES_8,
        .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        .BitRateSwitch = FDCAN_BRS_OFF,
        .FDFormat = FDCAN_CLASSIC_CAN,
        .TxEventFifoControl = FDCAN_NO_TX_EVENTS,
        .MessageMarker = 0,
    };
#ifdef GIMBAL
    Blackboard::instance().g2cOutput.read(output);
    taskENTER_CRITICAL();
    HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, output.buffer);
    taskEXIT_CRITICAL();
#elifdef CHSSIS

#endif

}

#ifdef CHASSIS
extern "C" void getBoardCommFromISR(uint8_t* pData) {

    static GimbalToChassisComm comm{};
    memcpy(comm.buffer, pData, 8);
    Blackboard::instance().rComm.write(comm);

}
#elifdef GIMBAL

extern "C" void getBoardCommFromISR(uint8_t* pData) {


}
#endif
