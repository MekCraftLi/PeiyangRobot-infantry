#pragma once

#include "uimaker/ui_command_buffer.h"

#include <cstddef>

namespace uimaker {

struct UiMakerConfig {
    uint16_t screenWidth{1920};
    uint16_t screenHeight{1080};
    uint16_t anchorX{1920 / 2};
    uint16_t anchorY{850};
    float capVoltageMax{28.0f};
    float spinRpsMax{3.0f};
};

class UiMakerCore {
  public:
    explicit UiMakerCore(UiMakerConfig cfg = {});

    template <size_t Capacity>
    void emitInit(UiCommandBuffer<Capacity>& out) {
        out.clear();
        _appendDeleteAll(out);
        _emitStaticTable(out, GraphicOption::Add);
        _emitInitPrimitives(out);
    }

    template <size_t Capacity>
    void emitTick(const UiStateInput& in, UiCommandBuffer<Capacity>& out) {
        out.clear();

        if (in.resetRequested) {
            emitInit(out);
            return;
        }

        const UiColor centerColor = in.shootEnabled ? UiColor::Green : UiColor::Pink;
        _appendCircle(out, {{6, 0, 0}}, GraphicOption::Update, centerColor, 0, 4, _cfg.screenWidth / 2,
                      _cfg.screenHeight / 2, 30);

        float ratioCap = (_cfg.capVoltageMax > 0.0f) ? (in.capVoltage / _cfg.capVoltageMax) : 0.0f;
        ratioCap = _clamp01(ratioCap);

        const float rps = _absf(in.gyroZ) / _kTwoPi;
        float ratioSpin = (_cfg.spinRpsMax > 0.0f) ? (rps / _cfg.spinRpsMax) : 0.0f;
        ratioSpin = _clamp01(ratioSpin);

        const uint16_t angleCap = static_cast<uint16_t>(ratioCap * 18.0f);
        const uint16_t angleSpin = static_cast<uint16_t>(ratioSpin * 80.0f);

        _appendArc(out, {{4, 0, 0}}, GraphicOption::Update, UiColor::Cyan, 0, 17, _cfg.screenWidth / 2, 944, 185,
                   static_cast<uint16_t>(185 + angleCap), 1000, 210);
        _appendArc(out, {{5, 0, 0}}, GraphicOption::Update, UiColor::Cyan, 0, 17, _cfg.screenWidth / 2, 944,
                   static_cast<uint16_t>(176 - angleCap), 175, 1000, 210);
        _appendFloat(out, {{1, 0, 0}}, GraphicOption::Update, UiColor::White, 0, 2, _cfg.screenWidth / 2 - 20, 750,
                     in.capVoltage);

        _appendArc(out, {{3, 0, 0}}, GraphicOption::Update, UiColor::Orange, 0, 10, 450, 0,
                   static_cast<uint16_t>(79 - angleSpin), 80, 400, 150);
        _appendFloat(out, {{2, 0, 0}}, GraphicOption::Update, UiColor::Orange, 0, 2, 600, 50, in.gyroZ / _kTwoPi);
    }

  private:
    static constexpr float _kTwoPi = 6.28318530717958647692f;

    UiMakerConfig _cfg{};
    uint8_t _figureIdx{0};

    template <size_t Capacity>
    static void _push(UiCommandBuffer<Capacity>& out, const UiCommand& cmd) {
        (void)out.push(cmd);
    }

    template <size_t Capacity>
    void _appendDeleteAll(UiCommandBuffer<Capacity>& out) {
        UiCommand cmd{};
        cmd.option = GraphicOption::Delete;
        cmd.name.value = {0xFF, 0xFF, 0xFF};
        _push(out, cmd);
    }

    template <size_t Capacity>
    void _appendLine(UiCommandBuffer<Capacity>& out, GraphicId name, GraphicOption opt, UiColor color, uint8_t layer,
                     uint16_t width, uint16_t startX, uint16_t startY, uint16_t endX, uint16_t endY) {
        UiCommand cmd{};
        cmd.name = name;
        cmd.option = opt;
        cmd.color = color;
        cmd.layer = layer;
        cmd.width = width;
        cmd.startX = startX;
        cmd.startY = startY;
        cmd.shape = CommandShape::Line;
        cmd.endX = endX;
        cmd.endY = endY;
        _push(out, cmd);
    }

    template <size_t Capacity>
    void _appendArc(UiCommandBuffer<Capacity>& out, GraphicId name, GraphicOption opt, UiColor color, uint8_t layer,
                    uint16_t width, uint16_t startX, uint16_t startY, uint16_t startAngle, uint16_t endAngle,
                    uint16_t radiusX, uint16_t radiusY) {
        UiCommand cmd{};
        cmd.name = name;
        cmd.option = opt;
        cmd.color = color;
        cmd.layer = layer;
        cmd.width = width;
        cmd.startX = startX;
        cmd.startY = startY;
        cmd.shape = CommandShape::Arc;
        cmd.arcStartAngle = startAngle;
        cmd.arcEndAngle = endAngle;
        cmd.radiusX = radiusX;
        cmd.radiusY = radiusY;
        _push(out, cmd);
    }

    template <size_t Capacity>
    void _appendCircle(UiCommandBuffer<Capacity>& out, GraphicId name, GraphicOption opt, UiColor color, uint8_t layer,
                       uint16_t width, uint16_t startX, uint16_t startY, uint16_t radius) {
        UiCommand cmd{};
        cmd.name = name;
        cmd.option = opt;
        cmd.color = color;
        cmd.layer = layer;
        cmd.width = width;
        cmd.startX = startX;
        cmd.startY = startY;
        cmd.shape = CommandShape::Circle;
        cmd.circleRadius = radius;
        _push(out, cmd);
    }

    template <size_t Capacity>
    void _appendFloat(UiCommandBuffer<Capacity>& out, GraphicId name, GraphicOption opt, UiColor color, uint8_t layer,
                      uint16_t width, uint16_t startX, uint16_t startY, float value) {
        UiCommand cmd{};
        cmd.name = name;
        cmd.option = opt;
        cmd.color = color;
        cmd.layer = layer;
        cmd.width = width;
        cmd.startX = startX;
        cmd.startY = startY;
        cmd.shape = CommandShape::FloatValue;
        cmd.floatValue = value;
        _push(out, cmd);
    }

    template <size_t Capacity>
    void _emitStaticTable(UiCommandBuffer<Capacity>& out, GraphicOption opt);

    template <size_t Capacity>
    void _emitInitPrimitives(UiCommandBuffer<Capacity>& out);

    static float _clamp01(float v);
    static float _absf(float v);
};

template <size_t Capacity>
void UiMakerCore::_emitStaticTable(UiCommandBuffer<Capacity>& out, GraphicOption opt) {
    static constexpr UiRelativeElementConfig kCustomUiTable[] = {
        {{'c', '0', '1'}, GraphicType::Arc, UiColor::Main, 0, 3, -14, 20, 20, 20, 130, 210},
        {{'c', '0', '2'}, GraphicType::Arc, UiColor::Main, 0, 3, 14, 20, 20, 20, 155, 225},

        {{'c', '0', '3'}, GraphicType::Arc, UiColor::Main, 0, 3, 3, -55, 40, 80, 298, 315},
        {{'c', '0', '4'}, GraphicType::Arc, UiColor::Main, 0, 3, -3, -55, 40, 80, 43, 62},

        {{'c', '0', '5'}, GraphicType::Arc, UiColor::Main, 0, 3, -35, -25, 10, 7, 15, 180},
        {{'c', '0', '6'}, GraphicType::Arc, UiColor::Main, 0, 3, 35, -25, 10, 7, 180, 345},

        {{'c', '0', '7'}, GraphicType::Arc, UiColor::Main, 0, 3, 7, -33, 40, 40, 230, 270},
        {{'c', '0', '8'}, GraphicType::Arc, UiColor::Main, 0, 3, -7, -33, 40, 40, 90, 130},

        {{'c', '0', '9'}, GraphicType::Arc, UiColor::Main, 0, 3, 91, 18, 150, 120, 217, 230},
        {{'c', '1', '0'}, GraphicType::Arc, UiColor::Main, 0, 3, -91, 18, 150, 120, 130, 143},

        {{'c', '1', '3'}, GraphicType::Line, UiColor::White, 0, 3, -20, -25, -6, -25, 0, 0},
        {{'c', '1', '4'}, GraphicType::Line, UiColor::White, 0, 3, -13, -25, -13, -45, 0, 0},
        {{'c', '1', '5'}, GraphicType::Line, UiColor::White, 0, 3, 0, -23, 0, -40, 0, 0},
        {{'c', '1', '6'}, GraphicType::Arc, UiColor::White, 0, 3, -5, -40, 5, 5, 80, 250},
        {{'c', '1', '7'}, GraphicType::Line, UiColor::White, 0, 3, 6, -23, 6, -40, 0, 0},
        {{'c', '1', '8'}, GraphicType::Line, UiColor::White, 0, 4, 20, -23, 20, -40, 0, 0},
        {{'c', '1', '9'}, GraphicType::Arc, UiColor::White, 0, 3, 13, -40, 7, 5, 80, 280},
    };

    for (const auto& cfg : kCustomUiTable) {
        const uint16_t startX = static_cast<uint16_t>(static_cast<int32_t>(_cfg.anchorX) + cfg.offsetX);
        const uint16_t startY = static_cast<uint16_t>(static_cast<int32_t>(_cfg.anchorY) + cfg.offsetY);
        if (cfg.type == GraphicType::Arc) {
            _appendArc(out, cfg.name, opt, cfg.color, cfg.layer, cfg.width, startX, startY, cfg.param1, cfg.param2,
                       static_cast<uint16_t>(cfg.detailX), static_cast<uint16_t>(cfg.detailY));
        } else if (cfg.type == GraphicType::Line) {
            const uint16_t endX = static_cast<uint16_t>(static_cast<int32_t>(_cfg.anchorX) + cfg.detailX);
            const uint16_t endY = static_cast<uint16_t>(static_cast<int32_t>(_cfg.anchorY) + cfg.detailY);
            _appendLine(out, cfg.name, opt, cfg.color, cfg.layer, cfg.width, startX, startY, endX, endY);
        }
    }
}

template <size_t Capacity>
void UiMakerCore::_emitInitPrimitives(UiCommandBuffer<Capacity>& out) {
    auto nextId = [this]() -> GraphicId { return {{0, 0, _figureIdx++}}; };

    _appendLine(out, nextId(), GraphicOption::Add, UiColor::Main, 0, 10, _cfg.screenWidth / 2 - 110, 835,
                _cfg.screenWidth / 2 - 80, 800);
    _appendLine(out, nextId(), GraphicOption::Add, UiColor::Main, 0, 10, _cfg.screenWidth / 2 + 110, 835,
                _cfg.screenWidth / 2 + 80, 800);
    _appendLine(out, nextId(), GraphicOption::Add, UiColor::Main, 0, 10, _cfg.screenWidth / 2 - 160, 835,
                _cfg.screenWidth / 2 - 130, 800);
    _appendLine(out, nextId(), GraphicOption::Add, UiColor::Main, 0, 10, _cfg.screenWidth / 2 + 160, 835,
                _cfg.screenWidth / 2 + 130, 800);
    _appendLine(out, nextId(), GraphicOption::Add, UiColor::Main, 0, 10, _cfg.screenWidth / 2 - 210, 835,
                _cfg.screenWidth / 2 - 180, 800);
    _appendLine(out, nextId(), GraphicOption::Add, UiColor::Main, 0, 10, _cfg.screenWidth / 2 + 210, 835,
                _cfg.screenWidth / 2 + 180, 800);

    _appendArc(out, nextId(), GraphicOption::Add, UiColor::White, 0, 10, 450, 0, 0, 80, 400, 120);
    _appendArc(out, nextId(), GraphicOption::Add, UiColor::White, 0, 10, 450, 0, 0, 80, 400, 130);
    _appendArc(out, nextId(), GraphicOption::Add, UiColor::White, 0, 10, 450, 0, 0, 80, 400, 140);
    _appendArc(out, nextId(), GraphicOption::Add, UiColor::White, 0, 10, 450, 0, 0, 80, 400, 150);
    _appendArc(out, {{3, 0, 0}}, GraphicOption::Add, UiColor::Orange, 0, 10, 450, 0, 0, 80, 400, 150);

    _appendArc(out, nextId(), GraphicOption::Add, UiColor::White, 0, 6, _cfg.screenWidth / 2, 950, 185, 203, 1000,
               210);
    _appendArc(out, nextId(), GraphicOption::Add, UiColor::White, 0, 6, _cfg.screenWidth / 2, 970, 196, 220, 1000,
               150);
    _appendArc(out, nextId(), GraphicOption::Add, UiColor::White, 0, 6, _cfg.screenWidth / 2, 970, 140, 164, 1000,
               150);

    _appendArc(out, {{4, 0, 0}}, GraphicOption::Add, UiColor::Cyan, 0, 17, _cfg.screenWidth / 2, 944, 185, 203, 1000,
               210);
    _appendArc(out, {{5, 0, 0}}, GraphicOption::Add, UiColor::Cyan, 0, 17, _cfg.screenWidth / 2, 944, 157, 175, 1000,
               210);

    _appendFloat(out, {{1, 0, 0}}, GraphicOption::Add, UiColor::White, 0, 2, _cfg.screenWidth / 2 - 20, 750, 0.0f);
    _appendFloat(out, {{2, 0, 0}}, GraphicOption::Add, UiColor::Orange, 0, 2, 600, 50, 0.0f);

    _appendCircle(out, {{6, 0, 0}}, GraphicOption::Add, UiColor::Pink, 0, 4, _cfg.screenWidth / 2,
                  _cfg.screenHeight / 2, 30);
}

} // namespace uimaker
