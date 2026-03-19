/**
 *******************************************************************************
 * @file    referee-ui.h
 * @brief   裁判系统客户端 UI 绘制服务
 *******************************************************************************
 * @attention
 * 基于 FreeRTOS Queue 实现的无锁异步 UI 绘制接口。
 *******************************************************************************
 * @author  MekLi
 * @date    2026/3/19
 * @version 1.0
 *******************************************************************************
 */

#ifndef INFANTRY_REFEREE_UI_H
#define INFANTRY_REFEREE_UI_H

/* ------- include -----------------------------------------------------------*/
#include "FreeRTOS.h"
#include "System/DataHub/ui-protocol.h"
#include "System/Thread/application-base.h"
#include "queue.h"
#include "tools/crtp.h"

/* ------- class prototypes --------------------------------------------------*/

class UiRendererSrvc : public PeriodicApp, public Singleton<UiRendererSrvc> {
public:
    UiRendererSrvc();

    void init() override;
    void run() override;

    // --- 图形绘制 API (标准 Draw 命名语义) ---
    void drawLine     (const GraphicProperties& props, uint16_t endX, uint16_t endY);
    void drawRectangle(const GraphicProperties& props, uint16_t endX, uint16_t endY);
    void drawCircle   (const GraphicProperties& props, uint16_t radius);
    void drawEllipse  (const GraphicProperties& props, uint16_t radiusX, uint16_t radiusY);
    void drawArc      (const GraphicProperties& props, uint16_t radiusX, uint16_t radiusY, uint16_t startAngle, uint16_t endAngle);

    // --- 数值与文本渲染 API ---
    void drawFloat    (const GraphicProperties& props, uint16_t fontSize, float value);
    void drawInt      (const GraphicProperties& props, uint16_t fontSize, int32_t value);

    // --- 控制 API ---
    void clearGraphic (GraphicDelMode mode, const uint8_t* graphicName = nullptr);

private:
    QueueHandle_t _renderQueue; // 渲染管线指令队列 (Render Pipeline Queue)

    // 内部封装
    void _applyProperties(GraphicPayload& payload, const GraphicProperties& props, GraphicType type);
    void _submitToPipeline(const GraphicPayload& payload); // 提交到渲染管线
};

#endif /* INFANTRY_REFEREE_UI_H */