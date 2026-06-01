#pragma once

#include <functional>

#include <X11/Xlib.h>

#include "ui/gui/GuiMainMotionOps.h"

namespace arachno {

struct GuiMainEventScaffoldDispatchResult {
    bool consumed = false;
    bool needsRedraw = false;
    bool shouldQuit = false;
};

struct GuiMainEventScaffoldDispatchContext {
    XEvent event {};
    Atom wmDelete = 0;
    int& windowWidth;
    int& windowHeight;

    bool& resizingSidebar;
    bool& resizingTopPanel;
    bool& draggingSelection;
    bool& draggingPatternRows;
    bool& paintingNotes;
    int& lastPaintRow;
    int& lastPaintTrack;

    std::function<void()> ensureTrackerBackbuffer;
    std::function<bool(const XEvent&)> isAutoRepeatRelease;
    std::function<void(unsigned int)> releaseSynthPreviewKey;
    std::function<GuiMainMotionResult(int, int, unsigned int)> dispatchMotion;
};

GuiMainEventScaffoldDispatchResult dispatchMainEventScaffold(const GuiMainEventScaffoldDispatchContext& context);

} // namespace arachno
