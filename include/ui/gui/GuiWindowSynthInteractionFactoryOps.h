#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "Synthesizer.h"
#include "ui/gui/GuiMidiInput.h"
#include "ui/gui/GuiSynthWindowInteractionAdapterOps.h"

namespace arachno {

struct GuiWindowSynthInteractionFactoryInput {
    ApplicationSession& session;
    Display* display = nullptr;
    Window synthWindow = 0;
    Atom wmDelete = 0;

    int& synthWindowWidth;
    int& synthWindowHeight;
    int synthWindowMinWidth = 0;
    int synthWindowMinHeight = 0;
    std::function<void()> ensureSynthBackbuffer;
    std::function<void(bool)> setSynthWindowVisible;

    bool& synthWindowVisible;
    bool& synthWindowNeedsRedraw;
    bool& needsRedraw;

    int& synthPointerX;
    int& synthPointerY;
    bool& synthPointerDown;
    int& synthLastPointerMidi;

    bool& synthParamDragActive;
    bool& synthParamDragKnob;
    bool& synthParamDragDirty;
    std::string& synthParamDragName;
    UiRect& synthParamDragRect;
    int& synthParamDragStartX;
    int& synthParamDragStartY;
    double& synthParamDragStartValue;
    double& synthParamDragLastValue;

    std::string& synthTooltipParam;
    std::chrono::steady_clock::time_point& synthTooltipHoverSince;

    UiRect& synthParamViewport;
    int& synthParamContentHeight;
    int& synthParamScroll;

    int& synthKeyboardBaseOctave;
    int& synthKeyboardVisibleOctaves;
    int& synthParamPage;
    int& synthParamOscTarget;

    int& armedOctave;
    int& armedInstrument;
    int& synthPreviewMidi;
    int& paintNoteMidi;
    bool& stepAdvance;
    float& defaultVelocity;

    std::vector<SynthWindowHit>& synthWindowHits;
    std::vector<PianoKeyHit>& synthKeyboardHits;

    InlinePromptState& inlinePrompt;
    std::function<bool(InlinePromptKind)> isSynthInlinePromptKind;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    bool& synthInlinePromptButtonsVisible;
    UiRect& synthInlinePromptAcceptButton;
    UiRect& synthInlinePromptCancelButton;
    std::vector<FileBrowserHit>& fileBrowserHits;
    UiRect& fileBrowserListRect;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::filesystem::path& fileBrowserDirectory;
    std::function<void()> refreshFileBrowserEntries;
    std::function<void()> executeInlinePrompt;
    std::function<void()> cancelInlinePrompt;

    std::function<void(int)> ensureSynthKeyboardShowsMidi;
    std::function<void(int)> cycleInstrumentBy;
    std::function<void(int)> setArmedOctave;
    std::function<bool(unsigned int, int)> claimSynthPreviewKey;
    std::function<void(unsigned int)> releaseSynthPreviewKey;
    std::function<void(int)> auditionSynthPreviewMidi;
    std::function<void(int, float)> auditionSynthPreviewMidiVelocity;
    std::function<void(const SynthPatch&, int, int)> auditionSynthOscillatorPreview;
    std::function<void()> auditionArmedInstrument;
    std::function<void(int)> selectInstrument;
    std::function<int()> clampInstrumentIndex;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
    std::function<bool(int, const std::string&, const std::string&)> setSynthWaveform;
    std::function<bool(int, const std::string&, double)> setSynthParameter;
    std::function<bool(int, const std::string&, double, bool)> setSynthParameterWithRefresh;
    std::function<const SynthParamDef*(const std::string&)> findSynthParamDef;
    std::function<double(const SynthPatch&, const std::string&)> getSynthParameterValue;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
    std::function<void()> refreshSnapshot;

    GuiMidiInput& midiInput;
    std::array<bool, 128>& synthMidiPreviewHeld;
};

GuiSynthWindowInteractionAdapterContext makeSynthInteractionContextFromWindowState(
    const GuiWindowSynthInteractionFactoryInput& input);

} // namespace arachno
