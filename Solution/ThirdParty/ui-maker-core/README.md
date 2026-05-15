# ui-maker-core

Portable UI logic core extracted from `Solution/Application/ui-maker-app.cpp`.

## Goals

- Keep current project behavior logic (static UI + dynamic updates).
- Remove dependencies on RTOS/HAL/Blackboard/threading.
- Expose a command-buffer interface for any renderer adapter.

## Build

```bash
cmake -S . -B build
cmake --build build
cmake --install build --prefix <install-prefix>
```

## Consume

```cmake
find_package(ui-maker-core CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE uimaker::core)
```

## API Overview

- `uimaker::UiMakerCore`: main logic class.
- `uimaker::UiStateInput`: dynamic input (`shootEnabled`, `capVoltage`, `gyroZ`, `resetRequested`).
- `uimaker::UiCommandBuffer<N>`: fixed-capacity output buffer.
- `uimaker::UiCommand`: renderer-agnostic command description.
- `uimaker::UiRenderEngine`: queue-driven renderer + RM frame packer + UART sender callback.

## Thread / Queue / UART Mode

This library does not create threads by itself.

You provide:

1. Your own thread loop.
2. Your own input queue (`IRenderInputQueue`).
3. Your own UART send callback (`UartSendFn`).

Then call `engine.render()` in your thread.

```cpp
class MyQueue : public uimaker::IRenderInputQueue {
public:
    bool pop(uimaker::UiRenderInput& out) override;
};

bool uart_send(const uint8_t* data, uint16_t len, void* ctx);

uimaker::UiRenderEngine engine;
MyQueue queue;
engine.setQueue(&queue);
engine.setUartSender(&uart_send, nullptr);

for (;;) {
    (void)engine.render(); // pop -> generate ui -> pack RM frame -> uart send
}
```

Queue payload:

- `RenderPhase::Init`: generate init graphics and send.
- `RenderPhase::Tick`: generate dynamic graphics from `UiStateInput` and send.

## Integration Pattern

1. Push `UiRenderInput` message to your queue.
2. External thread calls `render()`.
3. Library packs 1/2/5/7 figure frames (or delete-all frame).
4. Library invokes your UART callback with ready-to-send bytes.
