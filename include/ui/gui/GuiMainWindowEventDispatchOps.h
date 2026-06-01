#pragma once

#include <functional>

#include <X11/Xlib.h>

#include "ui/gui/GuiMainButtonPressDispatchOps.h"
#include "ui/gui/GuiMainEventScaffoldDispatchOps.h"
#include "ui/gui/GuiMainKeyPressDispatchOps.h"
#include "ui/gui/GuiSynthWindowEventOps.h"

namespace arachno {

struct GuiMainWindowEventDispatchContext {
    Window mainWindow = 0;
    bool& running;
    bool& needsRedraw;
    bool& synthWindowNeedsRedraw;

    std::function<GuiSynthWindowEventResult(const XEvent&)> dispatchSynthWindowEvent;
    std::function<GuiMainEventScaffoldDispatchResult(const XEvent&)> dispatchScaffoldEvent;
    std::function<GuiMainButtonPressDispatchResult(const XButtonEvent&)> dispatchButtonPressEvent;
    std::function<GuiMainKeyPressDispatchResult(const XKeyEvent&)> dispatchKeyPressEvent;
};

bool dispatchMainWindowEvent(const GuiMainWindowEventDispatchContext& context, const XEvent& event);

} // namespace arachno
