#include "ui/gui/GuiMainKeyNoteContextFactoryOps.h"

#include "GuiInput.h"

namespace arachno {

GuiMainKeyNoteContext makeMainKeyNoteContextFromState(const GuiMainKeyNoteContextFactoryInput& input) {
    const auto state = input;
    return GuiMainKeyNoteContext {
        input.key,
        input.keycode,
        input.ctrlDown,
        input.shiftDown,
        input.altDown,
        input.synthWindowVisible,
        input.armedOctave,
        input.armedInstrument,
        input.synthPreviewMidi,
        input.paintNoteMidi,
        input.stepAdvance,
        input.keyMatches,
        input.resolvedDigit,
        input.setArmedOctave,
        input.ensureSynthKeyboardShowsMidi,
        input.claimSynthPreviewKey,
        input.auditionSynthPreviewMidi,
        input.selectInstrument,
        [state]() {
            const AppSessionSnapshot snap = state.activeSnapshot();
            return static_cast<int>(snap.editor.instruments.size());
        },
        [state](int instrumentIndex, int midiNote, bool shouldStepAdvance) {
            const AppSessionSnapshot snap = state.activeSnapshot();
            const int count = static_cast<int>(snap.editor.instruments.size());
            const int resolvedInstrument =
                count <= 0 ? -1 : std::clamp(instrumentIndex, 0, count - 1);
            if (resolvedInstrument < 0) {
                return false;
            }
            AppActionRequest note;
            note.actionId = "editor.step.note";
            note.parameters = {
                {"note", midiNoteName(midiNote)},
                {"velocity", velocityText(state.defaultVelocity)},
                {"index", std::to_string(resolvedInstrument)}};
            AppActionResult noteResult = state.runActionWithRefresh(note, true);
            if (!noteResult.ok) {
                return false;
            }
            AppActionRequest preview;
            preview.actionId = "preview.cursor";
            (void)state.runActionWithRefresh(preview, false);
            if (shouldStepAdvance) {
                const AppSessionSnapshot snap = state.activeSnapshot();
                state.ensurePatternRowsForRow(snap.editor.status.cursorRow + 1);
                AppActionRequest down;
                down.actionId = "editor.navigation.down";
                (void)state.runActionWithRefresh(down, true);
            }
            return true;
        },
        [state](bool shouldStepAdvance) {
            AppActionRequest noteOff;
            noteOff.actionId = "editor.step.noteoff";
            AppActionResult noteOffResult = state.runActionWithRefresh(noteOff, true);
            if (!noteOffResult.ok) {
                return false;
            }
            if (shouldStepAdvance) {
                const AppSessionSnapshot snap = state.activeSnapshot();
                state.ensurePatternRowsForRow(snap.editor.status.cursorRow + 1);
                AppActionRequest down;
                down.actionId = "editor.navigation.down";
                (void)state.runActionWithRefresh(down, true);
            }
            return true;
        }};
}

} // namespace arachno
