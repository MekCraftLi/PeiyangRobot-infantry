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
static pyro::dji_m3508_motor_drv_t* drive[4];

static pyro::dji_gm_6020_motor_drv_t* steer[4];

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

    //
    // rud_cfg.pid.wheel_pid[0] =
    //     new pid_t(20.0f, 0.1f, 0.00f, 1.00f, 20.0f);
    // rud_cfg.pid.wheel_pid[1] =
    //     new pid_t(20.0f, 0.1f, 0.00f, 1.00f, 20.0f);
    // rud_cfg.pid.wheel_pid[2] =
    //     new pid_t(20.0f, 0.1f, 0.00f, 1.00f, 20.0f);
    // rud_cfg.pid.wheel_pid[3] =
    //     new pid_t(20.0f, 0.1f, 0.00f, 1.00f, 20.0f);
    //
    // rud_cfg.pid.rud_pos_pid[0] =
    //     new pid_t(15.0f, 0.0f, 0.00f, 0.0f, 10.0f);
    // rud_cfg.pid.rud_pos_pid[1] =
    //     new pid_t(15.0f, 0.0f, 0.00f, 0.0f, 10.0f);
    // rud_cfg.pid.rud_pos_pid[2] =
    //     new pid_t(15.0f, 0.0f, 0.00f, 0.0f, 10.0f);
    // rud_cfg.pid.rud_pos_pid[3] =
    //     new pid_t(15.0f, 0.0f, 0.00f, 0.0f, 10.0f);
    //
    // rud_cfg.pid.rud_spd_pid[0] =
    //     new pid_t(0.3f, 0.0f, 0.00f, 0.0f, 3.0f);
    // rud_cfg.pid.rud_spd_pid[1] =
    //     new pid_t(0.3f, 0.0f, 0.00f, 0.0f, 3.0f);
    // rud_cfg.pid.rud_spd_pid[2] =
    //     new pid_t(0.3f, 0.0f, 0.00f, 0.0f, 3.0f);
    // rud_cfg.pid.rud_spd_pid[3] =
    //     new pid_t(0.3f, 0.0f, 0.00f, 0.0f, 3.0f);
    //
    // rud_cfg.pid.follow_yaw_pid =
    //     new pid_t(3.6f, 0.01f, 0.003f, 0.1f, 5.0f);
    //
    // rud_cfg.rud_pos_moving_offset[0] = 1.01472831f;
    // rud_cfg.rud_pos_moving_offset[1] = -0.29145637f;
    // rud_cfg.rud_pos_moving_offset[2] = -1.87299052f;
    // rud_cfg.rud_pos_moving_offset[3] = -1.04003897f;
    //
    // power_control_drv_t &power_controller = power_control_drv_t::get_instance(4);
    // power_control_drv_t::motor_coefficient_t coef1;
    // coef1.k1 = 0;
    // coef1.k2 = 0;
    // coef1.k3 = 0;
    // coef1.k4 = 0;
    // power_controller.set_motor_coefficient(1, coef1);
    //
    // power_control_drv_t::motor_coefficient_t coef2;
    // coef2.k1 = 0;
    // coef2.k2 = 0;
    // coef2.k3 = 0;
    // coef2.k4 = 0;
    // power_controller.set_motor_coefficient(2, coef2);
    //
    // power_control_drv_t::motor_coefficient_t coef3;
    // coef3.k1 = 0;
    // coef3.k2 = 0;
    // coef3.k3 = 0;
    // coef3.k4 = 0;
    // power_controller.set_motor_coefficient(3, coef3);
    //
    // power_control_drv_t::motor_coefficient_t coef4;
    // coef4.k1 = 0;
    // coef4.k2 = 0;
    // coef4.k3 = 0;
    // coef4.k4 = 0;
    // power_controller.set_motor_coefficient(4, coef4);
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

    Blackboard::instance().chassis_state.Write(state);
}
