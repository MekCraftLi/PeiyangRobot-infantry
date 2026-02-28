/**
 *******************************************************************************
 * @file    motor-actuator.cpp
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

#include "motor-actuator.h"
#include "Config/config.h"

/* II. other application */

#include "Component/Motor/pyro_dji_motor_drv.h"
#include "Peripheral/CAN/pyro_can_drv.h"

#include "../DataHub/blackboard.h"

/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = MotorActuatorSrvc::instance();


pyro::can_drv_t candrv1(&hfdcan1);
pyro::can_drv_t candrv2(&hfdcan2);
pyro::can_drv_t candrv3(&hfdcan3);

// 改为指针声明




/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "CanParse"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/





/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/


MotorActuatorSrvc::MotorActuatorSrvc()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 1) {}


void MotorActuatorSrvc::init() {
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
        new (&drive[i]) pyro::dji_m3508_motor_drv_t((pyro::dji_motor_tx_frame_t::register_id_t)i, Config::Hardware::MotorTopo::DRIVE_MOTOR_CANS[i]);
        new (&steer[i]) pyro::dji_gm_6020_motor_drv_t((pyro::dji_motor_tx_frame_t::register_id_t)i, Config::Hardware::MotorTopo::STEER_MOTOR_CANS[i], Config::Hardware::MotorTopo::STEER_ECD_OFFSET[i]);
    }

}


void MotorActuatorSrvc::run() {

    ChassisState state{.timestamp = xTaskGetTickCount()};
    ChassisOutput chasOut {};
    Blackboard::instance().chassisOut.read(chasOut);

    // 更新数据与发送数据
    for (uint8_t i = 0; i < 4; i++) {
        drive[i].update_feedback();
        steer[i].update_feedback();
        state.modules[i].drive.pos = drive[i].get_current_position();
        state.modules[i].drive.temp = drive[i].get_temperature();
        state.modules[i].drive.torque = drive[i].get_current_torque();
        state.modules[i].drive.vel = drive[i].get_current_rotate();

        state.modules[i].steer.pos = steer[i].get_current_position();
        state.modules[i].steer.temp = steer[i].get_temperature();
        state.modules[i].steer.torque = steer[i].get_current_torque();
        state.modules[i].steer.vel = steer[i].get_current_rotate();
        steer[i].send_torque(chasOut.steerCurrent[i]);
        drive[i].send_torque(chasOut.driveCurrent[i]);

    }

    Blackboard::instance().chassisState.Write(state);
}
