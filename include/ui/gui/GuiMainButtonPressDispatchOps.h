#pragma once

#include <functional>

#include "ui/gui/GuiMainGridClickOps.h"
#include "ui/gui/GuiMainModalMouseOps.h"
#include "ui/gui/GuiMainPrimaryClickOps.h"
#include "ui/gui/GuiMainSidebarClickOps.h"
#include "ui/gui/GuiMainWheelOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainButtonPressDispatchResult {
    bool consumed = false;
    bool needsRedraw = false;
};

struct GuiMainButtonPressDispatchContext {
    int button = 0;
    int mx = 0;
    int my = 0;
    unsigned int stateMask = 0;

    UiRect sidebarViewport;
    int verticalSplitterX = 0;
    int horizontalSplitterY = 0;

    int& pointerX;
    int& pointerY;
    bool& resizingSidebar;
    bool& resizingTopPanel;

    std::function<int()> playbackSampleRate;
    std::function<GuiMainModalMouseResult(int, int, int, int, unsigned int)> dispatchModalMouse;
    std::function<GuiMainWheelResult(int, int, int, unsigned int)> dispatchWheel;
    std::function<GuiMainPrimaryClickResult(int, int, int)> dispatchPrimaryLeft;
    std::function<GuiMainSidebarClickResult(bool, int, int)> dispatchSidebarLeft;
    std::function<GuiMainGridClickResult(int, int, int, bool, bool)> dispatchGridClick;
};

GuiMainButtonPressDispatchResult dispatchMainButtonPress(const GuiMainButtonPressDispatchContext& context);

} // namespace arachno
