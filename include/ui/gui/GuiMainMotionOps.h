#pragma once

#include <functional>
#include <vector>

#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainMotionResult {
    bool consumed = false;
    bool needsRedraw = false;
};

struct GuiMainMotionContext {
    int mx = 0;
    int my = 0;
    unsigned int stateMask = 0;

    int& pointerX;
    int& pointerY;
    int windowWidth = 1280;
    int windowHeight = 800;
    const TrackerWindowLayout& layout;

    bool resizingSidebar = false;
    bool resizingTopPanel = false;
    bool& draggingPatternRows;
    bool& paintingNotes;
    bool draggingSelection = false;
    int& patternResizeAnchorY;
    int patternResizeStartRows = 64;
    int& topPanelHeightState;
    int& sidebarWidthState;
    int& paintNoteMidi;
    int& lastPaintRow;
    int& lastPaintTrack;
    int dragAnchorRow = 0;
    int dragAnchorTrack = 0;
    int& viewStartRow;
    int& hoveredTrackHeader;

    const std::vector<TrackHeaderHit>& trackHeaderHits;

    std::function<void(int)> resizePatternRows;
    std::function<bool(int, int, int&, int&)> gridPositionToCell;
    std::function<void(int, int, int)> paintNoteAt;
    std::function<void()> refreshSnapshot;
    std::function<void(int, int, int, int)> applySelectionRange;
    std::function<void()> lockManualScroll;
};

GuiMainMotionResult handleMainMotionNotify(const GuiMainMotionContext& context);

} // namespace arachno
