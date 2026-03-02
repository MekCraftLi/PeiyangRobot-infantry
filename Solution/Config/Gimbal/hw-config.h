/**
 *******************************************************************************
 * @file    hw-config.h
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
 * @date    2026/3/1
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_HW_CONFIG_H
#define INFANTRY_HW_CONFIG_H




/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#include "pyro_can_drv.h"
#include "pyro_dji_motor_drv.h"




/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/

namespace Config::Hardware {

constexpr uint32_t SYSTEM_CLOCK_HZ = 550000000;


// ========================================
// 1. 底盘机械参数 (单位: 米/弧度)
// ========================================

// ========================================
// 2. 电机 CAN 总线拓扑映射
// ========================================
namespace MotorTopo {
constexpr pyro::can_hub_t::which_can FRIC_LEFT_CAN = pyro::can_hub_t::can2;
constexpr pyro::can_hub_t::which_can FRIC_RIGHT_CAN = pyro::can_hub_t::can2;
constexpr pyro::can_hub_t::which_can PITCH_CAN = pyro::can_hub_t::can2;
constexpr pyro::can_hub_t::which_can YAW_CAN = pyro::can_hub_t::can1;
constexpr pyro::can_hub_t::which_can TRIGGER_CAN = pyro::can_hub_t::can1;

constexpr pyro::dji_motor_tx_frame_t::register_id_t FRIC_LEFT_ID = pyro::dji_motor_tx_frame_t::id_1;
constexpr pyro::dji_motor_tx_frame_t::register_id_t FRIC_RIGHT_ID = pyro::dji_motor_tx_frame_t::id_2;
constexpr pyro::dji_motor_tx_frame_t::register_id_t YAW_ID = pyro::dji_motor_tx_frame_t::id_5;
constexpr pyro::dji_motor_tx_frame_t::register_id_t TRIGGER_ID = pyro::dji_motor_tx_frame_t::id_3;
constexpr pyro::dji_motor_tx_frame_t::register_id_t PITCH = pyro::dji_motor_tx_frame_t::id_5;

constexpr uint16_t YAW_OFFSET = 1526;
}


// ========================================
// 3. 核心外设映射
// ========================================
namespace Comms {
// 遥控器 DR16 接收串口
#define REMOTE_UART huart5
}
}


/*-------- 3. interface ----------------------------------------------------------------------------------------------*/




/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_HW_CONFIG_H*/
