#include "uimaker/uimaker.h"

#include <cstdio>
#include <deque>

class DemoQueue final : public uimaker::IRenderInputQueue {
  public:
    void push(const uimaker::UiRenderInput& in) { _q.push_back(in); }

    bool pop(uimaker::UiRenderInput& out) override {
        if (_q.empty()) {
            return false;
        }
        out = _q.front();
        _q.pop_front();
        return true;
    }

  private:
    std::deque<uimaker::UiRenderInput> _q;
};

static bool uartSend(const uint8_t* data, uint16_t len, void*) {
    std::printf("uart frame len=%u sof=0x%02X\n", static_cast<unsigned>(len), static_cast<unsigned>(data[0]));
    return true;
}

int main() {
    DemoQueue queue;
    uimaker::UiRenderEngine engine;
    engine.setQueue(&queue);
    engine.setUartSender(&uartSend, nullptr);

    uimaker::UiRenderInput initMsg{};
    initMsg.phase = uimaker::RenderPhase::Init;
    queue.push(initMsg);

    uimaker::UiRenderInput tickMsg{};
    tickMsg.phase = uimaker::RenderPhase::Tick;
    tickMsg.state.shootEnabled = true;
    tickMsg.state.capVoltage = 24.5f;
    tickMsg.state.gyroZ = 8.0f;
    queue.push(tickMsg);

    while (engine.render()) {
    }

    return 0;
}
