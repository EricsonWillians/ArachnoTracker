#include "ui/gui/GuiMainPointerContextFactoryOps.h"

#include <algorithm>

#include <X11/X.h>

namespace arachno {

GuiMainWheelContext makeMainWheelContextFromState(const GuiMainWheelContextFactoryInput& input) {
    return GuiMainWheelContext {
        input.button,
        input.mx,
        input.my,
        (input.stateMask & ControlMask) != 0,
        (input.stateMask & ShiftMask) != 0,
        (input.stateMask & Button1Mask) != 0,
        input.layout,
        input.sidebarViewport,
        input.instrumentListRect,
        input.sidebarContentHeight,
        input.sidebarScrollOffset,
        input.viewStartRow,
        input.draggingSelection,
        input.dragAnchorRow,
        input.dragAnchorTrack,
        [&input](int direction) {
            const AppSessionSnapshot snap = input.activeSnapshot();
            const int totalTrackCols = std::max(1, snap.editor.activeGrid.trackCount);
            const int maxTrackStart = std::max(0, totalTrackCols - std::max(1, input.layout.trackCols));
            input.gridTrackStart = std::clamp(input.gridTrackStart + direction, 0, maxTrackStart);
        },
        input.scrollInstrumentList,
        [&input](int wheelDelta) { input.resizePatternRows(input.activePatternRows + (wheelDelta * -2)); },
        input.lockManualScroll,
        input.refreshSnapshot,
        input.gridPositionToCell,
        input.applySelectionRange};
}

GuiMainGridClickContext makeMainGridClickContextFromState(const GuiMainGridClickContextFactoryInput& input) {
    return GuiMainGridClickContext {
        input.button,
        input.mx,
        input.my,
        input.altDown,
        input.shiftDown,
        input.paintNoteMidi,
        input.keyboardSelectionActive,
        input.paintingNotes,
        input.lastPaintRow,
        input.lastPaintTrack,
        input.draggingSelection,
        input.dragAnchorRow,
        input.dragAnchorTrack,
        input.gridPositionToCell,
        [&input]() {
            const AppSessionSnapshot snap = input.activeSnapshot();
            return std::make_pair(snap.editor.status.cursorRow, snap.editor.status.cursorTrack);
        },
        input.moveCursor,
        input.paintNoteAt,
        input.refreshSnapshot,
        input.runActionById,
        input.applySelectionRange};
}

} // namespace arachno
