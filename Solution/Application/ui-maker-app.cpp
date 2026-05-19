/**
 *******************************************************************************
 * @file    ui-maker-app.cpp
 * @brief   Referee UI application scheduler.
 *******************************************************************************
 */

#include "ui-maker-app.h"

#include "System/DataHub/blackboard.h"
#include "System/DataHub/data-def.h"

#include <cmath>

[[maybe_unused]] static auto& forceInit = UiMakerApp::instance();

#ifndef UI_MAKER_USE_SIM_INPUT
#define UI_MAKER_USE_SIM_INPUT 0
#endif

#define APPLICATION_ENABLE     true
#define APPLICATION_NAME       "UiMaker"
#define APPLICATION_STACK_SIZE 512
#define APPLICATION_PRIORITY   4

static StackType_t appStack[APPLICATION_STACK_SIZE];

namespace {

class BlackboardUiMakerInputSource final : public UiMakerInputSource {
  public:
    UiMakerInputSnapshot sample(float) override {
        GimbalToChassisComm comm{};
        SuperCapState capState{};
        Blackboard::instance().rComm.read(comm);
        Blackboard::instance().capState.read(capState);

        UiMakerInputSnapshot input{};
        input.capVoltage     = capState.voltage;
        input.capEnabled     = comm.msg.capSwitch != 0;
        input.capError       = capState.isError || capState.isCapLow;
        input.resetRequested = comm.msg.resetUI != 0;
        input.turboEnabled   = comm.msg.turboMode != 0;
        input.feederEnabled  = (comm.msg.fireState > 0) || (comm.msg.shootEn != 0);
        input.spinEnabled    = (comm.msg.mode & 0x03U) == CHASSIS_SPIN;
        input.legLengthState = RefereeHudSpec::normalizeLegLengthState(static_cast<uint8_t>(comm.msg.legLength));
        input.aimModeState   = RefereeHudSpec::normalizeAimModeState(static_cast<uint8_t>(comm.msg.aimMode));
        input.aimTargetState = comm.msg.shootEn != 0 ? static_cast<uint8_t>(RefereeHudAimTarget::Fire)
                                                     : static_cast<uint8_t>(RefereeHudAimTarget::None);
        RefereeHudSpec::fillDualLegPoseFromState(input);
        return input;
    }
};

class SimUiMakerInputSource final : public UiMakerInputSource {
  public:
    UiMakerInputSnapshot sample(float dt) override {
        _time += dt;
        while (_time >= kCycleSeconds) {
            _time -= kCycleSeconds;
        }
        _legTimer += dt;
        while (_legTimer >= kLegStateIntervalSeconds) {
            _legTimer -= kLegStateIntervalSeconds;
            _legLengthState = static_cast<uint8_t>((_legLengthState + 1) % 3);
        }
        _aimModeTimer += dt;
        while (_aimModeTimer >= kAimModeIntervalSeconds) {
            _aimModeTimer -= kAimModeIntervalSeconds;
            _aimModeState = static_cast<uint8_t>((_aimModeState + 1) % 4);
        }
        _switchTimer += dt;
        while (_switchTimer >= kSwitchIntervalSeconds) {
            _switchTimer -= kSwitchIntervalSeconds;
            _switchIndex = static_cast<uint8_t>((_switchIndex + 1) % 4);
        }
        _aimTargetTimer += dt;
        while (_aimTargetTimer >= kAimTargetIntervalSeconds) {
            _aimTargetTimer -= kAimTargetIntervalSeconds;
            _aimTargetState = static_cast<uint8_t>((_aimTargetState + 1) % 3);
        }

        const float halfCycle = kCycleSeconds * 0.5f;
        const float ratio     = (_time < halfCycle) ? (_time / halfCycle) : ((kCycleSeconds - _time) / halfCycle);
        const float legWave = std::sin(_time * 2.0f * RefereeHudSpec::kPi / kLegSignalCycleSeconds);

        UiMakerInputSnapshot input{};
        input.capVoltage     = 6.0f + 20.0f * ratio;
        input.capEnabled     = _time > 0.8f;
        input.capError       = input.capVoltage < RefereeHudSpec::kVoltageStage1;
        input.resetRequested = false;
        input.turboEnabled   = _switchIndex >= 1;
        input.feederEnabled  = _switchIndex >= 2;
        input.spinEnabled    = _switchIndex >= 3;
        input.legLengthState = _legLengthState;
        input.aimModeState   = _aimModeState;
        input.aimTargetState = _aimTargetState;
        input.leftLegHipWheelDistance =
            RefereeHudSpec::kWheelLegDistanceMidRaw +
            (RefereeHudSpec::kWheelLegDistanceMaxRaw - RefereeHudSpec::kWheelLegDistanceMinRaw) * 0.5f * legWave;
        input.leftLegThighAngleDeg = RefereeHudSpec::wheelLegAngleForDistance(input.leftLegHipWheelDistance);
        input.rightLegHipWheelDistance =
            RefereeHudSpec::kWheelLegDistanceMidRaw -
            (RefereeHudSpec::kWheelLegDistanceMaxRaw - RefereeHudSpec::kWheelLegDistanceMinRaw) * 0.5f * legWave;
        input.rightLegThighAngleDeg = RefereeHudSpec::wheelLegAngleForDistance(input.rightLegHipWheelDistance);
        return input;
    }

  private:
    static constexpr float kCycleSeconds = 8.0f;
    static constexpr float kLegStateIntervalSeconds = 2.0f;
    static constexpr float kAimModeIntervalSeconds = 2.0f;
    static constexpr float kSwitchIntervalSeconds = 1.0f;
    static constexpr float kAimTargetIntervalSeconds = 10.0f;
    static constexpr float kLegSignalCycleSeconds = 3.2f;
    float _time = 0.0f;
    float _legTimer = 0.0f;
    float _aimModeTimer = 0.0f;
    float _switchTimer = 0.0f;
    float _aimTargetTimer = 0.0f;
    uint8_t _legLengthState = 0;
    uint8_t _aimModeState = 0;
    uint8_t _switchIndex = 0;
    uint8_t _aimTargetState = 0;
};

UiMakerInputSource& defaultInputSource() {
#if UI_MAKER_USE_SIM_INPUT
    static SimUiMakerInputSource inputSource;
#else
    static BlackboardUiMakerInputSource inputSource;
#endif
    return inputSource;
}
} // namespace

UiMakerApp::UiMakerApp()
    : PeriodicApp(APPLICATION_ENABLE, APPLICATION_NAME, APPLICATION_STACK_SIZE, appStack, APPLICATION_PRIORITY, 50) {}

void UiMakerApp::init() {
    if (_inputSource == nullptr) {
        _inputSource = &defaultInputSource();
    }

    RefereeHudRendererApp::instance().waitInit();
    resetGraphics();
    _input = _inputSource->sample(0.0f);
    _hudUi.draw(_uiRender, _input);
}

void UiMakerApp::run() {
    if (_inputSource == nullptr) {
        _inputSource = &defaultInputSource();
    }

    _input = _inputSource->sample(RefereeHudUi::kPeriodSeconds);

    if (_input.resetRequested && !_lastResetRequested) {
        resetGraphics();
    }
    _lastResetRequested = _input.resetRequested;

    _hudUi.draw(_uiRender, _input);
}

void UiMakerApp::setInputSource(UiMakerInputSource& inputSource) {
    _inputSource = &inputSource;
    resetGraphics();
}

void UiMakerApp::resetGraphics() { _hudUi.reset(_uiRender); }
