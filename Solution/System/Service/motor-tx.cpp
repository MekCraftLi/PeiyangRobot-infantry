/**
 *******************************************************************************
 * @file    motor-tx.cpp
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

#include "motor-tx.h"
#include "../DataHub/blackboard.h"

/* II. other application */

#include "can-parse.h"


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = MotorTxApp::instance();



/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "MotorTx"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/






/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/


MotorTxApp::MotorTxApp()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE,  appStack, APPLICATION_PRIORITY, 1){
}


void MotorTxApp::init() {
    /* driver object initialize */
}


void MotorTxApp::run() {
    // 1. 无锁极速读取黑板上的最新输出快照
    ChassisOutput chasOut;
    Blackboard::instance().chassisOut.read(chasOut);

    // 2. 直接调用 PYRo 框架提供的方法，下发物理期望值 (float)
    for (int i = 0; i < 4; i++) {
        if (CanParseApp::instance().drive[i]) {
            // send_torque 内部会自动根据 20.0f 和 16384 的比例进行换算并限幅
            //CanParseApp::instance().drive[i]->send_torque(chas_out.drive_current[i]);
            CanParseApp::instance().drive[i]->send_torque(chasOut.driveCurrent[i]);
        }

        if (CanParseApp::instance().steer[i]) {
            // send_torque 内部会自动根据 3.0f 和 16384 的比例进行换算并限幅
            //CanParseApp::instance().steer[i]->send_torque(chas_out.steer_current[i]);
            CanParseApp::instance().steer[i]->send_torque(chasOut.steerCurrent[i]);
        }
    }

    // 循环结束时：
    // PYRo 的 dji_motor_tx_frame_t 会自动检测到所有的 _update_list 均已就绪
    // 并自动通过 CAN1/CAN2 将打包好的 0x200 (或者 GM6020的 0x1fe) 发送出去！

 
}
