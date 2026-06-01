#include "ui/gui/GuiSynthPatchActionOps.h"

#include "ui/gui/GuiPatchApplyData.h"

namespace arachno {

bool runSetSynthParameter(
    const GuiSynthPatchActionContext& context,
    int instrument,
    const std::string& param,
    double value,
    bool refresh) {
    AppActionRequest request;
    request.actionId = "editor.instrument.parameter";
    request.parameters = {
        {"instrument", std::to_string(instrument)},
        {"name", param},
        {"value", std::to_string(value)}};
    bool ok = context.runAction(request, refresh).ok;
    if (!ok) {
        AppActionRequest fallback = request;
        fallback.commandText = "instrument-param "
            + std::to_string(instrument) + " "
            + param + " "
            + std::to_string(value);
        ok = context.runAction(fallback, refresh).ok;
    }
    if (ok) {
        context.session.playback().applyLiveInstrumentParameterChange(instrument, param, value);
        context.synthScopeRetriggerRequested = true;
    }
    return ok;
}

bool runSetSynthWaveform(
    const GuiSynthPatchActionContext& context,
    int instrument,
    const std::string& oscillator,
    const std::string& wave,
    bool refresh) {
    AppActionRequest request;
    request.actionId = "editor.instrument.wave";
    request.parameters = {
        {"instrument", std::to_string(instrument)},
        {"oscillator", oscillator},
        {"wave", wave}};
    bool ok = context.runAction(request, refresh).ok;
    if (!ok) {
        AppActionRequest fallback = request;
        fallback.commandText = "instrument-wave "
            + std::to_string(instrument) + " "
            + oscillator + " "
            + wave;
        ok = context.runAction(fallback, refresh).ok;
    }
    if (ok) {
        context.session.playback().applyLiveInstrumentWaveformChange(instrument, oscillator, waveformFromName(wave));
        context.synthScopeRetriggerRequested = true;
    }
    return ok;
}

bool applyPatchToInstrumentAction(
    const GuiApplyPatchToInstrumentContext& context,
    int instrument,
    const SynthPatch& patch,
    bool preserveName) {
    const int count = static_cast<int>(context.session.song().instruments.size());
    if (instrument < 0 || instrument >= count) {
        context.lastAction.ok = false;
        context.lastAction.actionId = "editor.instrument.patch_apply";
        context.lastAction.error = "instrument index out of range";
        return false;
    }

    auto patchAction = [&](const AppActionRequest& request) {
        const AppActionResult result = context.runAction(request, false);
        return result.ok;
    };
    if (!preserveName) {
        AppActionRequest rename;
        rename.actionId = "editor.instrument.rename";
        rename.parameters = {
            {"instrument", std::to_string(instrument)},
            {"name", patch.name.empty() ? "ImportedPatch" : patch.name}};
        if (!patchAction(rename)) {
            context.refreshSnapshot();
            return false;
        }
    }

    for (const SynthWaveAssignment& assignment : synthPatchWaveAssignments(patch)) {
        if (!context.setSynthWaveform(instrument, assignment.oscillator, assignment.wave, false)) {
            context.refreshSnapshot();
            return false;
        }
    }

    const std::vector<std::pair<std::string, double>> params = synthPatchParameterAssignments(patch);
    for (const auto& entry : params) {
        if (!context.setSynthParameter(instrument, entry.first, entry.second, false)) {
            context.refreshSnapshot();
            return false;
        }
    }

    context.refreshSnapshot();
    return true;
}

} // namespace arachno
