/**
 *******************************************************************************
 * @file    can-parse.cpp
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

#include "can-parse.h"

/* II. other application */

#include "Component/Motor/pyro_dji_motor_drv.h"
#include "Peripheral/CAN/pyro_can_drv.h"

#include "../DataHub/blackboard.h"

/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = CanParseApp::instance();


pyro::can_drv_t candrv1(&hfdcan1);
pyro::can_drv_t candrv2(&hfdcan2);
pyro::can_drv_t candrv3(&hfdcan3);

// 改为指针声明


static uint8_t motorsIdx[4] = {3, 2, 4, 1};
static pyro::can_hub_t::which_can motorsCan[4] = {pyro::can_hub_t::can2, pyro::can_hub_t::can1, pyro::can_hub_t::can2, pyro::can_hub_t::can1};



/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "CanParse"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/





/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/


CanParseApp::CanParseApp()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 1) {}


void CanParseApp::init() {
    /* driver object initialize */

    /* 1. driver object initialize */
    candrv1.init().start();
    candrv2.init().start();
    candrv3.init().start();

    /* 2. 【关键】先将底层硬件注册进 hub */
    pyro::can_hub_t::get_instance()->hub_register_can_obj(&hfdcan1, &candrv1);
    pyro::can_hub_t::get_instance()->hub_register_can_obj(&hfdcan2, &candrv2);
    pyro::can_hub_t::get_instance()->hub_register_can_obj(&hfdcan3, &candrv3);

    /* 3. 再实例化电机对象，此时它们就能从 hub 中成功获取 _can_drv 并注册反馈邮箱了 */


    for (uint8_t i = 0; i < 4; i++) {
        motorsIdx[i] -= 1;
        drive[i] = new pyro::dji_m3508_motor_drv_t((pyro::dji_motor_tx_frame_t::register_id_t)i, motorsCan[i]);
        steer[i] = new pyro::dji_gm_6020_motor_drv_t((pyro::dji_motor_tx_frame_t::register_id_t)i, motorsCan[i]);
    }

}


void CanParseApp::run() {

    ChassisState state{.timestamp = xTaskGetTickCount()};

    for (uint8_t i = 0; i < 4; i++) {
        drive[i]->update_feedback();
        steer[i]->update_feedback();
        state.modules[motorsIdx[i]].drive.pos = drive[i]->get_current_position();
        state.modules[motorsIdx[i]].drive.temp = drive[i]->get_temperature();
        state.modules[motorsIdx[i]].drive.torque = drive[i]->get_current_torque();
        state.modules[motorsIdx[i]].drive.vel = drive[i]->get_current_rotate();

        state.modules[motorsIdx[i]].steer.pos = steer[i]->get_current_position();
        state.modules[motorsIdx[i]].steer.temp = steer[i]->get_temperature();
        state.modules[motorsIdx[i]].steer.torque = steer[i]->get_current_torque();
        state.modules[motorsIdx[i]].steer.vel = steer[i]->get_current_rotate();

    }

    Blackboard::instance().chassisState.Write(state);
}
