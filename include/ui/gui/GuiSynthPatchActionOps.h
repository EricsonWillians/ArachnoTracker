#pragma once

#include <functional>
#include <string>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "Synthesizer.h"

namespace arachno {

struct GuiSynthPatchActionContext {
    ApplicationSession& session;
    std::function<AppActionResult(const AppActionRequest&, bool)> runAction;
    bool& synthScopeRetriggerRequested;
};

bool runSetSynthParameter(
    const GuiSynthPatchActionContext& context,
    int instrument,
    const std::string& param,
    double value,
    bool refresh = true);

bool runSetSynthWaveform(
    const GuiSynthPatchActionContext& context,
    int instrument,
    const std::string& oscillator,
    const std::string& wave,
    bool refresh = true);

struct GuiApplyPatchToInstrumentContext {
    ApplicationSession& session;
    std::function<AppActionResult(const AppActionRequest&, bool)> runAction;
    std::function<void()> refreshSnapshot;
    std::function<bool(int, const std::string&, const std::string&, bool)> setSynthWaveform;
    std::function<bool(int, const std::string&, double, bool)> setSynthParameter;
    AppActionResult& lastAction;
};

bool applyPatchToInstrumentAction(
    const GuiApplyPatchToInstrumentContext& context,
    int instrument,
    const SynthPatch& patch,
    bool preserveName);

} // namespace arachno
