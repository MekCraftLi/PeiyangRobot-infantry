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
#include "System/DataHub/referee-data-hub.h"
#include "System/DataHub/referee-protocol.h"
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
        .Identifier = 0x100,
#elifdef CHASSIS
        .Identifier = 0x101,
#endif
        .IdType = FDCAN_STANDARD_ID,
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

    HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, output.buffer);

#elifdef CHASSIS
    // 1. 获取裁判系统中的射击数据
    RMShootData shootData{};
    RefereeDataHub::instance().shootData.read(shootData);
    static ChassisToGimbalComm comm {};

    // 2. 填充到底盘发往云台的结构体中
    comm.msg.initialSpeed = shootData.initialSpeed;

    // 3. 将装填好的数据回写到底盘黑板，供调试或其他应用查看
    Blackboard::instance().tComm.write(comm);

    // 4. 将 buffer 发送至 CAN 邮箱
    HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan3, &txHeader, comm.buffer);
#endif

}

#ifdef CHASSIS
extern "C" void getBoardCommFromISR(uint8_t* pData) {

    static GimbalToChassisComm comm{};
    memcpy(comm.buffer, pData, 8);
    Blackboard::instance().rComm.writeFromISR(comm);

}
#elifdef GIMBAL

extern "C" void getBoardCommFromISR(uint8_t* pData) {

    // [新增] 云台板在此接收来自底盘的 0x101 CAN 数据
    static ChassisToGimbalComm comm{};
    memcpy(comm.buffer, pData, 8);
    Blackboard::instance().c2gComm.writeFromISR(comm);
}
#endif