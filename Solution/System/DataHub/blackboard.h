/**
 *******************************************************************************
 * @file    blackboard.h
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


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_CHASSIS_BLACKBOARD_H
#define INFANTRY_CHASSIS_BLACKBOARD_H



/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/

#include "data-def.h"
#include "seq-variable.h"

#include "Component/Motor/pyro_dji_motor_drv.h"



/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/




/*-------- 3. interface ----------------------------------------------------------------------------------------------*/

class Blackboard: public Singleton<Blackboard> {
public:

    Blackboard() = default;

    // ----------------------------------------
    // [意图区] (主要由 DR16 任务 / ROS 通信任务 写入)
    // ----------------------------------------
    SeqVariable<RcRawData>    rc_raw;
    SeqVariable<RcRawData>    rc_other; // 其他输入源（如第二遥控器、上位机、图传链路等）


    SeqVariable<ChassisCmd>   chassis_cmd;
    SeqVariable<GimbalCmd>    gimbal_cmd;

    // ----------------------------------------
    // [状态区] (主要由 CAN 接收任务 / SPI 中断 写入)
    // ----------------------------------------
    SeqVariable<ImuState>     imu_state;
    SeqVariable<ChassisState> chassis_state;
    SeqVariable<GimbalState>  gimbal_state;

    // ----------------------------------------
    // [输出区] (主要由 核心控制算法任务 写入)
    // ----------------------------------------
    SeqVariable<ChassisOutput> chassis_out;
    SeqVariable<GimbalOutput>  gimbal_out;

    // ----------------------------------------
    // [中间区] (主要由 核心控制算法任务 同步写入)
    // ----------------------------------------
    SeqVariable<ChassisTelemetry> chassis_telem;

private:

};




/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/





#endif /*INFANTRY_CHASSIS_BLACKBOARD_H*/
