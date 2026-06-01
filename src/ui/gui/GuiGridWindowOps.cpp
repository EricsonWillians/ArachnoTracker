#include "ui/gui/GuiGridWindowOps.h"

namespace arachno {

GuiGridOpsContext makeGridOpsContextFromWindowState(
    const std::function<AppSessionSnapshot()>& snapshot,
    const std::function<AppActionResult(const AppActionRequest&, bool)>& runAction,
    const std::function<void(int)>& ensurePatternRowsForRow) {
    GuiGridOpsContext context;
    context.snapshot = snapshot;
    context.runAction = runAction;
    context.ensurePatternRowsForRow = ensurePatternRowsForRow;
    return context;
}

AppActionResult moveCursorFromWindowState(
    const GuiGridOpsContext& context,
    int row,
    int track,
    bool refresh) {
    return arachno::moveCursor(context, row, track, refresh);
}

void applySelectionRangeFromWindowState(
    const GuiGridOpsContext& context,
    int anchorRow,
    int anchorTrack,
    int targetRow,
    int targetTrack) {
    arachno::applySelectionRange(context, anchorRow, anchorTrack, targetRow, targetTrack);
}

void paintNoteAtFromWindowState(
    const GuiGridOpsContext& context,
    int row,
    int track,
    int midiNote,
    int& armedInstrument,
    float defaultVelocity) {
    arachno::paintNoteAt(context, row, track, midiNote, armedInstrument, defaultVelocity);
}

bool gridPositionToCellFromWindowState(
    const TrackerWindowLayout& layout,
    int viewStartRow,
    int gridTrackStart,
    int x,
    int y,
    int& row,
    int& track) {
    GuiGridLayout gridLayout;
    gridLayout.gridLeft = layout.gridLeft;
    gridLayout.gridTop = layout.gridTop;
    gridLayout.gridWidth = layout.gridWidth;
    gridLayout.gridHeight = layout.gridHeight;
    gridLayout.rowNumberWidth = layout.rowNumberWidth;
    gridLayout.trackCols = layout.trackCols;
    gridLayout.trackWidth = layout.trackWidth;
    gridLayout.rowHeight = layout.rowHeight;
    gridLayout.visibleRows = layout.visibleRows;
    return arachno::gridPositionToCell(gridLayout, viewStartRow, gridTrackStart, x, y, row, track);
}

} // namespace arachno
