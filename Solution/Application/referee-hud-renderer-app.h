#ifndef INFANTRY_REFEREE_HUD_RENDERER_APP_H
#define INFANTRY_REFEREE_HUD_RENDERER_APP_H

#ifdef __cplusplus

#include "System/Thread/application-base.h"
#include "tools/crtp.h"
#include "ui-renderer-srvc.h"

class RefereeHudRendererApp final : public PeriodicApp, public Singleton<RefereeHudRendererApp> {
  public:
    RefereeHudRendererApp();

    void init() override;
    void run() override;

    UiRendererSrvc& renderer() { return _renderer; }

  private:
    UiRendererSrvc _renderer;
};

#endif

#endif
