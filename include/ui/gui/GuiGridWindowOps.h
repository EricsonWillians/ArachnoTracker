#pragma once

#include <functional>

#include "ui/gui/GuiGridOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

GuiGridOpsContext makeGridOpsContextFromWindowState(
    const std::function<AppSessionSnapshot()>& snapshot,
    const std::function<AppActionResult(const AppActionRequest&, bool)>& runAction,
    const std::function<void(int)>& ensurePatternRowsForRow);

AppActionResult moveCursorFromWindowState(
    const GuiGridOpsContext& context,
    int row,
    int track,
    bool refresh = true);

void applySelectionRangeFromWindowState(
    const GuiGridOpsContext& context,
    int anchorRow,
    int anchorTrack,
    int targetRow,
    int targetTrack);

void paintNoteAtFromWindowState(
    const GuiGridOpsContext& context,
    int row,
    int track,
    int midiNote,
    int& armedInstrument,
    float defaultVelocity);

bool gridPositionToCellFromWindowState(
    const TrackerWindowLayout& layout,
    int viewStartRow,
    int gridTrackStart,
    int x,
    int y,
    int& row,
    int& track);

} // namespace arachno
