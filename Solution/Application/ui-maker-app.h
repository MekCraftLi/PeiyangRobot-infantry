#ifndef INFANTRY_UI_MAKER_APP_H
#define INFANTRY_UI_MAKER_APP_H

#ifdef __cplusplus

#include "referee-hud-renderer-app.h"
#include "referee-hud-ui.h"
#include "System/Thread/application-base.h"
#include "tools/crtp.h"

using UiMakerInputSnapshot = RefereeHudInput;
using UiMakerInputSource = RefereeHudInputSource;

class UiMakerApp final : public PeriodicApp, public Singleton<UiMakerApp> {
  public:
    UiMakerApp();

    void init() override;
    void run() override;
    void setInputSource(UiMakerInputSource& inputSource);

  private:
    UiRendererSrvc& _uiRender = RefereeHudRendererApp::instance().renderer();
    RefereeHudUi _hudUi {};
    UiMakerInputSource* _inputSource = nullptr;
    UiMakerInputSnapshot _input {};
    bool _lastResetRequested = false;
    uint16_t _lastRendererSenderId = 0;

    bool syncRendererSenderId();
    void resetGraphics();
};

#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif
