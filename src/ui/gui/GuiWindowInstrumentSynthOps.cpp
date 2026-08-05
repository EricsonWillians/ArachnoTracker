#include "ui/gui/GuiWindowInstrumentSynthOps.h"

#include <algorithm>
#include <cctype>

#include "GuiInput.h"

namespace arachno {

namespace {

std::string canonicalizeInstrumentName(const std::string& name) {
    const std::string lowered = lowerCopy(trimCopy(name));
    std::string canonical;
    canonical.reserve(lowered.size());
    bool previousWasSpace = false;
    for (const char ch : lowered) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || std::isspace(static_cast<unsigned char>(ch))) {
            const bool isSpace = std::isspace(static_cast<unsigned char>(ch));
            if (isSpace) {
                if (!previousWasSpace && !canonical.empty()) {
                    canonical.push_back(' ');
                }
                previousWasSpace = true;
            } else {
                canonical.push_back(ch);
                previousWasSpace = false;
            }
        }
    }
    while (!canonical.empty() && std::isspace(static_cast<unsigned char>(canonical.back()))) {
        canonical.pop_back();
    }
    return canonical;
}

std::string canonicalizeInstrumentNameCompact(const std::string& name) {
    std::string canonical = canonicalizeInstrumentName(name);
    canonical.erase(std::remove_if(canonical.begin(), canonical.end(), [](unsigned char ch) {
        return std::isspace(ch);
    }),
    canonical.end());
    return canonical;
}

int resolveProjectInstrumentByCanonicalName(const std::vector<InstrumentSummary>& instruments, const std::string& targetName) {
    const std::string target = canonicalizeInstrumentName(targetName);
    const std::string targetCompact = canonicalizeInstrumentNameCompact(targetName);
    if (target.empty()) {
        return -1;
    }
    for (std::size_t index = 0; index < instruments.size(); ++index) {
        if (canonicalizeInstrumentName(instruments[index].name) == target) {
            return static_cast<int>(index);
        }
        if (canonicalizeInstrumentNameCompact(instruments[index].name) == targetCompact) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

} // namespace

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
    auto ensureInstrument = [&](const std::string& fallbackName) -> bool {
        const AppSessionSnapshot snapshot = context.activeSnapshot();
        if (!snapshot.editor.instruments.empty()) {
            return true;
        }
        AppActionRequest request;
        request.actionId = "editor.instrument.new";
        request.parameters = {{"name", fallbackName}};
        const AppActionResult created = context.runActionWithRefresh(request, true);
        context.applyActionResultStatus(created);
        const AppSessionSnapshot updated = context.activeSnapshot();
        return created.ok && !updated.editor.instruments.empty();
    };

    const auto createStandardInstrument = [&](const std::string& instrumentName) -> bool {
        AppActionRequest request;
        request.actionId = "editor.instrument.new";
        request.parameters = {{"name", instrumentName}};
        const AppActionResult createResult = context.runActionWithRefresh(request, true);
        context.applyActionResultStatus(createResult);
        return createResult.ok;
    };

    const auto resolveProjectInstrument = [&](const std::string& instrumentName) -> int {
        const AppSessionSnapshot snapshot = context.activeSnapshot();
        return resolveProjectInstrumentByCanonicalName(snapshot.editor.instruments, instrumentName);
    };

    const auto resolveOrCreateStandardMidi = [&](const std::string& instrumentName) -> int {
        const int resolved = resolveProjectInstrument(instrumentName);
        if (resolved >= 0) {
            return resolved;
        }
        if (!createStandardInstrument(instrumentName)) {
            return -1;
        }
        const int created = resolveProjectInstrument(instrumentName);
        if (created >= 0) {
            return created;
        }
        const AppSessionSnapshot snapshot = context.activeSnapshot();
        const int latestCount = static_cast<int>(snapshot.editor.instruments.size());
        return latestCount > 0 ? std::clamp(latestCount - 1, 0, latestCount - 1) : -1;
    };

    if (isStandardMidiBrowserIndex(index)) {
        const std::string instrumentName = standardMidiBrowserName(index);
        if (!instrumentName.empty()) {
            const int resolvedIndex = resolveOrCreateStandardMidi(instrumentName);
            if (resolvedIndex < 0) {
                return;
            }
            index = resolvedIndex;
        } else {
            return;
        }
    }

    const AppSessionSnapshot snapshot = context.activeSnapshot();
    int count = static_cast<int>(snapshot.editor.instruments.size());
    if (count <= 0) {
        if (!ensureInstrument("Init")) {
            return;
        }
        const AppSessionSnapshot updated = context.activeSnapshot();
        count = static_cast<int>(updated.editor.instruments.size());
    }
    if (count <= 0) {
        return;
    }

    const int clampedIndex = std::clamp(index, 0, count - 1);
    context.armedInstrument = clampedIndex;
    AppActionRequest stepInstrument;
    stepInstrument.actionId = "editor.step.instrument";
    stepInstrument.parameters = {{"index", std::to_string(context.armedInstrument)}};
    (void)context.runActionWithRefresh(stepInstrument, false);
    context.synthScopeRetriggerRequested = true;

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
    const int count = static_cast<int>(snap.editor.instruments.size());
    if (count > 0) {
        context.armedInstrument = std::clamp(context.armedInstrument, 0, count - 1);
    } else {
        context.armedInstrument = 0;
    }
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
    const AppSessionSnapshot snap = context.activeSnapshot();
    if (applySelection) {
        const std::vector<int> indices = filteredInstrumentIndicesFromWindowState(context, snap);
        if (!indices.empty()) {
            const int row = std::clamp(
                context.instrumentBrowserSelected,
                0,
                static_cast<int>(indices.size()) - 1);
            const int instrumentIndex = indices[static_cast<std::size_t>(row)];
            selectInstrument(instrumentIndex);
        }
    }

    context.instrumentBrowserActive = false;
    context.instrumentBrowserQuery.clear();
    context.instrumentBrowserScroll = 0;
    context.instrumentBrowserSelected = 0;
    context.instrumentBrowserHitTargets.clear();
    context.instrumentBrowserListRect = UiRect {};
    context.instrumentBrowserAcceptButton = UiRect {};
    context.instrumentBrowserCancelButton = UiRect {};
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
    const int count = static_cast<int>(snap.editor.instruments.size());
    context.instrumentListStart = std::clamp(context.instrumentListStart, 0, std::max(0, count - 1));
    if (count <= 0) {
        context.instrumentListStart = 0;
        return;
    }
    const int maxStart = std::max(0, count - std::max(1, context.instrumentListVisibleRows));
    context.instrumentListStart = std::clamp(context.instrumentListStart, 0, maxStart);
}

void scrollInstrumentListFromWindowState(const GuiWindowInstrumentSynthContext& context, int delta) {
    const AppSessionSnapshot snap = context.activeSnapshot();
    const int count = static_cast<int>(snap.editor.instruments.size());
    if (count <= 0) {
        context.instrumentListStart = 0;
        return;
    }
    const int maxStart = std::max(0, count - std::max(1, context.instrumentListVisibleRows));
    context.instrumentListStart = std::clamp(context.instrumentListStart + delta, 0, maxStart);
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
