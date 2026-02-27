/**
 *******************************************************************************
 * @file    input.cpp
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

#define REMOTE_UART huart5



/* ------- include ---------------------------------------------------------------------------------------------------*/



/* I. header */

#include "input.h"

/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = InputApp::instance();

__attribute__((section(".dma_pool"))) static uint8_t rxbuf[32] = {0};

static Dr16Data dr16Data;


// 全局动作实例 (通常放在一个 GlobalContext 或单例中)
InputAction Action_MoveX;
InputAction Action_MoveY;
InputAction Action_Yaw;
InputAction Action_Pitch;
InputAction Action_FrictionOn;
InputAction Action_FireSingle;
InputAction Action_FireBurst;
InputAction Action_ModeSwitch; // 稍微复杂一点，处理三种模式

RemoteDR16& remote = RemoteDR16::instance();

/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "Input"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];



/* ------- message interface attribute -------------------------------------------------------------------------------*/





/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/


InputApp::InputApp()
    : NotifyApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE,  appStack, APPLICATION_PRIORITY){
}


void InputApp::init() {
    /* driver object initialize */
    /* driver object initialize */

    /* 设置触发条件 */
    // 1.设置线性触发器
    static TriggerLinear trigLinear = TriggerLinear(0.01f);

    /* 将动作系统，控件以及触发器进行绑定 */
    // 2.将线性触发器绑定到右摇杆的X轴控件
    Action_MoveX.Bind(RemoteDR16::instance().getRightX(), &trigLinear);


    /* 初始化通信硬件 */
    HAL_UARTEx_ReceiveToIdle_DMA(&REMOTE_UART, rxbuf, sizeof(rxbuf));
}


void InputApp::run() {
    // 更新遥控器数据
    RemoteDR16::instance().updateRaw(dr16Data);
}


extern "C" void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {

    memcpy(&dr16Data, rxbuf, Size);

    HAL_UARTEx_ReceiveToIdle_DMA(&REMOTE_UART, rxbuf, sizeof(rxbuf));


    BaseType_t higherPriorityTaskWoken = pdFALSE;
    forceInit.notifyFromISR(&higherPriorityTaskWoken);
}

extern "C" void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
    // 1. 禁用 UART DMA
    HAL_UART_DMAStop(huart);


    // 2. 清除 UART 错误标志
    __HAL_UART_CLEAR_FEFLAG(huart);  // 帧错误
    __HAL_UART_CLEAR_NEFLAG(huart);  // 噪声错误
    __HAL_UART_CLEAR_OREFLAG(huart); // 溢出错误
    HAL_UARTEx_ReceiveToIdle_DMA(&REMOTE_UART, rxbuf, sizeof(rxbuf));
}
