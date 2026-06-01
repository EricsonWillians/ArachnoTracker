#pragma once

#include <array>
#include <chrono>
#include <string>

#include <X11/Xlib.h>

#include "AppActions.h"
#include "Synthesizer.h"
#include "ui/gui/GuiSynthWindowLifecycleOps.h"

namespace arachno {

struct GuiSynthWindowLifecycleAdapterInput {
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
};

GuiSynthWindowLifecycleContext makeSynthWindowLifecycleContextFromWindowState(
    const GuiSynthWindowLifecycleAdapterInput& input);

void releaseSynthBackbufferFromWindowState(const GuiSynthWindowLifecycleAdapterInput& input);
void ensureSynthBackbufferFromWindowState(const GuiSynthWindowLifecycleAdapterInput& input);
bool claimSynthPreviewKeyFromWindowState(
    const GuiSynthWindowLifecycleAdapterInput& input,
    unsigned int keycode,
    int midiNote);
void releaseSynthPreviewKeyFromWindowState(const GuiSynthWindowLifecycleAdapterInput& input, unsigned int keycode);
void setSynthWindowVisibleFromWindowState(const GuiSynthWindowLifecycleAdapterInput& input, bool visible);

} // namespace arachno
