#pragma once

#include <functional>
#include <string>

#include <X11/Xlib.h>

#include "AppActions.h"
#include "ui/gui/GuiMidiInput.h"

namespace arachno {

struct GuiWindowStartupContext {
    std::function<void(const std::string&)> runSync;
    std::function<void()> refreshSnapshot;
    std::function<AppActionResult(const AppActionRequest&, bool)> runAction;
    std::function<void()> ensureTrackerBackbuffer;
    GuiMidiInput& midiInput;
};

void startupGuiWindowSession(const GuiWindowStartupContext& context);

struct GuiWindowShutdownContext {
    Display* display = nullptr;
    XFontStruct* uiFont = nullptr;
    Window synthWindow = 0;
    GC synthGc = nullptr;
    GC gc = nullptr;
    Window window = 0;

    std::function<void()> closeAudioOutput;
    GuiMidiInput& midiInput;
    std::function<void()> releaseTrackerBackbuffer;
    std::function<void()> releaseSynthBackbuffer;
};

void shutdownGuiWindowSession(const GuiWindowShutdownContext& context);

} // namespace arachno
