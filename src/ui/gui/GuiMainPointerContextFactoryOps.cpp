#include "ui/gui/GuiMainPointerContextFactoryOps.h"

#include <algorithm>

#include <X11/X.h>

namespace arachno {

GuiMainWheelContext makeMainWheelContextFromState(const GuiMainWheelContextFactoryInput& input) {
    const auto state = input;
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
        [state](int direction) {
            const AppSessionSnapshot snap = state.activeSnapshot();
            const int totalTrackCols = std::max(1, snap.editor.activeGrid.trackCount);
            const int maxTrackStart = std::max(0, totalTrackCols - std::max(1, state.layout.trackCols));
            state.gridTrackStart = std::clamp(state.gridTrackStart + direction, 0, maxTrackStart);
        },
        input.scrollInstrumentList,
        [state](int wheelDelta) { state.resizePatternRows(state.activePatternRows + (wheelDelta * -2)); },
        input.lockManualScroll,
        input.refreshSnapshot,
        input.gridPositionToCell,
        input.applySelectionRange};
}

GuiMainGridClickContext makeMainGridClickContextFromState(const GuiMainGridClickContextFactoryInput& input) {
    const auto state = input;
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
        [state]() {
            const AppSessionSnapshot snap = state.activeSnapshot();
            return std::make_pair(snap.editor.status.cursorRow, snap.editor.status.cursorTrack);
        },
        input.moveCursor,
        input.paintNoteAt,
        input.refreshSnapshot,
        input.runActionById,
        input.applySelectionRange};
}

} // namespace arachno
