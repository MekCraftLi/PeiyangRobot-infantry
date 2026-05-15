#pragma once

#include "uimaker/ui_maker_core.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace uimaker {

enum class RenderPhase : uint8_t {
    Init = 0,
    Tick = 1,
};

struct UiRenderInput {
    RenderPhase phase{RenderPhase::Tick};
    UiStateInput state{};
};

class IRenderInputQueue {
  public:
    virtual ~IRenderInputQueue() = default;
    virtual bool pop(UiRenderInput& out) = 0;
};

using UartSendFn = bool (*)(const uint8_t* data, uint16_t len, void* userCtx);

struct RmEndpoint {
    uint16_t senderId{3};
    uint16_t receiverId{0x0103};
};

class UiRenderEngine {
  public:
    explicit UiRenderEngine(UiMakerCore core = UiMakerCore{});

    void setQueue(IRenderInputQueue* queue) { _queue = queue; }
    void setUartSender(UartSendFn fn, void* userCtx) {
        _uartSend = fn;
        _uartCtx = userCtx;
    }
    void setEndpoint(RmEndpoint endpoint) { _endpoint = endpoint; }

    bool render();

    template <size_t Capacity>
    bool enqueueInitCommandFrames(std::array<std::array<uint8_t, 256>, Capacity>& outFrames,
                                  std::array<uint16_t, Capacity>& outLens, size_t& outCount) {
        UiCommandBuffer<128> buffer;
        _core.emitInit(buffer);
        return _commandsToFrames(buffer, outFrames, outLens, outCount);
    }

    template <size_t Capacity>
    bool enqueueTickCommandFrames(const UiStateInput& state, std::array<std::array<uint8_t, 256>, Capacity>& outFrames,
                                  std::array<uint16_t, Capacity>& outLens, size_t& outCount) {
        UiCommandBuffer<128> buffer;
        _core.emitTick(state, buffer);
        return _commandsToFrames(buffer, outFrames, outLens, outCount);
    }

  private:
#pragma pack(push, 1)
    struct RmFigurePayload {
        uint8_t graphicName[3];
        uint32_t opt    : 3;
        uint32_t type   : 3;
        uint32_t layer  : 4;
        uint32_t color  : 4;
        uint32_t param1 : 9;
        uint32_t param2 : 9;
        uint32_t width  : 10;
        uint32_t startX : 11;
        uint32_t startY : 11;
        uint32_t param3 : 10;
        uint32_t endX   : 11;
        uint32_t endY   : 11;
    };

    struct RmFrameHeader {
        uint8_t sof;
        uint16_t dataLength;
        uint8_t seq;
        uint8_t crc8;
    };

    struct RmInteractiveHeader {
        uint16_t subCmdId;
        uint16_t senderId;
        uint16_t receiverId;
    };

    struct LayerDeletePayload {
        uint8_t deleteType;
        uint8_t layer;
    };
#pragma pack(pop)

    static_assert(sizeof(RmFigurePayload) == 15, "RmFigurePayload must be 15 bytes");
    static_assert(sizeof(RmFrameHeader) == 5, "RmFrameHeader must be 5 bytes");
    static_assert(sizeof(RmInteractiveHeader) == 6, "RmInteractiveHeader must be 6 bytes");
    static_assert(sizeof(LayerDeletePayload) == 2, "LayerDeletePayload must be 2 bytes");

    UiMakerCore _core{};
    IRenderInputQueue* _queue{nullptr};
    UartSendFn _uartSend{nullptr};
    void* _uartCtx{nullptr};
    uint8_t _seqCounter{0};
    RmEndpoint _endpoint{};

    template <size_t Capacity>
    bool _commandsToFrames(const UiCommandBuffer<Capacity>& commands, std::array<std::array<uint8_t, 256>, Capacity>& outFrames,
                           std::array<uint16_t, Capacity>& outLens, size_t& outCount);

    template <size_t Capacity>
    static size_t _packFigureBatch(const UiCommandBuffer<Capacity>& commands, size_t startIdx, size_t batchCount,
                                   std::array<RmFigurePayload, 7>& outBatch);

    static RmFigurePayload _toPayload(const UiCommand& cmd);

    uint16_t _subCmdFromBatchSize(size_t batchSize) const;
    uint16_t _buildFrame(uint16_t subCmdId, const void* payload, uint16_t payloadLen, uint8_t* outFrame, uint16_t outCap);

    static uint8_t _crc8(const uint8_t* data, uint32_t len);
    static uint16_t _crc16(const uint8_t* data, uint32_t len);
    static void _appendCrc8(uint8_t* frame, uint32_t len);
    static void _appendCrc16(uint8_t* frame, uint32_t len);
};

template <size_t Capacity>
bool UiRenderEngine::_commandsToFrames(const UiCommandBuffer<Capacity>& commands, std::array<std::array<uint8_t, 256>, Capacity>& outFrames,
                                       std::array<uint16_t, Capacity>& outLens, size_t& outCount) {
    outCount = 0;
    size_t idx = 0;

    while (idx < commands.size()) {
        if (outCount >= Capacity) {
            return false;
        }

        const UiCommand& first = commands[idx];
        if (first.option == GraphicOption::Delete && first.name.value[0] == 0xFF && first.name.value[1] == 0xFF) {
            LayerDeletePayload del{2, 0};
            const uint16_t len = _buildFrame(0x0100, &del, sizeof(del), outFrames[outCount].data(),
                                             static_cast<uint16_t>(outFrames[outCount].size()));
            if (len == 0) {
                return false;
            }
            outLens[outCount++] = len;
            ++idx;
            continue;
        }

        const size_t remain = commands.size() - idx;
        size_t batch = 1;
        if (remain >= 7) {
            batch = 7;
        } else if (remain >= 5) {
            batch = 5;
        } else if (remain >= 2) {
            batch = 2;
        }

        std::array<RmFigurePayload, 7> payloadBatch{};
        const size_t packed = _packFigureBatch(commands, idx, batch, payloadBatch);
        if (packed == 0) {
            return false;
        }

        const uint16_t subCmd = _subCmdFromBatchSize(packed);
        const uint16_t payloadLen = static_cast<uint16_t>(packed * sizeof(RmFigurePayload));
        const uint16_t len = _buildFrame(subCmd, payloadBatch.data(), payloadLen, outFrames[outCount].data(),
                                         static_cast<uint16_t>(outFrames[outCount].size()));
        if (len == 0) {
            return false;
        }
        outLens[outCount++] = len;
        idx += packed;
    }

    return true;
}

template <size_t Capacity>
size_t UiRenderEngine::_packFigureBatch(const UiCommandBuffer<Capacity>& commands, size_t startIdx, size_t batchCount,
                                        std::array<RmFigurePayload, 7>& outBatch) {
    size_t packed = 0;
    for (size_t i = 0; i < batchCount && (startIdx + i) < commands.size(); ++i) {
        const UiCommand& cmd = commands[startIdx + i];
        if (cmd.option == GraphicOption::Delete && cmd.name.value[0] == 0xFF && cmd.name.value[1] == 0xFF) {
            break;
        }
        outBatch[packed++] = _toPayload(cmd);
    }
    return packed;
}

} // namespace uimaker
