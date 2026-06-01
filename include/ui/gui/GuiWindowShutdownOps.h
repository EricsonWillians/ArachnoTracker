#pragma once

#include <functional>

#include <X11/Xlib.h>

namespace arachno {

void shutdownGuiWindowsAndResources(
    Display* display,
    XFontStruct* uiFont,
    std::function<void()> releaseTrackerBackbuffer,
    std::function<void()> releaseSynthBackbuffer,
    Window synthWindow,
    GC synthGc,
    GC gc,
    Window window);

} // namespace arachno
