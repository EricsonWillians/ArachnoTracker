#include "ui/gui/GuiGridOps.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "Note.h"
#include "GuiInput.h"

namespace arachno {

AppActionResult moveCursor(
    const GuiGridOpsContext& context,
    int row,
    int track,
    bool refresh) {
    context.ensurePatternRowsForRow(std::max(0, row));
    AppActionRequest move;
    move.actionId = "editor.navigation.move";
    move.parameters = {
        {"row", std::to_string(std::max(0, row))},
        {"track", std::to_string(std::max(0, track))}};
    return context.runAction(move, refresh);
}

void applySelectionRange(
    const GuiGridOpsContext& context,
    int anchorRow,
    int anchorTrack,
    int targetRow,
    int targetTrack) {
    const int startRow = std::min(anchorRow, targetRow);
    const int startTrack = std::min(anchorTrack, targetTrack);
    const int rows = std::abs(targetRow - anchorRow) + 1;
    const int tracks = std::abs(targetTrack - anchorTrack) + 1;
    AppActionRequest select;
    select.actionId = "editor.selection.select";
    select.parameters = {
        {"row", std::to_string(std::max(0, startRow))},
        {"track", std::to_string(std::max(0, startTrack))},
        {"rows", std::to_string(std::max(1, rows))},
        {"tracks", std::to_string(std::max(1, tracks))}};
    (void)context.runAction(select, true);
}

void paintNoteAt(
    const GuiGridOpsContext& context,
    int row,
    int track,
    int midiNote,
    int& armedInstrument,
    float defaultVelocity) {
    const AppSessionSnapshot before = context.snapshot();
    const int instrumentCount = static_cast<int>(before.editor.instruments.size());
    if (instrumentCount <= 0) {
        return;
    }

    armedInstrument = std::clamp(armedInstrument, 0, instrumentCount - 1);
    (void)moveCursor(context, row, track, false);
    AppActionRequest note;
    note.actionId = "editor.step.note";
    note.parameters = {
        {"note", midiNoteName(std::clamp(midiNote, 0, 127))},
        {"velocity", velocityText(defaultVelocity)},
        {"index", std::to_string(armedInstrument)}};
    const AppActionResult noteResult = context.runAction(note, false);
    if (noteResult.ok) {
        AppActionRequest preview;
        preview.actionId = "preview.cursor";
        (void)context.runAction(preview, false);
    }
}

bool gridPositionToCell(
    const GuiGridLayout& layout,
    int viewStartRow,
    int gridTrackStart,
    int x,
    int y,
    int& row,
    int& track) {
    if (x < layout.gridLeft || x >= layout.gridLeft + layout.gridWidth
        || y < layout.gridTop || y >= layout.gridTop + layout.gridHeight) {
        return false;
    }
    const int headerBottom = layout.gridTop + 26;
    if (y < headerBottom) {
        return false;
    }
    const int firstTrackX = layout.gridLeft + layout.rowNumberWidth + 4;
    if (x < firstTrackX || layout.trackWidth <= 0 || layout.rowHeight <= 0 || layout.trackCols <= 0) {
        return false;
    }
    const int col = (x - firstTrackX) / layout.trackWidth;
    const int rowOffset = (y - headerBottom) / layout.rowHeight;
    if (col < 0 || col >= layout.trackCols || rowOffset < 0 || rowOffset >= layout.visibleRows) {
        return false;
    }
    row = viewStartRow + rowOffset;
    track = gridTrackStart + col;
    return true;
}

} // namespace arachno
