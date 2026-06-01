#pragma once

#include <functional>

#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainWheelResult {
    bool consumed = false;
    bool needsRedraw = false;
};

struct GuiMainWheelContext {
    int button = 0;
    int mx = 0;
    int my = 0;
    bool ctrlDown = false;
    bool shiftDown = false;
    bool pointerButton1Down = false;

    const TrackerWindowLayout& layout;
    UiRect& sidebarViewport;
    UiRect& instrumentListRect;

    int& sidebarContentHeight;
    int& sidebarScrollOffset;
    int& viewStartRow;

    bool draggingSelection = false;
    int dragAnchorRow = 0;
    int dragAnchorTrack = 0;

    std::function<void(int)> shiftGridTrackWindow;
    std::function<void(int)> scrollInstrumentList;
    std::function<void(int)> resizePatternRowsByWheelDelta;
    std::function<void()> lockManualScroll;
    std::function<void()> refreshSnapshot;
    std::function<bool(int, int, int&, int&)> gridPositionToCell;
    std::function<void(int, int, int, int)> applySelectionRange;
};

GuiMainWheelResult handleMainWheel(const GuiMainWheelContext& context);

} // namespace arachno
