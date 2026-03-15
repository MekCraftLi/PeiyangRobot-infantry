/**
 *******************************************************************************
 * @file    interrupt-callback.cpp
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
 * @date    2026/3/4
 * @version 1.0
 *******************************************************************************
 */


/* ------- define ----------------------------------------------------------------------------------------------------*/





/* ------- include ---------------------------------------------------------------------------------------------------*/

#include "Config/Chassis/hw-config.h"
#include "commander.h"
#include "main.h"
#include "referee.h"
#include "vision-comm.h"



/* ------- class prototypes-------------------------------------------------------------------------------------------*/





/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/


extern "C" {

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t size) {
    switch (reinterpret_cast<uint32_t>(huart->Instance)) {
        case USART1_BASE: {
#ifdef GIMBAL
#if REMOTE_DEVICE == REMOTE_VIDEO_LINK
            CommanderSrvc::instance().onUartRxEventCallback(size);
#endif
#elifdef CHASSIS
            RefereeSrvc::instance().onUartRxEventCallback(size);
#endif
        }
        case UART7_BASE: {
#if REMOTE_DEVICE == REMOTE_GAMEPAD && defined(GIMBAL)
    CommanderSrvc::instance().onUartRxEventCallback(size);
#endif
        }break;

        case UART5_BASE: {
#ifdef GIMBAL
#if REMOTE_DEVICE == REMOTE_DR16
            CommanderSrvc::instance().onUartRxEventCallback(size);
#endif
#endif
        } break;
        default: {
        }
    }
}



extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
    // 1. 禁用 UART DMA
    HAL_UART_DMAStop(huart);
    // 2. 清除 UART 错误标志
    __HAL_UART_CLEAR_FEFLAG(huart);  // 帧错误
    __HAL_UART_CLEAR_NEFLAG(huart);  // 噪声错误
    __HAL_UART_CLEAR_OREFLAG(huart); // 溢出错误


    switch (reinterpret_cast<uint32_t>(huart->Instance)) {
        case USART1_BASE: {
#ifdef GIMBAL
#if REMOTE_DEVICE == REMOTE_VIDEO_LINK
            CommanderSrvc::instance().onUartErrCallback();
#endif
#elifdef CHASSIS
            RefereeSrvc::instance().onUartErrCallback();
#endif

        } break;

        case UART5_BASE: {
#ifdef GIMBAL
#if REMOTE_DEVICE == REMOTE_DR16
            CommanderSrvc::instance().onUartErrCallback();
#endif
#endif

        } break;
        default: {
        }
    }
}




}
