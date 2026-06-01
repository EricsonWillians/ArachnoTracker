#pragma once

#include <array>
#include <chrono>
#include <string>

#include <X11/Xlib.h>

#include "AppActions.h"
#include "Synthesizer.h"

namespace arachno {

struct GuiSynthWindowLifecycleContext {
    Display* display = nullptr;
    int screen = 0;
    Atom wmDelete = 0;
    XFontStruct* uiFont = nullptr;
    Window& synthWindow;
    GC& synthGc;
    Pixmap& synthBackbuffer;
    int& synthBackbufferWidth;
    int& synthBackbufferHeight;
    int synthWindowWidth = 0;
    int synthWindowHeight = 0;
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
    int synthKeyboardVisibleOctaves = 0;
    int& synthPreviewMidi;
    int& paintNoteMidi;
    int armedOctave = 4;
    bool& synthWindowNeedsRedraw;
    bool& synthScopeLaneActive;
    std::array<Synthesizer, 4>& synthScopeLaneSynth;
    AppActionResult& lastAction;
};

bool ensureSynthWindowResources(const GuiSynthWindowLifecycleContext& context);
void releaseSynthBackbufferResources(const GuiSynthWindowLifecycleContext& context);
void ensureSynthBackbufferResources(const GuiSynthWindowLifecycleContext& context);
bool claimSynthPreviewKey(
    std::array<bool, 256>& synthPreviewKeyHeld,
    std::array<int, 256>& synthPreviewKeyMidi,
    unsigned int keycode,
    int midiNote);
void releaseSynthPreviewKey(
    std::array<bool, 256>& synthPreviewKeyHeld,
    std::array<int, 256>& synthPreviewKeyMidi,
    unsigned int keycode);
void clearSynthPreviewKeys(
    std::array<bool, 256>& synthPreviewKeyHeld,
    std::array<int, 256>& synthPreviewKeyMidi);
void clearSynthMidiPreviewNotes(std::array<bool, 128>& synthMidiPreviewHeld);
void setSynthWindowVisible(const GuiSynthWindowLifecycleContext& context, bool visible);

} // namespace arachno
