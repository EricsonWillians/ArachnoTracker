#pragma once

#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <X11/Xlib.h>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "ui/gui/GuiMidiInput.h"
#include "ui/gui/GuiSynthEventContextFactoryOps.h"
#include "ui/gui/GuiSynthInputOps.h"
#include "ui/gui/GuiSynthInteractionOps.h"

namespace arachno {

struct GuiSynthWindowInteractionAdapterContext {
    ApplicationSession& session;
    Display* display = nullptr;
    Window& synthWindow;
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
    int synthParamContentHeight = 0;
    int& synthParamScroll;

    int& synthKeyboardBaseOctave;
    int synthKeyboardVisibleOctaves = 4;
    int& synthParamPage;
    int& synthParamOscTarget;

    int& armedOctave;
    int& armedInstrument;
    int& synthPreviewMidi;
    int& paintNoteMidi;
    bool synthStepAdvance = true;
    float synthDefaultVelocity = 0.8f;

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

bool triggerSynthKeyboardPointerFromWindowState(
    const GuiSynthWindowInteractionAdapterContext& context,
    int mx,
    int my,
    bool allowRetrigger);

bool handleSynthWindowClickFromWindowState(
    const GuiSynthWindowInteractionAdapterContext& context,
    int mx,
    int my);

bool pollSynthMidiInputFromWindowState(const GuiSynthWindowInteractionAdapterContext& context);

GuiSynthKeyContext makeSynthKeyContextFromWindowState(const GuiSynthWindowInteractionAdapterContext& context);

GuiSynthWindowEventContext makeSynthWindowEventContextFromWindowState(
    const GuiSynthWindowInteractionAdapterContext& context,
    const std::function<bool(const XEvent&)>& isAutoRepeatRelease);

} // namespace arachno
