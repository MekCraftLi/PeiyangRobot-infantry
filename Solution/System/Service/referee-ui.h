/**
 *******************************************************************************
 * @file    referee-ui.h
 * @brief   RoboMaster 裁判系统 UI 绘制接口适配器
 *******************************************************************************
 * @attention
 * 基于 Bsp-Figure-Driver 的逻辑重构，采用 Modern C++ 风格封装。
 * 提供流式接口 (Fluent Interface) 和类型安全的枚举定义。
 *******************************************************************************
 */

#ifndef INFANTRY_SYSTEM_SERVICE_REFEREE_UI_H
#define INFANTRY_SYSTEM_SERVICE_REFEREE_UI_H

#include <cstdint>
#include <cstring>
#include <functional>
#include <string_view>
#include <type_traits>

#include "System/DataHub/referee-protocol.h"

namespace System::Service::Referee {

/**
 * @brief UI 颜色定义
 */
enum class UiColor : uint32_t {
    Main   = 0, // 红/蓝 (取决于自身阵营)
    Yellow = 1,
    Green  = 2,
    Orange = 3,
    Purple = 4,
    Pink   = 5,
    Cyan   = 6,
    Black  = 7,
    White  = 8
};

/**
 * @brief 图形操作类型
 */
enum class UiOp : uint32_t {
    Null   = 0,
    Add    = 1,
    Modify = 2,
    Delete = 3
};

/**
 * @brief 图形形状类型
 */
enum class UiShape : uint32_t {
    Line      = 0,
    Rectangle = 1,
    Circle    = 2,
    Ellipse   = 3,
    Arc       = 4,
    Float     = 5,
    Int       = 6,
    String    = 7
};

/**
 * @brief UI 绘制构建器
 * @note  非线程安全，建议在主循环或特定任务中使用
 */
class UiBuilder {
public:
    using SendCallback = std::function<void(const uint8_t* data, uint16_t len, uint16_t cmd_id)>;

    explicit UiBuilder(SendCallback sender) : _sender(std::move(sender)) {}

    /**
     * @brief 开始绘制一个图形
     * @param name 图形名称 (必须为 3 个字符)
     * @param op   操作类型 (默认为 Add)
     */
    class Figure {
    public:
        Figure(UiBuilder& parent, const char name[3], UiOp op);

        // --- 属性设置 (链式调用) ---

        /** 设置颜色 */
        Figure& color(UiColor c);
        /** 设置线宽 */
        Figure& width(uint32_t w);
        /** 设置图层 (0-9) */
        Figure& layer(uint32_t l);

        // --- 具体图形绘制 (终结操作) ---

        /** 绘制直线 */
        void line(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2);

        /** 绘制矩形 */
        void rectangle(uint32_t left, uint32_t top, uint32_t right, uint32_t bottom);

        /** 绘制圆 */
        void circle(uint32_t centerX, uint32_t centerY, uint32_t radius);

        /** 绘制椭圆 */
        void ellipse(uint32_t centerX, uint32_t centerY, uint32_t radiusX, uint32_t radiusY);

        /** 绘制圆弧 */
        void arc(uint32_t centerX, uint32_t centerY, uint32_t radiusX, uint32_t radiusY, uint32_t startAngle, uint32_t endAngle);

        /** 绘制浮点数 */
        void number(uint32_t x, uint32_t y, float val, uint32_t fontSize = 20);

        /** 绘制整数 */
        void number(uint32_t x, uint32_t y, int32_t val, uint32_t fontSize = 20);

        /** 绘制字符串 */
        void string(uint32_t x, uint32_t y, const char* text, uint32_t fontSize = 20);

    private:
        UiBuilder& _parent;
        RMInteractionFigure _data;

        void _send(UiShape type);
        void _sendString(const char* text);
    };

    /**
     * @brief 创建一个图形对象
     */
    Figure shape(const char name[3], UiOp op = UiOp::Add);

    /**
     * @brief 全局删除操作
     */
    void deleteLayer(uint8_t layer);
    void deleteAll();

private:
    SendCallback _sender;

    // 辅助函数：打包交互数据头
    template <typename T>
    void _packAndSend(uint16_t subCmdId, const T& body);
};

} // namespace System::Service::Referee

#endif // INFANTRY_SYSTEM_SERVICE_REFEREE_UI_H

