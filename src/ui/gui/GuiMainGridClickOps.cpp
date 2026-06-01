#include "ui/gui/GuiMainGridClickOps.h"

#include <algorithm>

#include <X11/X.h>

namespace arachno {

GuiMainGridClickResult handleMainGridClick(const GuiMainGridClickContext& context) {
    GuiMainGridClickResult result;

    int row = 0;
    int track = 0;
    if (context.button == Button1 && context.gridPositionToCell(context.mx, context.my, row, track)) {
        context.keyboardSelectionActive = false;
        if (context.altDown) {
            context.paintNoteAt(row, track, context.paintNoteMidi);
            context.paintingNotes = true;
            context.lastPaintRow = row;
            context.lastPaintTrack = track;
            context.refreshSnapshot();
            result.consumed = true;
            result.needsRedraw = true;
            return result;
        }

        const std::pair<int, int> before = context.currentCursor();
        context.moveCursor(row, track);
        context.draggingSelection = true;
        if (context.shiftDown) {
            context.dragAnchorRow = std::max(0, before.first);
            context.dragAnchorTrack = std::max(0, before.second);
        } else {
            context.dragAnchorRow = std::max(0, row);
            context.dragAnchorTrack = std::max(0, track);
        }
        if (context.shiftDown) {
            context.applySelectionRange(context.dragAnchorRow, context.dragAnchorTrack, row, track);
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    if (context.button == Button2 && context.gridPositionToCell(context.mx, context.my, row, track)) {
        context.moveCursor(row, track);
        context.runActionById("preview.cursor");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    if (context.button == Button3 && context.gridPositionToCell(context.mx, context.my, row, track)) {
        context.keyboardSelectionActive = false;
        context.moveCursor(row, track);
        context.runActionById("editor.step.clear");
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    return result;
}

} // namespace arachno
