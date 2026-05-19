#include "referee-hud-renderer-app.h"

#include "Config/Chassis/hw-config.h"

[[maybe_unused]] static auto& forceInit = RefereeHudRendererApp::instance();

#define APPLICATION_ENABLE     true
#define APPLICATION_NAME       "UiRenderer"
#define APPLICATION_STACK_SIZE 1024
#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];

namespace {

__attribute__((section(".dma_pool"))) uint8_t refereeUiTxBuffer[256];

bool transmitRefereeUiPacket(const uint8_t* data, uint16_t length, void*) {
    if (data == nullptr || length > sizeof(refereeUiTxBuffer)) {
        return false;
    }

    return HAL_UART_Transmit_DMA(&Config::Hardware::Comms::REFEREE_SYSTEM_UART, const_cast<uint8_t*>(data), length) ==
           HAL_OK;
}

} // namespace

RefereeHudRendererApp::RefereeHudRendererApp()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 50),
      _renderer({transmitRefereeUiPacket, nullptr, 3, 35, refereeUiTxBuffer, sizeof(refereeUiTxBuffer)}) {}

void RefereeHudRendererApp::init() {
    _renderer.init();
}

void RefereeHudRendererApp::run() { _renderer.run(); }
