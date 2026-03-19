/**
 *******************************************************************************
 * @file    ui-renderer-srvc.cpp
 * @brief   裁判系统 UI 渲染服务实现 (UI Renderer Service)
 *******************************************************************************
 * @attention
 * 实现了无锁的图形渲染指令入队，以及自适应 1/2/5/7 批量封包发送机制。
 *******************************************************************************
 * @author  MekLi
 * @date    2026/3/19
 * @version 1.0
 *******************************************************************************
 */

/* ------- include -----------------------------------------------------------*/
#include "ui-renderer-srvc.h"

#include "Config/Chassis/hw-config.h"
#include "System/DataHub/referee-data-hub.h" // 假设这里包含底层的发包接口
#include "tools/crc.h"

#include <cstring>

/* ------- variables ---------------------------------------------------------*/

[[maybe_unused]] static auto& forceInit = UiRendererSrvc::instance();
// 定义 DMA 专用的发送缓冲区，放置在不可 Cache 的内存段中防 DMA 脏数据
__attribute__((section(".dma_pool"))) static uint8_t uiTxBuffer[256];


#define APPLICATION_ENABLE     true
#define APPLICATION_NAME       "UiRenderer"
#define APPLICATION_STACK_SIZE 1024
#define APPLICATION_PRIORITY   4 // 渲染服务需要较高的优先级，保证UI不卡顿

static StackType_t appStack[APPLICATION_STACK_SIZE];

/* ------- function implement ------------------------------------------------*/

// 构造函数：10ms 周期 (100Hz)，高频消费队列，防止指令积压
UiRendererSrvc::UiRendererSrvc()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 50) {
}

void UiRendererSrvc::init() {
    // 创建深度为 35 的渲染管线队列 (足够缓存 5 帧 7图形满载包)
    _renderQueue = xQueueCreate(35, sizeof(GraphicPayload));
}

void UiRendererSrvc::run() {
   if (_renderQueue == nullptr) return;

    UBaseType_t waitingCount = uxQueueMessagesWaiting(_renderQueue);
    if (waitingCount == 0) return;

    // --- 1. 大疆自适应批量打包算法 ---
    uint8_t batchSize = 0;
    uint16_t subCmdId = 0;

    if (waitingCount >= 7) {
        batchSize = 7; subCmdId = 0x0104;
    } else if (waitingCount >= 5) {
        batchSize = 5; subCmdId = 0x0103;
    } else if (waitingCount >= 2) {
        batchSize = 2; subCmdId = 0x0102;
    } else {
        batchSize = 1; subCmdId = 0x0101;
    }

    // 从队列中取出图形
    GraphicPayload payloadBatch[7];
    for (uint8_t i = 0; i < batchSize; ++i) {
        xQueueReceive(_renderQueue, &payloadBatch[i], 0);
    }

    // --- 2. 协议封装 (Protocol Encapsulation) ---
    static uint8_t seqCounter = 0;
    uint16_t payloadLen = sizeof(GraphicPayload) * batchSize;
    uint16_t dataSegmentLen = sizeof(RmInteractiveHeader) + payloadLen;

    // 2.1 获取当前机器人 ID 与操作手客户端 ID (此处需根据您项目的实际获取方式调整)
    RMRobotStatus robotStatus{};
    RefereeDataHub::instance().robotStatus.read(robotStatus);
    uint16_t senderId   = robotStatus.robotId;
    uint16_t receiverId = robotStatus.robotId + 0x0100; // 规则: 操作手客户端 ID = 机器人 ID + 0x0100

    uint32_t offset = 0;

    // 2.2 组装帧头 (Frame Header)
    RmFrameHeader header{};
    header.sof = 0xA5;
    header.dataLength = dataSegmentLen;
    header.seq = seqCounter++;
    // 追加 CRC8
    std::memcpy(&uiTxBuffer[offset], &header, 4); // 先拷前 4 字节
    Crc::appendCrc8(uiTxBuffer, 5);               // 计算并追加第 5 字节的 CRC8
    offset += sizeof(RmFrameHeader);

    // 2.3 组装命令码 (CmdID) -> UI交互指令固定为 0x0301
    uint16_t cmdId = 0x0301;
    std::memcpy(&uiTxBuffer[offset], &cmdId, sizeof(cmdId));
    offset += sizeof(cmdId);

    // 2.4 组装交互头 (Interactive Header)
    RmInteractiveHeader interactHeader{};
    interactHeader.subCmdId   = subCmdId;
    interactHeader.senderId   = senderId;
    interactHeader.receiverId = receiverId;
    std::memcpy(&uiTxBuffer[offset], &interactHeader, sizeof(RmInteractiveHeader));
    offset += sizeof(RmInteractiveHeader);

    // 2.5 组装图形载荷 (Graphic Payload)
    std::memcpy(&uiTxBuffer[offset], payloadBatch, payloadLen);
    offset += payloadLen;

    // 2.6 组装整帧 CRC16 校验码 (整帧长度 = 当前 offset + 2 字节 CRC16)
    uint16_t frameTotalLength = offset + 2;
    Crc::appendCrc16(uiTxBuffer, frameTotalLength);

    // --- 3. 触发底层串口 DMA 发送 ---
    // (需替换为您 config.h 中实际的裁判系统 UART 外设句柄)
    HAL_UART_Transmit_DMA(&Config::Hardware::Comms::REFEREE_UART, uiTxBuffer, frameTotalLength);
}

// -----------------------------------------------------------------------------
// 内部渲染管线工具
// -----------------------------------------------------------------------------

void UiRendererSrvc::_applyProperties(GraphicPayload& payload, const GraphicProperties& props, GraphicType type) {
    std::memcpy(payload.graphicName, props.name, 3);
    payload.action = static_cast<uint32_t>(props.action);
    payload.type   = static_cast<uint32_t>(type);
    payload.layer  = props.layer;
    payload.color  = static_cast<uint32_t>(props.color);
    payload.lineWidth = props.lineWidth;
    payload.startX = props.startX;
    payload.startY = props.startY;

    // 初始化其余位域，防止脏数据
    payload.param1 = 0; payload.param2 = 0;
    payload.param3 = 0; payload.endX   = 0; payload.endY = 0;
}

void UiRendererSrvc::_submitToPipeline(const GraphicPayload& payload) {
    if (_renderQueue != nullptr) {
        // 非阻塞压入渲染管线，队列满则丢弃当前帧 (防死锁保护)
        xQueueSend(_renderQueue, &payload, 0);
    }
}

// -----------------------------------------------------------------------------
// UI 图形绘制 API 实现
// -----------------------------------------------------------------------------

void UiRendererSrvc::drawLine(const GraphicProperties& props, uint16_t endX, uint16_t endY) {
    GraphicPayload payload{};
    _applyProperties(payload, props, GraphicType::Line);
    payload.endX = endX;
    payload.endY = endY;
    _submitToPipeline(payload);
}

void UiRendererSrvc::drawRectangle(const GraphicProperties& props, uint16_t endX, uint16_t endY) {
    GraphicPayload payload{};
    _applyProperties(payload, props, GraphicType::Rectangle);
    payload.endX = endX;
    payload.endY = endY;
    _submitToPipeline(payload);
}

void UiRendererSrvc::drawCircle(const GraphicProperties& props, uint16_t radius) {
    GraphicPayload payload{};
    _applyProperties(payload, props, GraphicType::Circle);
    // RM协议中，画圆的半径存在 detailsD (重构后的 endX) 中
    payload.endX = radius;
    _submitToPipeline(payload);
}

void UiRendererSrvc::drawEllipse(const GraphicProperties& props, uint16_t radiusX, uint16_t radiusY) {
    GraphicPayload payload{};
    _applyProperties(payload, props, GraphicType::Ellipse);
    payload.endX = radiusX; // 原 detailsD
    payload.endY = radiusY; // 原 detailsE
    _submitToPipeline(payload);
}

void UiRendererSrvc::drawArc(const GraphicProperties& props, uint16_t radiusX, uint16_t radiusY, uint16_t startAngle, uint16_t endAngle) {
    GraphicPayload payload{};
    _applyProperties(payload, props, GraphicType::Arc);
    payload.param1 = startAngle; // 原 detailsA
    payload.param2 = endAngle;   // 原 detailsB
    payload.endX   = radiusX;    // 原 detailsD
    payload.endY   = radiusY;    // 原 detailsE
    _submitToPipeline(payload);
}

// -----------------------------------------------------------------------------
// 数值与文本渲染 API 实现
// -----------------------------------------------------------------------------

void UiRendererSrvc::drawFloat(const GraphicProperties& props, uint16_t fontSize, float value) {
    GraphicPayload payload{};
    _applyProperties(payload, props, GraphicType::Float);
    payload.param1 = fontSize; // 原 detailsA

    // 浮点数拆解：乘 1000 后转整型，拆分到 3 个字段中
    uint32_t valTemp = static_cast<uint32_t>(value * 1000.0f);
    payload.param3 = valTemp & 0x3FF;             // 原 detailsC (10 bit)
    payload.endX   = (valTemp >> 10) & 0x7FF;     // 原 detailsD (11 bit)
    payload.endY   = (valTemp >> 21) & 0x7FF;     // 原 detailsE (11 bit)

    _submitToPipeline(payload);
}

void UiRendererSrvc::drawInt(const GraphicProperties& props, uint16_t fontSize, int32_t value) {
    GraphicPayload payload{};
    _applyProperties(payload, props, GraphicType::Int);
    payload.param1 = fontSize; // 原 detailsA

    // 整数拆解
    payload.param3 = value & 0x3FF;               // 原 detailsC (10 bit)
    payload.endX   = (value >> 10) & 0x7FF;       // 原 detailsD (11 bit)
    payload.endY   = (value >> 21) & 0x7FF;       // 原 detailsE (11 bit)

    _submitToPipeline(payload);
}

// -----------------------------------------------------------------------------
// 控制 API 实现
// -----------------------------------------------------------------------------

void UiRendererSrvc::clearGraphic(GraphicDelMode mode, const uint8_t* graphicName) {
    GraphicPayload payload{};
    payload.action = static_cast<uint32_t>(GraphicAction::Delete);

    switch (mode) {
        case GraphicDelMode::Name:
            if (graphicName) { std::memcpy(payload.graphicName, graphicName, 3); }
            break;

        case GraphicDelMode::Layer:
            payload.color = static_cast<uint32_t>(UiColor::Del);
            payload.layer = graphicName ? graphicName[0] : 0; // 借用 graphicName[0] 传图层号
            break;

        case GraphicDelMode::All:
            payload.color = static_cast<uint32_t>(UiColor::Del);
            break;

        default:
            break;
    }

    _submitToPipeline(payload);
}