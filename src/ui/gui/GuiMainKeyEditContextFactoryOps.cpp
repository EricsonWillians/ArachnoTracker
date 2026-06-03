#include "ui/gui/GuiMainKeyEditContextFactoryOps.h"

#include <algorithm>

namespace arachno {

GuiMainKeyEditContext makeMainKeyEditContextFromState(const GuiMainKeyEditContextFactoryInput& input) {
    const auto state = input;
    return GuiMainKeyEditContext {
        input.key,
        input.ctrlDown,
        input.shiftDown,
        input.altDown,
        input.synthWindowVisible,
        input.stepAdvance,
        input.defaultVelocity,
        input.armedOctave,
        input.synthPreviewMidi,
        input.paintNoteMidi,
        input.activePatternRows,
        input.requestedRowCount,
        input.viewStartRow,
        input.keyboardSelectionActive,
        input.keyboardSelectionAnchorRow,
        input.keyboardSelectionAnchorTrack,
        input.keyMatches,
        input.resolvedDigit,
        input.setArmedOctave,
        input.applyArmedOctaveToSelection,
        input.ensureSynthKeyboardShowsMidi,
        input.cycleInstrumentBy,
        input.selectInstrument,
        input.auditionArmedInstrument,
        [state]() {
            const AppSessionSnapshot snap = state.activeSnapshot();
            return std::make_pair(snap.editor.status.cursorRow, snap.editor.status.cursorTrack);
        },
        input.moveCursor,
        input.paintNoteAt,
        input.refreshSnapshot,
        input.ensurePatternRowsForRow,
        input.runActionById,
        input.applySelectionRange,
        input.lockManualScroll,
        [state]() {
            const AppSessionSnapshot snap = state.activeSnapshot();
            return static_cast<int>(snap.editor.instruments.size());
        },
        [state]() {
            const AppSessionSnapshot snap = state.activeSnapshot();
            const int rows = std::max(1, state.activePatternRows);
            const int tracks = std::max(1, snap.editor.activeGrid.trackCount);
            AppActionRequest selectAll;
            selectAll.actionId = "editor.selection.select";
            selectAll.parameters = {
                {"row", "0"},
                {"track", "0"},
                {"rows", std::to_string(rows)},
                {"tracks", std::to_string(tracks)}};
            state.runAction(selectAll);
        }};
}

} // namespace arachno
