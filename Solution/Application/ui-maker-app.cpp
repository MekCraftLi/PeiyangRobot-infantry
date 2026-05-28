/**
 *******************************************************************************
 * @file    ui-maker-app.cpp
 * @brief   Referee UI application scheduler.
 *******************************************************************************
 */

#include "ui-maker-app.h"

#include "System/DataHub/blackboard.h"
#include "System/DataHub/data-def.h"
#include "System/DataHub/referee-data-hub.h"

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

float wheelLegDistanceRatioFromState(uint8_t state) {
    switch (state % 3U) {
        case 0:
            return RefereeHudSpec::kWheelLegDistanceMinRatio;
        case 1:
            return RefereeHudSpec::kWheelLegDistanceMidRatio;
        default:
            return RefereeHudSpec::kWheelLegDistanceMaxRatio;
    }
}

void fillWheelLegPoseFromState(UiMakerInputSnapshot& input, uint8_t state) {
    const float distanceRatio = wheelLegDistanceRatioFromState(state);
    const float thighAngleDeg = RefereeHudSpec::wheelLegAngleForDistanceRatio(distanceRatio);
    input.leftLegHipWheelDistance = distanceRatio;
    input.rightLegHipWheelDistance = distanceRatio;
    input.leftLegThighAngleDeg = thighAngleDeg;
    input.rightLegThighAngleDeg = thighAngleDeg;
}

class BlackboardUiMakerInputSource final : public UiMakerInputSource {
  public:
    UiMakerInputSnapshot sample(float) override {
        GimbalToChassisComm comm{};
        SuperCapState capState{};
        Blackboard::instance().rComm.read(comm);
        Blackboard::instance().capState.read(capState);

        UiMakerInputSnapshot input{};
        input.capVoltage       = capState.voltage;
        input.capEnabled       = comm.msg.capSwitch != 0;
        input.capError         = capState.isError || capState.isCapLow;
        input.resetRequested   = comm.msg.resetUI != 0;
        input.stepClimbEnabled = comm.msg.stepClimb != 0;
        input.turboEnabled     = (comm.msg.turboMode != 0) && !input.stepClimbEnabled;
        input.feederEnabled    = (comm.msg.fireState > 0) || (comm.msg.shootEn != 0);
        input.spinEnabled      = (comm.msg.mode & 0x03U) == CHASSIS_SPIN;
        input.aimModeState     = RefereeHudSpec::normalizeAimModeState(static_cast<uint8_t>(comm.msg.aimMode));
        input.aimTargetState   = comm.msg.shootEn != 0 ? static_cast<uint8_t>(RefereeHudAimTarget::Fire)
                                                       : static_cast<uint8_t>(RefereeHudAimTarget::None);
        fillWheelLegPoseFromState(input, static_cast<uint8_t>(comm.msg.legLength));
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
            _switchIndex = static_cast<uint8_t>((_switchIndex + 1) % 5);
        }
        _aimTargetTimer += dt;
        while (_aimTargetTimer >= kAimTargetIntervalSeconds) {
            _aimTargetTimer -= kAimTargetIntervalSeconds;
            _aimTargetState = static_cast<uint8_t>((_aimTargetState + 1) % 3);
        }

        const float halfCycle = kCycleSeconds * 0.5f;
        const float ratio     = (_time < halfCycle) ? (_time / halfCycle) : ((kCycleSeconds - _time) / halfCycle);
        const float legPhase      = _time * 2.0f * RefereeHudSpec::kPi / kLegSignalCycleSeconds;
        const float leftLegWave    = std::sin(legPhase);
        const float rightLegWave   = std::sin(legPhase + RefereeHudSpec::kPi);
        const float leftAngleWave  = std::sin(legPhase + RefereeHudSpec::kPi / 3.0f);
        const float rightAngleWave = std::sin(legPhase + RefereeHudSpec::kPi + RefereeHudSpec::kPi / 3.0f);
        const float legDistanceRatioAmp =
            (RefereeHudSpec::kWheelLegDistanceMaxRatio - RefereeHudSpec::kWheelLegDistanceMinRatio) * 0.5f;

        UiMakerInputSnapshot input{};
        input.capVoltage     = 6.0f + 20.0f * ratio;
        input.capEnabled     = _time > 0.8f;
        input.capError       = input.capVoltage < RefereeHudSpec::kVoltageStage1;
        input.resetRequested = false;
        input.turboEnabled     = _switchIndex == 1;
        input.stepClimbEnabled = _switchIndex == 2;
        input.feederEnabled    = _switchIndex >= 3;
        input.spinEnabled      = _switchIndex >= 4;
        input.aimModeState   = _aimModeState;
        input.aimTargetState = _aimTargetState;
        input.leftLegHipWheelDistance =
            RefereeHudSpec::kWheelLegDistanceMidRatio + legDistanceRatioAmp * leftLegWave;
        input.leftLegThighAngleDeg = 30.0f + 14.0f * leftAngleWave;
        input.rightLegHipWheelDistance =
            RefereeHudSpec::kWheelLegDistanceMidRatio + legDistanceRatioAmp * rightLegWave;
        input.rightLegThighAngleDeg = 30.0f + 14.0f * rightAngleWave;
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
    syncRendererSenderId();
    resetGraphics();
    _input = _inputSource->sample(0.0f);
    _hudUi.draw(_uiRender, _input);
}

void UiMakerApp::run() {
    if (_inputSource == nullptr) {
        _inputSource = &defaultInputSource();
    }

    _input = _inputSource->sample(RefereeHudUi::kPeriodSeconds);

    const bool senderIdChanged = syncRendererSenderId();
    if (senderIdChanged || (_input.resetRequested && !_lastResetRequested)) {
        resetGraphics();
    }
    _lastResetRequested = _input.resetRequested;

    _hudUi.draw(_uiRender, _input);
}

void UiMakerApp::setInputSource(UiMakerInputSource& inputSource) {
    _inputSource = &inputSource;
    resetGraphics();
}

bool UiMakerApp::syncRendererSenderId() {
    RMRobotStatus robotStatus {};
    RefereeDataHub::instance().robotStatus.read(robotStatus);

    const uint16_t senderId = robotStatus.robotId;
    if (senderId == 0 || senderId == _lastRendererSenderId) {
        return false;
    }

    _uiRender.setSenderId(senderId);
    _lastRendererSenderId = senderId;
    return true;
}

void UiMakerApp::resetGraphics() { _hudUi.reset(_uiRender); }
