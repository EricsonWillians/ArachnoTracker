#pragma once

#include <functional>
#include <string>
#include <utility>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "ui/gui/GuiMainGridClickOps.h"
#include "ui/gui/GuiMainWheelOps.h"

namespace arachno {

struct GuiMainWheelContextFactoryInput {
    int button = 0;
    int mx = 0;
    int my = 0;
    unsigned int stateMask = 0;

    const TrackerWindowLayout& layout;
    UiRect& sidebarViewport;
    UiRect& instrumentListRect;
    int& sidebarContentHeight;
    int& sidebarScrollOffset;
    int& viewStartRow;
    bool draggingSelection = false;
    int dragAnchorRow = 0;
    int dragAnchorTrack = 0;

    int& gridTrackStart;
    int activePatternRows = 64;

    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(int)> scrollInstrumentList;
    std::function<void(int)> resizePatternRows;
    std::function<void()> lockManualScroll;
    std::function<void()> refreshSnapshot;
    std::function<bool(int, int, int&, int&)> gridPositionToCell;
    std::function<void(int, int, int, int)> applySelectionRange;
};

GuiMainWheelContext makeMainWheelContextFromState(const GuiMainWheelContextFactoryInput& input);

struct GuiMainGridClickContextFactoryInput {
    int button = 0;
    int mx = 0;
    int my = 0;
    bool altDown = false;
    bool shiftDown = false;

    int& paintNoteMidi;
    bool& keyboardSelectionActive;
    bool& paintingNotes;
    int& lastPaintRow;
    int& lastPaintTrack;
    bool& draggingSelection;
    int& dragAnchorRow;
    int& dragAnchorTrack;

    std::function<bool(int, int, int&, int&)> gridPositionToCell;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(int, int)> moveCursor;
    std::function<void(int, int, int)> paintNoteAt;
    std::function<void()> refreshSnapshot;
    std::function<void(const std::string&)> runActionById;
    std::function<void(int, int, int, int)> applySelectionRange;
};

GuiMainGridClickContext makeMainGridClickContextFromState(const GuiMainGridClickContextFactoryInput& input);

} // namespace arachno
