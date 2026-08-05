#pragma once

#include <array>
#include <chrono>
#include <functional>
#include <string>

#include <X11/Xlib.h>

#include "AppActions.h"
#include "Synthesizer.h"
#include "ui/gui/GuiSynthWindowLifecycleAdapterOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiWindowSynthLifecycleBindingsInput {
    Display* display = nullptr;
    int screen = 0;
    Atom wmDelete = 0;
    XFontStruct* uiFont = nullptr;
    Window& synthWindow;
    GC& synthGc;
    Pixmap& synthBackbuffer;
    int& synthBackbufferWidth;
    int& synthBackbufferHeight;
    int& synthWindowWidth;
    int& synthWindowHeight;
    bool& synthWindowVisible;
    std::array<bool, 256>& synthPreviewKeyHeld;
    std::array<int, 256>& synthPreviewKeyMidi;
    std::array<bool, 128>& synthMidiPreviewHeld;
    bool& synthPointerDown;
    int& synthLastPointerMidi;
    std::string& synthTooltipParam;
    std::chrono::steady_clock::time_point& synthTooltipHoverSince;
    bool& synthScopeRetriggerRequested;
    int& synthKeyboardBaseOctave;
    int& synthKeyboardVisibleOctaves;
    int& synthPreviewMidi;
    int& paintNoteMidi;
    int& armedOctave;
    bool& synthWindowNeedsRedraw;
    bool& synthScopeLaneActive;
    std::array<Synthesizer, 4>& synthScopeLaneSynth;
    AppActionResult& lastAction;

    bool& audioTuningDialogActive;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(const AppActionRequest&)> runLifecycleAction;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
    // Sends a note-off for a released sustained preview key (midi note argument).
    std::function<void(int)> auditionSynthPreviewNoteOff;
    // Releases every sustained live-audition voice (used when the preview context closes).
    std::function<void()> releaseAllSynthPreviewNotes;
};

struct GuiWindowSynthLifecycleBindings {
    GuiSynthWindowLifecycleAdapterInput lifecycleInput;
    std::function<void()> releaseSynthBackbuffer;
    std::function<void()> ensureSynthBackbuffer;
    std::function<bool(unsigned int, int)> claimSynthPreviewKey;
    std::function<void(unsigned int)> releaseSynthPreviewKey;
    std::function<void(bool)> setSynthWindowVisible;
    std::function<void(const std::string&)> runFileButtonAction;
};

GuiWindowSynthLifecycleBindings makeSynthLifecycleBindingsFromWindowState(
    const GuiWindowSynthLifecycleBindingsInput& input);

} // namespace arachno
