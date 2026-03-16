/**
 *******************************************************************************
 * @file    referee-ui.cpp
 * @brief   RoboMaster 裁判系统 UI 绘制接口适配器实现
 *******************************************************************************
 */

#include "referee-ui.h"

#include <cmath>
#include <cstring>

namespace System::Service::Referee {

/* ================================================================================================================== */
/*                                            UiBuilder Implementation                                                */
/* ================================================================================================================== */

template <typename T>
void UiBuilder::_packAndSend(uint16_t subCmdId, const T& body) {
    if (!_sender) return;

    // 1. 准备缓冲区 (Interaction Header + Body)
    // 根据协议手册，单次图形交互包最大约 128 字节足够
    static uint8_t buffer[128];
    if (sizeof(RMInteractionHeader) + sizeof(T) > sizeof(buffer)) return;

    auto* header = reinterpret_cast<RMInteractionHeader*>(buffer);

    // 2. 填充 Interaction Header (0x0301 内容头部)
    header->dataCmdId = subCmdId;

    // 发送者/接收者 ID:
    // 实际上 RoboMaster 协议要求填入 Robot ID 和 Client ID
    // 由于 UiBuilder 本身不持有这些状态，这里暂时留空，或者依赖 Caller 在 Sender 回调中覆写
    header->senderId   = 0;
    header->receiverId = 0;

    // 3. 填充 Body
    std::memcpy(buffer + sizeof(RMInteractionHeader), &body, sizeof(T));

    // 4. 发送 (0x0301)
    _sender(buffer, sizeof(RMInteractionHeader) + sizeof(T), 0x0301);
}

UiBuilder::Figure UiBuilder::shape(const char name[3], UiOp op) {
    return Figure(*this, name, op);
}

void UiBuilder::deleteLayer(uint8_t layer) {
    RMInteractionLayerDelete payload;
    payload.deleteType = 1; // Delete Layer
    payload.layer      = layer;
    _packAndSend(0x0100, payload);
}

void UiBuilder::deleteAll() {
    RMInteractionLayerDelete payload;
    payload.deleteType = 2; // Delete All
    payload.layer      = 0;
    _packAndSend(0x0100, payload);
}

/* ================================================================================================================== */
/*                                            UiBuilder::Figure Implementation                                        */
/* ================================================================================================================== */

UiBuilder::Figure::Figure(UiBuilder& parent, const char name[3], UiOp op) : _parent(parent) {
    std::memset(&_data, 0, sizeof(_data));
    std::memcpy(_data.figureName, name, 3);

    _data.operateType = static_cast<uint32_t>(op);
    _data.figureType  = 0;
    _data.layer       = 0;
    _data.color       = static_cast<uint32_t>(UiColor::Main);
    _data.width       = 1;
}

UiBuilder::Figure& UiBuilder::Figure::color(UiColor c) {
    _data.color = static_cast<uint32_t>(c);
    return *this;
}

UiBuilder::Figure& UiBuilder::Figure::width(uint32_t w) {
    _data.width = w;
    return *this;
}

UiBuilder::Figure& UiBuilder::Figure::layer(uint32_t l) {
    _data.layer = l;
    return *this;
}

void UiBuilder::Figure::line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) {
    _data.startX   = x1;
    _data.startY   = y1;
    _data.detailsD = x2;
    _data.detailsE = y2;
    _send(UiShape::Line);
}

void UiBuilder::Figure::rectangle(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom) {
    _data.startX   = left;
    _data.startY   = top;
    _data.detailsD = right;
    _data.detailsE = bottom;
    _send(UiShape::Rectangle);
}

void UiBuilder::Figure::circle(uint32_t centerX, uint32_t centerY, uint32_t radius) {
    _data.startX   = centerX;
    _data.startY   = centerY;
    _data.detailsD = radius;
    _send(UiShape::Circle);
}

void UiBuilder::Figure::ellipse(uint32_t centerX, uint32_t centerY, uint32_t radiusX, uint32_t radiusY) {
    _data.startX   = centerX;
    _data.startY   = centerY;
    _data.detailsD = radiusX;
    _data.detailsE = radiusY;
    _send(UiShape::Ellipse);
}

void UiBuilder::Figure::arc(uint32_t centerX, uint32_t centerY, uint32_t radiusX, uint32_t radiusY, uint32_t startAngle,
                            uint32_t endAngle) {
    _data.startX   = centerX;
    _data.startY   = centerY;
    _data.detailsD = radiusX;
    _data.detailsE = radiusY;
    _data.detailsA = startAngle;
    _data.detailsB = endAngle;
    _send(UiShape::Arc);
}

void UiBuilder::Figure::number(uint32_t x, uint32_t y, float val, uint32_t fontSize) {
    _data.startX   = x;
    _data.startY   = y;
    _data.detailsA = fontSize;

    int32_t intVal = static_cast<int32_t>(val * 1000.0f);
    _data.detailsC = intVal & 0x3FF;
    _data.detailsD = (intVal >> 10) & 0x7FF;
    _data.detailsE = (intVal >> 21) & 0x7FF;

    _send(UiShape::Float);
}

void UiBuilder::Figure::number(uint32_t x, uint32_t y, int32_t val, uint32_t fontSize) {
    _data.startX   = x;
    _data.startY   = y;
    _data.detailsA = fontSize;

    _data.detailsC = val & 0x3FF;
    _data.detailsD = (val >> 10) & 0x7FF;
    _data.detailsE = (val >> 21) & 0x7FF;

    _send(UiShape::Int);
}

// 字符串 Payload 结构
struct StringPayload {
    RMInteractionFigure base;
    uint8_t str[30];
} __attribute__((packed));

void UiBuilder::Figure::string(uint32_t x, uint32_t y, const char* text, uint32_t fontSize) {
    _data.startX   = x;
    _data.startY   = y;
    _data.detailsA = fontSize;
    _data.detailsB = std::strlen(text);

    _data.figureType = static_cast<uint32_t>(UiShape::String);

    _sendString(text);
}

void UiBuilder::Figure::_send(UiShape type) {
    _data.figureType = static_cast<uint32_t>(type);
    _parent._packAndSend(0x0101, _data);
}

void UiBuilder::Figure::_sendString(const char* text) {
    StringPayload payload;
    std::memset(&payload, 0, sizeof(payload));

    payload.base = _data;
    std::strncpy(reinterpret_cast<char*>(payload.str), text, 30);

    _parent._packAndSend(0x0110, payload);
}

} // namespace System::Service::Referee

