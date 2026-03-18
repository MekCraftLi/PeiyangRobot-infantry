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
#include "super-cap-comm.h"
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
        } break;
        case UART7_BASE: {
#if REMOTE_DEVICE == REMOTE_GAMEPAD && defined(GIMBAL)
            CommanderSrvc::instance().onUartRxEventCallback(size);
#endif
#ifdef CHASSIS
            SuperCapCommSrvc::instance().onUartRxEventCallback(size);
#endif
        } break;

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

        case UART7_BASE: {
#ifdef CHASSIS
            SuperCapCommSrvc::instance().onUartErrCallback();
#endif

        } break;
        default: {
        }
    }
}

extern "C" void HAL_FDCAN_ErrorStatusCallback(FDCAN_HandleTypeDef* hfdcan, uint32_t ErrorStatusITs) {
    // 如果发生总线关闭 (Bus-Off)
    if (ErrorStatusITs & FDCAN_IT_ERROR_PASSIVE) {
        // 1. 暴力关闭 FDCAN。这会清空被堵死的 Tx FIFO，并硬件重置 INIT 位
        HAL_FDCAN_Stop(hfdcan);

        // 2. 重新启动 FDCAN
        HAL_FDCAN_Start(hfdcan);

        // 3. 重新激活中断 (Stop 会清空中断配置，必须重开)
        HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE | FDCAN_IT_ERROR_PASSIVE, 0);
    }
}
}
