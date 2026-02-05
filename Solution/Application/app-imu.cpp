/**
 *******************************************************************************
 * @file    app-imu.cpp
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
 * @date    2026/2/5
 * @version 1.0
 *******************************************************************************
 */




/* ------- define ----------------------------------------------------------------------------------------------------*/

#define IMU_SPI_HANDLE     hspi2

// 定义片选引脚 (根据原理图)
#define ACCEL_CS_GPIO_Port GPIOC
#define ACCEL_CS_Pin       GPIO_PIN_0
#define GYRO_CS_GPIO_Port  GPIOC
#define GYRO_CS_Pin        GPIO_PIN_3


/* ------- include ---------------------------------------------------------------------------------------------------*/



/* I. header */

#include "app-imu.h"


/* II. other application */


/* III. standard lib */




/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

static __attribute__((section(".dma_pool"))) uint8_t rxBuf[64];
static __attribute__((section(".dma_pool"))) uint8_t txBuf[64];
static __attribute__((section(".dma_pool"))) uint8_t range;
static __attribute__((section(".dma_pool"))) uint16_t temp1;
typedef union {
    struct __attribute__((packed)) {
        uint8_t lsb; // 最低的 8 bits
        uint8_t msb; // 高 3 bits（位7~5）
    };
    struct __attribute__((packed)) {
        int16_t temp11 : 11; // 11bit signed
    };
} temp_raw11_t;

static __attribute__((section(".dma_pool"))) temp_raw11_t temp_raw11;




/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "Imu"

#define APPLICATION_STACK_SIZE 512

#define APPLICATION_PRIORITY   4

#define APPLICATION_PERIOD_MS  0

static StackType_t appStack[APPLICATION_STACK_SIZE];

static ImuApp imuApp;




/* ------- message interface attribute -------------------------------------------------------------------------------*/





/* ------- function prototypes ---------------------------------------------------------------------------------------*/

bool waitForReceive(uint32_t timeout);
bool waitForTransmit(uint32_t timeout);
bool waitForTransmitReceive(uint32_t timeout);




/* ------- function implement ----------------------------------------------------------------------------------------*/


ImuApp::ImuApp()
    : StaticAppBase(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY,
                    APPLICATION_PERIOD_MS, 0, nullptr),
      _bmi088(&IMU_SPI_HANDLE, ACCEL_CS_GPIO_Port, GYRO_CS_GPIO_Port, ACCEL_CS_Pin, GYRO_CS_Pin, txBuf, rxBuf) {}


ImuApp& ImuApp::instance() { return imuApp; }



void ImuApp::init() {
    HAL_TIM_Base_Start(&htim23);
    _bmi088.init(Bmi088AccRange::RANGE_3G, Bmi088AccODR::ODR_200_HZ, Bmi088AccWidth::OSR2, Bmi088GyroRange::RANGE_125_DPS, Bmi088GyroWidth::ODR_1000HZ_BW_116HZ);
}


ImuData data;
uint32_t exeTimeUs;

void ImuApp::run() {
    uint32_t lastTime = TIM23->CNT;

    _bmi088.waitingForData(portMAX_DELAY);
    _bmi088.getImuData(data);



    exeTimeUs  = TIM23->CNT - lastTime;
}



uint8_t ImuApp::rxMsg(void* msg, uint16_t size) { return 0; }
uint8_t ImuApp::rxMsg(void* msg, uint16_t size, TickType_t timeout) { return 0; }


void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &hspi2 && imuApp._inited) {
        GPIOC->BSRR                         = (ACCEL_CS_Pin | GYRO_CS_Pin);
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(imuApp._waitForReceive, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}


void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &hspi2 && imuApp._inited) {
        imuApp._bmi088.onTxComplete();
    }
}


void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &hspi2 && imuApp._inited) {
        imuApp._bmi088.onTransferComplete();
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_12 && imuApp._inited) {
        imuApp._bmi088.onExti();
    }
}


bool waitForReceive(uint32_t timeout) { return xSemaphoreTake(imuApp._waitForReceive, timeout); }

bool waitForTransmit(uint32_t timeout) { return xSemaphoreTake(imuApp._waitForTransmit, timeout); }

bool waitForTransmitReceive(uint32_t timeout) { return xSemaphoreTake(imuApp._waitForTransmitReceive, timeout); }


