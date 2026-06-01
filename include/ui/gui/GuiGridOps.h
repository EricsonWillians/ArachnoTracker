#pragma once

#include <functional>

#include "AppActions.h"

namespace arachno {

struct GuiGridLayout {
    int gridLeft = 0;
    int gridTop = 0;
    int gridWidth = 0;
    int gridHeight = 0;
    int rowNumberWidth = 0;
    int trackCols = 0;
    int trackWidth = 0;
    int rowHeight = 0;
    int visibleRows = 0;
};

struct GuiGridOpsContext {
    std::function<AppSessionSnapshot()> snapshot;
    std::function<AppActionResult(const AppActionRequest&, bool refresh)> runAction;
    std::function<void(int row)> ensurePatternRowsForRow;
};

AppActionResult moveCursor(
    const GuiGridOpsContext& context,
    int row,
    int track,
    bool refresh = true);

void applySelectionRange(
    const GuiGridOpsContext& context,
    int anchorRow,
    int anchorTrack,
    int targetRow,
    int targetTrack);

void paintNoteAt(
    const GuiGridOpsContext& context,
    int row,
    int track,
    int midiNote,
    int& armedInstrument,
    float defaultVelocity);

bool gridPositionToCell(
    const GuiGridLayout& layout,
    int viewStartRow,
    int gridTrackStart,
    int x,
    int y,
    int& row,
    int& track);

} // namespace arachno
