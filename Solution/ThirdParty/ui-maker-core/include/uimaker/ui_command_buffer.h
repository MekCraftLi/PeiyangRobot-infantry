#pragma once

#include "uimaker/ui_types.h"

#include <array>
#include <cstddef>

namespace uimaker {

template <size_t Capacity>
class UiCommandBuffer {
  public:
    void clear() { _size = 0; }

    bool push(const UiCommand& cmd) {
        if (_size >= Capacity) {
            return false;
        }
        _data[_size++] = cmd;
        return true;
    }

    [[nodiscard]] const UiCommand* data() const { return _data.data(); }
    [[nodiscard]] size_t size() const { return _size; }
    [[nodiscard]] static constexpr size_t capacity() { return Capacity; }

    [[nodiscard]] const UiCommand& operator[](size_t i) const { return _data[i]; }

  private:
    std::array<UiCommand, Capacity> _data{};
    size_t _size{0};
};

} // namespace uimaker
