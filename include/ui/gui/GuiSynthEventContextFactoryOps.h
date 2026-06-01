#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <X11/Xlib.h>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "Synthesizer.h"
#include "ui/gui/GuiSynthInteractionOps.h"
#include "ui/gui/GuiSynthWindowEventOps.h"

namespace arachno {

struct GuiSynthKeyContextFactoryInput {
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

    int& armedOctave;
    int& synthPreviewMidi;
    int& paintNoteMidi;
    std::function<void(int)> setArmedOctave;
    std::function<void(int)> ensureSynthKeyboardShowsMidi;
    std::function<void(bool)> setSynthWindowVisible;
    std::function<void(int)> cycleInstrument;
    std::function<void(int)> auditionSynthPreviewMidi;
    std::function<bool(unsigned int, int)> claimSynthPreviewKey;
    std::function<void(unsigned int)> releaseSynthPreviewKey;
};

GuiSynthKeyContext makeSynthKeyContextFromState(const GuiSynthKeyContextFactoryInput& input);

struct GuiSynthClickContextFactoryInput {
    ApplicationSession& session;
    int mx = 0;
    int my = 0;
    int& synthLastPointerMidi;
    int& synthKeyboardBaseOctave;
    int synthKeyboardVisibleOctaves = 4;
    int armedOctave = 4;
    int& synthParamPage;
    int& synthParamScroll;
    int& synthParamOscTarget;
    int& synthParamDragStartX;
    int& synthParamDragStartY;
    double& synthParamDragStartValue;
    double& synthParamDragLastValue;
    bool& synthParamDragActive;
    bool& synthParamDragKnob;
    bool& synthParamDragDirty;
    std::string& synthParamDragName;
    UiRect& synthParamDragRect;
    int synthPreviewMidi = 60;
    std::vector<SynthWindowHit> synthWindowHits;

    std::function<int()> clampInstrumentIndex;
    std::function<void(bool)> setSynthWindowVisible;
    std::function<void(int)> selectInstrument;
    std::function<void()> auditionArmedInstrument;
    std::function<void(int)> auditionSynthPreviewMidi;
    std::function<void(const SynthPatch&, int, int)> auditionSynthOscillatorPreview;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<bool(int, const std::string&, const std::string&)> setSynthWaveform;
    std::function<bool(int, const std::string&, double)> setSynthParameter;
    std::function<const SynthParamDef*(const std::string&)> findSynthParamDef;
    std::function<double(const SynthPatch&, const std::string&)> getSynthParameterValue;
};

GuiSynthClickContext makeSynthClickContextFromState(const GuiSynthClickContextFactoryInput& input);

struct GuiSynthWindowEventContextFactoryInput {
    Display* display = nullptr;
    Window synthWindow = 0;
    Atom wmDelete = 0;

    int& synthWindowWidth;
    int& synthWindowHeight;
    int synthWindowMinWidth = 0;
    int synthWindowMinHeight = 0;
    std::function<void()> ensureSynthBackbuffer;
    std::function<void(bool)> setSynthWindowVisible;

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

    const std::vector<SynthWindowHit>& synthWindowHits;

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

    std::function<bool(int, int, bool)> triggerSynthKeyboardPointer;
    std::function<bool(int, int)> handleSynthWindowClick;
    std::function<void()> refreshSnapshot;

    std::function<int()> clampInstrumentIndex;
    std::function<const SynthParamDef*(const std::string&)> findSynthParamDef;
    std::function<bool(int, const std::string&, double, bool)> setSynthParameter;

    std::function<GuiSynthKeyContext()> makeSynthKeyContext;
    std::function<bool(const XEvent&)> isAutoRepeatRelease;
};

GuiSynthWindowEventContext makeSynthWindowEventContextFromState(
    const GuiSynthWindowEventContextFactoryInput& input);

} // namespace arachno
