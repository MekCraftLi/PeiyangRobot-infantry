/**
 *******************************************************************************
 * @file    usb-descriptors.c
 * @brief   USB描述符
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


/* ------- define ----------------------------------------------------------------------------------------------------*/





/* ------- include ---------------------------------------------------------------------------------------------------*/

#include "../../ThirdParty/TinyUSB/src/tusb.h"




/* ------- class prototypes-------------------------------------------------------------------------------------------*/





/* ------- macro -----------------------------------------------------------------------------------------------------*/





/* ------- variables -------------------------------------------------------------------------------------------------*/

tusb_desc_device_t const descDevice = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xCafe,
    .idProduct          = 0x0001,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01,
};

// 配置描述符：空配置
uint8_t const descConfiguration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 0, 0, TUD_CONFIG_DESC_LEN, 0x00, 100)
};



/* ------- function implement ----------------------------------------------------------------------------------------*/

// 回调函数：返回设备描述符
uint8_t const* tudDescriptorDeviceCb() { return (uint8_t const*)&descDevice; }


// 回调：返回配置描述符
uint8_t const * tudDescriptorConfigurationCb(uint8_t index) {
    (void)index;
    return descConfiguration;
}

// 回调：字符串描述符, 简化处理，直接返回空
uint16_t const* tudDescriptorStringCb(uint8_t index, uint8_t langid) {
    (void) index;
    (void) langid;
    return NULL;
}


