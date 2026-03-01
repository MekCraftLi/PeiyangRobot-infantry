/**
 *******************************************************************************
 * @file    state-estimator.cpp
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


#define ACCEL_CS_GPIO_Port GPIOC
#define ACCEL_CS_Pin       GPIO_PIN_0
#define GYRO_CS_GPIO_Port  GPIOC
#define GYRO_CS_Pin        GPIO_PIN_3


/* ------- include ---------------------------------------------------------------------------------------------------*/



/* I. header */

#include "state-estimator.h"


/* II. other application */



#include "System/DataHub/blackboard.h"
#include "System/DataHub/data-def.h"


/* III. standard lib */
#include "Algorithm/EKF/QuaternionEKF.h"
#include "pyro_dwt_drv.h"



/* ------- class prototypes-----------------------------------------------------------------------------------------*/




/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

static __attribute__((section(".dma_pool"))) uint8_t rxBuf[64];
static __attribute__((section(".dma_pool"))) uint8_t txBuf[64];


uint32_t exeTimeUs;

[[maybe_unused]] static auto& app = ImuApp::instance();




/* ------- application attribute -------------------------------------------------------------------------------------*/

#define APPLICATION_ENABLE     true

#define APPLICATION_NAME       "Imu"

#define APPLICATION_STACK_SIZE 1024

#define APPLICATION_PRIORITY   48

#define APPLICATION_PERIOD_MS  0

static StackType_t appStack[APPLICATION_STACK_SIZE];




/* ------- message interface attribute -------------------------------------------------------------------------------*/





/* ------- function prototypes ---------------------------------------------------------------------------------------*/





/* ------- function implement ----------------------------------------------------------------------------------------*/


ImuApp::ImuApp()
    : NotifyApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY),
      _bmi088(&IMU_SPI_HANDLE, ACCEL_CS_GPIO_Port, GYRO_CS_GPIO_Port, ACCEL_CS_Pin, GYRO_CS_Pin, txBuf, rxBuf) {}


void ImuApp::init() {
    HAL_TIM_Base_Start(&htim23);
    _bmi088.init(Bmi088AccRange::RANGE_3G, Bmi088AccODR::ODR_200_HZ, Bmi088AccWidth::OSR2,
                 Bmi088GyroRange::RANGE_500_DPS, Bmi088GyroWidth::ODR_1000HZ_BW_116HZ);

    IMU_QuaternionEKF_Init(10.0f, 0.001f, 10000000.0f, 0.9996f, 0);
}


uint32_t exeTime = 0;
void ImuApp::run() {
    static uint32_t dwtCnt;

    float dt = pyro::dwt_drv_t::get_delta_t(&dwtCnt);


    _bmi088.getImuData(data);

    // -------------------------------------------------------------
    // 【极其关键：坐标系对齐】
    // 在这里，你必须根据你的板子安装方向，手动凑出“右手直角坐标系” (前X, 左Y, 上Z)。
    // 比如，如果你的板子 Y 轴是向后的，你得传 -gy 和 -ay。
    // -------------------------------------------------------------
    float input_gx = data.rate.x, input_gy = -data.rate.y, input_gz = -data.rate.z;
    float input_ax = data.accel.x, input_ay = -data.accel.y, input_az = -data.accel.z;

    // 3. 执行 EKF 更新 (纯数学运算，几微秒跑完)
    uint32_t startTime = pyro::dwt_drv_t::get_current_ticks();
    IMU_QuaternionEKF_Update(input_gx, input_gy, input_gz,
                             input_ax, input_ay, input_az, dt);
    exeTime = pyro::dwt_drv_t::get_current_ticks() - startTime;

    // 4. 从全局的 QEKF_INS 结构体中提取解算好的欧拉角
    // (QEKF_INS 是 QuaternionEKF.h 中定义好的全局结构体，算完后自动更新)
    ImuState state;
    state.roll  = QEKF_INS.Roll;  // 内部已转为 Rad 或 Degree，请查阅 QuaternionEKF.h 确认
    state.pitch = QEKF_INS.Pitch;
    state.yaw   = QEKF_INS.Yaw;

    // 保留陀螺仪原始数据给云台内环
    state.gyro[0] = input_gx;
    state.gyro[1] = input_gy;
    state.gyro[2] = input_gz;

    state.accel[0] = input_ax;
    state.accel[1] = input_ay;
    state.accel[2] = input_az;

    state.timestamp = xTaskGetTickCount();

    // 5. 写入你自己的黑板
    Blackboard::instance().imuState.write(state);


}


void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &hspi2) {
        GPIOC->BSRR                         = (ACCEL_CS_Pin | GYRO_CS_Pin);
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(app._waitForReceive, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}


void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &hspi2) {
        app._bmi088.onTxComplete();
    }
}


void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &hspi2) {
        app._bmi088.onTransferComplete();
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin == GPIO_PIN_12 && app._inited) {
        BaseType_t pxHigherPriorityTaskWoken;
        app._bmi088.onExti();
        ImuApp::instance().notifyFromISR(&pxHigherPriorityTaskWoken);
    }
}
