#pragma once

#include <array>
#include <cstdint>

namespace uimaker {

enum class GraphicType : uint8_t {
    Line      = 0,
    Rectangle = 1,
    Circle    = 2,
    Ellipse   = 3,
    Arc       = 4,
    Float     = 5,
    Int       = 6,
    String    = 7
};

enum class GraphicOption : uint8_t {
    Null   = 0,
    Add    = 1,
    Update = 2,
    Delete = 3
};

enum class UiColor : uint8_t {
    Main   = 0,
    Yellow = 1,
    Green  = 2,
    Orange = 3,
    Purple = 4,
    Pink   = 5,
    Cyan   = 6,
    Black  = 7,
    White  = 8,
    Del    = 9
};

enum class CommandShape : uint8_t {
    None = 0,
    Line,
    Arc,
    Circle,
    FloatValue,
};

struct GraphicId {
    std::array<uint8_t, 3> value{0, 0, 0};
};

struct UiCommand {
    GraphicId name{};
    GraphicOption option{GraphicOption::Add};
    UiColor color{UiColor::Main};
    uint8_t layer{0};
    uint16_t width{1};
    uint16_t startX{0};
    uint16_t startY{0};

    CommandShape shape{CommandShape::None};

    uint16_t endX{0};
    uint16_t endY{0};
    uint16_t arcStartAngle{0};
    uint16_t arcEndAngle{0};
    uint16_t radiusX{0};
    uint16_t radiusY{0};
    uint16_t circleRadius{0};
    float floatValue{0.0f};
};

struct UiRelativeElementConfig {
    GraphicId name{};
    GraphicType type{GraphicType::Line};
    UiColor color{UiColor::Main};
    uint8_t layer{0};
    uint16_t width{1};

    int16_t offsetX{0};
    int16_t offsetY{0};

    int16_t detailX{0};
    int16_t detailY{0};

    uint16_t param1{0};
    uint16_t param2{0};
};

struct UiStateInput {
    bool shootEnabled{false};
    bool resetRequested{false};
    float capVoltage{0.0f};
    float gyroZ{0.0f};
};

} // namespace uimaker
