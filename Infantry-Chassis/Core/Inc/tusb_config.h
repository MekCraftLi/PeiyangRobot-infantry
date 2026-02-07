/**
 *******************************************************************************
 * @file    tusb_config.h
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
 * @date    2026/2/6
 * @version 1.0
 *******************************************************************************
 */


/* Define to prevent recursive inclusion -----------------------------------------------------------------------------*/

#ifndef INFANTRY_CHASSIS_TUSB_CONFIG_H
#define INFANTRY_CHASSIS_TUSB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif


/*-------- 1. includes and imports -----------------------------------------------------------------------------------*/




/*-------- 2. enum and define ----------------------------------------------------------------------------------------*/

// 配置MCU
#define CFG_TUSB_MCU              OPT_MCU_STM32H7

// 端口与速度配置
#define CFG_TUSB_RHPORT0_MODE     (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

// 驱动模式配置
#define CFG_TUD_DWC2_DMA_ENABLE   1
#define CFG_TUD_DWC2_SLAVE_ENABLE 0

// 4. 操作系统 (先跑裸机，排除 FreeRTOS 干扰)
#define CFG_TUSB_OS               OPT_OS_NONE

// 5. 内存管理 (H7 的 DMA 需要内存对齐)
#define CFG_TUSB_MEM_SECTION      __attribute__((section(".dma_pool")))
#define CFG_TUSB_MEM_ALIGN        __attribute__((aligned(4)))

// 6. 端点 0 缓冲区大小
#define CFG_TUD_ENDPOINT0_SIZE    64

#define CFG_TUD_ENABLED           1

// 7. 关闭所有 Class，只做枚举
#define CFG_TUD_CDC               0
#define CFG_TUD_MSC               0
#define CFG_TUD_HID               0
#define CFG_TUD_VENDOR            0

/*-------- 3. interface ----------------------------------------------------------------------------------------------*/




/*-------- 4. decorator ----------------------------------------------------------------------------------------------*/




/*-------- 5. factories ----------------------------------------------------------------------------------------------*/



#ifdef __cplusplus
}
#endif

#endif /*INFANTRY_CHASSIS_TUSB_CONFIG_H*/
