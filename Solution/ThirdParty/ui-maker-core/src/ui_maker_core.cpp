#include "uimaker/ui_maker_core.h"

namespace uimaker {

UiMakerCore::UiMakerCore(UiMakerConfig cfg) : _cfg(cfg) {}

float UiMakerCore::_clamp01(float v) {
    if (v < 0.0f) {
        return 0.0f;
    }
    if (v > 1.0f) {
        return 1.0f;
    }
    return v;
}

float UiMakerCore::_absf(float v) { return (v >= 0.0f) ? v : -v; }

} // namespace uimaker
