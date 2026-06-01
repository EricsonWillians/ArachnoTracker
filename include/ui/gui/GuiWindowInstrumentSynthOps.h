#pragma once

#include <functional>
#include <string>
#include <vector>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "Synthesizer.h"
#include "ui/gui/GuiInstrumentBrowserOps.h"
#include "ui/gui/GuiSynthAuditionWindowOps.h"
#include "ui/gui/GuiSynthPatchActionOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiWindowInstrumentSynthContext {
    ApplicationSession& session;
    int& armedInstrument;
    int& instrumentListStart;
    int& instrumentListVisibleRows;
    bool& synthScopeRetriggerRequested;
    int& synthPreviewMidi;
    int& paintNoteMidi;
    float& defaultVelocity;
    bool& synthWindowNeedsRedraw;
    AppActionResult& lastAction;
    std::string& instrumentBrowserQuery;
    bool& audioTuningDialogActive;
    bool& instrumentBrowserActive;
    int& instrumentBrowserScroll;
    int& instrumentBrowserSelected;
    std::vector<std::pair<UiRect, int>>& instrumentBrowserHitTargets;
    UiRect& instrumentBrowserListRect;
    UiRect& instrumentBrowserAcceptButton;
    UiRect& instrumentBrowserCancelButton;

    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
    std::function<AppActionResult(const AppActionRequest&, bool)> runActionWithRefresh;
    std::function<void()> refreshSnapshot;
    std::function<void(int)> ensureSynthKeyboardShowsMidi;
    std::function<void(const AppActionResult&)> applyActionResultStatus;
};

struct GuiWindowInstrumentSynthBindings {
    std::function<void(int)> selectInstrument;
    std::function<std::vector<int>(const AppSessionSnapshot&)> filteredInstrumentIndices;
    std::function<void()> openInstrumentBrowser;
    std::function<void(bool)> closeInstrumentBrowser;
    std::function<void(int)> cycleInstrumentBy;
    std::function<void()> clampInstrumentListWindow;
    std::function<void(int)> scrollInstrumentList;
    std::function<void()> auditionArmedInstrument;
    std::function<void(int)> auditionSynthPreviewMidi;
    std::function<void(int, float)> auditionSynthPreviewMidiVelocity;
    std::function<void(const SynthPatch&, int, int)> auditionSynthOscillatorPreview;
    std::function<int()> clampInstrumentIndex;
    std::function<bool(int, const std::string&, double, bool)> setSynthParameter;
    std::function<bool(int, const std::string&, const std::string&, bool)> setSynthWaveform;
    std::function<bool(int, const SynthPatch&, bool)> applyPatchToInstrument;
};

GuiWindowInstrumentSynthBindings makeInstrumentSynthBindingsFromWindowState(
    const GuiWindowInstrumentSynthContext& context);

std::vector<int> filteredInstrumentIndicesFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    const AppSessionSnapshot& snapshot);

void selectInstrumentFromWindowState(const GuiWindowInstrumentSynthContext& context, int index);
void openInstrumentBrowserFromWindowState(const GuiWindowInstrumentSynthContext& context);
void closeInstrumentBrowserFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    bool applySelection,
    const std::function<void(int)>& selectInstrument);
void cycleInstrumentByFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    int delta,
    const std::function<void(int)>& selectInstrument);
void clampInstrumentListWindowFromWindowState(const GuiWindowInstrumentSynthContext& context);
void scrollInstrumentListFromWindowState(const GuiWindowInstrumentSynthContext& context, int delta);

GuiSynthAuditionWindowContext makeSynthAuditionWindowContextFromWindowState(
    const GuiWindowInstrumentSynthContext& context);
void auditionArmedInstrumentFromWindowState(const GuiWindowInstrumentSynthContext& context);
void auditionSynthPreviewMidiFromWindowState(const GuiWindowInstrumentSynthContext& context, int midiNote);
void auditionSynthPreviewMidiVelocityFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    int midiNote,
    float velocity);
void auditionSynthOscillatorPreviewFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    const SynthPatch& sourcePatch,
    int oscillatorIndex,
    int midiNote);

int clampInstrumentIndexFromWindowState(const GuiWindowInstrumentSynthContext& context);
bool setSynthParameterFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    int instrument,
    const std::string& param,
    double value,
    bool refresh = true);
bool setSynthWaveformFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    int instrument,
    const std::string& oscillator,
    const std::string& wave,
    bool refresh = true);
bool applyPatchToInstrumentFromWindowState(
    const GuiWindowInstrumentSynthContext& context,
    int instrument,
    const SynthPatch& patch,
    bool preserveName);

} // namespace arachno
