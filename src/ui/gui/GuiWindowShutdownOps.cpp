#include "ui/gui/GuiWindowShutdownOps.h"

namespace arachno {

void shutdownGuiWindowsAndResources(
    Display* display,
    XFontStruct* uiFont,
    std::function<void()> releaseTrackerBackbuffer,
    std::function<void()> releaseSynthBackbuffer,
    Window synthWindow,
    GC synthGc,
    GC gc,
    Window window) {
    releaseTrackerBackbuffer();
    if (uiFont != nullptr) {
        XFreeFont(display, uiFont);
    }
    releaseSynthBackbuffer();
    if (synthGc != nullptr) {
        XFreeGC(display, synthGc);
    }
    if (synthWindow != 0) {
        XDestroyWindow(display, synthWindow);
    }
    XFreeGC(display, gc);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
}

} // namespace arachno
