#include "ui/gui/GuiWindowInstrumentSynthOps.h"

#include <algorithm>

namespace arachno {

GuiWindowInstrumentSynthBindings makeInstrumentSynthBindingsFromWindowState(
    const GuiWindowInstrumentSynthContext& context) {
    const auto state = context;
    GuiWindowInstrumentSynthBindings bindings;

    bindings.selectInstrument = [state](int index) {
        selectInstrumentFromWindowState(state, index);
    };
    bindings.filteredInstrumentIndices = [state](const AppSessionSnapshot& snapshot) {
        return filteredInstrumentIndicesFromWindowState(state, snapshot);
    };
    bindings.openInstrumentBrowser = [state]() {
        openInstrumentBrowserFromWindowState(state);
    };
    bindings.closeInstrumentBrowser = [state, selectInstrument = bindings.selectInstrument](bool applySelection) {
        closeInstrumentBrowserFromWindowState(state, applySelection, selectInstrument);
    };
    bindings.cycleInstrumentBy = [state, selectInstrument = bindings.selectInstrument](int delta) {
        cycleInstrumentByFromWindowState(state, delta, selectInstrument);
    };
    bindings.clampInstrumentListWindow = [state]() {
        clampInstrumentListWindowFromWindowState(state);
    };
    bindings.scrollInstrumentList = [state](int delta) {
        scrollInstrumentListFromWindowState(state, delta);
    };
    bindings.auditionArmedInstrument = [state]() {
        auditionArmedInstrumentFromWindowState(state);
    };
    bindings.auditionSynthPreviewMidi = [state](int midiNote) {
        auditionSynthPreviewMidiFromWindowState(state, midiNote);
    };
    bindings.auditionSynthPreviewMidiVelocity = [state](int midiNote, float velocity) {
        auditionSynthPreviewMidiVelocityFromWindowState(state, midiNote, velocity);
    };
    bindings.auditionSynthOscillatorPreview = [state](const SynthPatch& sourcePatch, int oscillatorIndex, int midiNote) {
        auditionSynthOscillatorPreviewFromWindowState(state, sourcePatch, oscillatorIndex, midiNote);
    };
    bindings.clampInstrumentIndex = [state]() {
        return clampInstrumentIndexFromWindowState(state);
    };
    bindings.setSynthParameter = [state](int instrument, const std::string& param, double value, bool refresh) {
        return setSynthParameterFromWindowState(state, instrument, param, value, refresh);
    };
    bindings.setSynthWaveform = [state](int instrument, const std::string& oscillator, const std::string& wave, bool refresh) {
        return setSynthWaveformFromWindowState(state, instrument, oscillator, wave, refresh);
    };
    bindings.applyPatchToInstrument = [state](int instrument, const SynthPatch& patch, bool preserveName) {
        return applyPatchToInstrumentFromWindowState(state, instrument, patch, preserveName);
    };

    return bindings;
}

std::vector<int> filteredInstrumentIndicesFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    const AppSessionSnapshot& snapshot) {
    return filteredInstrumentIndicesForQuery(snapshot, context.instrumentBrowserQuery);
}

void selectInstrumentFromWindowState(const GuiWindowInstrumentSynthContext& context, int index) {
    const AppSessionSnapshot snap = context.activeSnapshot();
    const int count = static_cast<int>(snap.editor.instruments.size());
    if (count <= 0) {
        return;
    }
    context.armedInstrument = std::clamp(index, 0, count - 1);
    AppActionRequest inst;
    inst.actionId = "editor.step.instrument";
    inst.parameters = {{"index", std::to_string(context.armedInstrument)}};
    (void)context.runAction(inst);
    if (context.armedInstrument < context.instrumentListStart) {
        context.instrumentListStart = context.armedInstrument;
    } else if (context.armedInstrument >= context.instrumentListStart + std::max(1, context.instrumentListVisibleRows)) {
        context.instrumentListStart = context.armedInstrument - std::max(1, context.instrumentListVisibleRows) + 1;
    }
    const int maxStart = std::max(0, count - std::max(1, context.instrumentListVisibleRows));
    context.instrumentListStart = std::clamp(context.instrumentListStart, 0, maxStart);
    context.synthScopeRetriggerRequested = true;
}

void openInstrumentBrowserFromWindowState(const GuiWindowInstrumentSynthContext& context) {
    const AppSessionSnapshot snap = context.activeSnapshot();
    arachno::openInstrumentBrowser(
        GuiInstrumentBrowserOpenContext {
            context.audioTuningDialogActive,
            context.instrumentBrowserActive,
            context.instrumentBrowserQuery,
            context.instrumentBrowserScroll,
            context.instrumentBrowserSelected,
            context.armedInstrument,
            [&](const AppSessionSnapshot& inputSnapshot) {
                return filteredInstrumentIndicesFromWindowState(context, inputSnapshot);
            }},
        snap);
}

void closeInstrumentBrowserFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    bool applySelection,
    const std::function<void(int)>& selectInstrument) {
    arachno::closeInstrumentBrowser(
        GuiInstrumentBrowserCloseContext {
            context.activeSnapshot,
            [&](const AppSessionSnapshot& inputSnapshot) {
                return filteredInstrumentIndicesFromWindowState(context, inputSnapshot);
            },
            selectInstrument,
            context.instrumentBrowserActive,
            context.instrumentBrowserQuery,
            context.instrumentBrowserScroll,
            context.instrumentBrowserSelected,
            context.instrumentBrowserHitTargets,
            context.instrumentBrowserListRect,
            context.instrumentBrowserAcceptButton,
            context.instrumentBrowserCancelButton},
        applySelection);
}

void cycleInstrumentByFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    int delta,
    const std::function<void(int)>& selectInstrument) {
    const AppSessionSnapshot snap = context.activeSnapshot();
    arachno::cycleInstrumentBy(delta, context.armedInstrument, snap, selectInstrument);
}

void clampInstrumentListWindowFromWindowState(const GuiWindowInstrumentSynthContext& context) {
    const AppSessionSnapshot snap = context.activeSnapshot();
    arachno::clampInstrumentListWindow(snap, context.instrumentListVisibleRows, context.instrumentListStart);
}

void scrollInstrumentListFromWindowState(const GuiWindowInstrumentSynthContext& context, int delta) {
    const AppSessionSnapshot snap = context.activeSnapshot();
    arachno::scrollInstrumentList(delta, snap, context.instrumentListVisibleRows, context.instrumentListStart);
}

GuiSynthAuditionWindowContext makeSynthAuditionWindowContextFromWindowState(
    const GuiWindowInstrumentSynthContext& context) {
    return GuiSynthAuditionWindowContext {
        context.session,
        context.armedInstrument,
        context.synthPreviewMidi,
        context.paintNoteMidi,
        context.defaultVelocity,
        context.synthWindowNeedsRedraw,
        context.lastAction,
        context.ensureSynthKeyboardShowsMidi,
        context.applyActionResultStatus};
}

void auditionArmedInstrumentFromWindowState(const GuiWindowInstrumentSynthContext& context) {
    ::arachno::auditionArmedInstrumentFromWindowState(makeSynthAuditionWindowContextFromWindowState(context));
}

void auditionSynthPreviewMidiFromWindowState(const GuiWindowInstrumentSynthContext& context, int midiNote) {
    ::arachno::auditionSynthPreviewMidiFromWindowState(makeSynthAuditionWindowContextFromWindowState(context), midiNote);
}

void auditionSynthPreviewMidiVelocityFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    int midiNote,
    float velocity) {
    ::arachno::auditionSynthPreviewMidiVelocityFromWindowState(
        makeSynthAuditionWindowContextFromWindowState(context),
        midiNote,
        velocity);
}

void auditionSynthOscillatorPreviewFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    const SynthPatch& sourcePatch,
    int oscillatorIndex,
    int midiNote) {
    ::arachno::auditionSynthOscillatorPreviewFromWindowState(
        makeSynthAuditionWindowContextFromWindowState(context),
        sourcePatch,
        oscillatorIndex,
        midiNote);
}

int clampInstrumentIndexFromWindowState(const GuiWindowInstrumentSynthContext& context) {
    const int count = static_cast<int>(context.session.song().instruments.size());
    if (count <= 0) {
        return -1;
    }
    context.armedInstrument = std::clamp(context.armedInstrument, 0, count - 1);
    return context.armedInstrument;
}

bool setSynthParameterFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    int instrument,
    const std::string& param,
    double value,
    bool refresh) {
    return runSetSynthParameter(
        GuiSynthPatchActionContext {
            context.session,
            context.runActionWithRefresh,
            context.synthScopeRetriggerRequested},
        instrument,
        param,
        value,
        refresh);
}

bool setSynthWaveformFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    int instrument,
    const std::string& oscillator,
    const std::string& wave,
    bool refresh) {
    return runSetSynthWaveform(
        GuiSynthPatchActionContext {
            context.session,
            context.runActionWithRefresh,
            context.synthScopeRetriggerRequested},
        instrument,
        oscillator,
        wave,
        refresh);
}

bool applyPatchToInstrumentFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    int instrument,
    const SynthPatch& patch,
    bool preserveName) {
    return applyPatchToInstrumentAction(
        GuiApplyPatchToInstrumentContext {
            context.session,
            context.runActionWithRefresh,
            context.refreshSnapshot,
            [&](int inst, const std::string& oscillator, const std::string& wave, bool refreshRequested) {
                return setSynthWaveformFromWindowState(context, inst, oscillator, wave, refreshRequested);
            },
            [&](int inst, const std::string& param, double value, bool refreshRequested) {
                return setSynthParameterFromWindowState(context, inst, param, value, refreshRequested);
            },
            context.lastAction},
        instrument,
        patch,
        preserveName);
}

} // namespace arachno
