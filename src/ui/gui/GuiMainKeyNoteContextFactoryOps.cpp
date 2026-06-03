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
            AppActionRequest inst;
            inst.actionId = "editor.step.instrument";
            inst.parameters = {{"index", std::to_string(instrumentIndex)}};
            AppActionResult setInstrument = state.runActionWithRefresh(inst, true);
            if (!setInstrument.ok) {
                return false;
            }
            AppActionRequest note;
            note.actionId = "editor.step.note";
            note.parameters = {
                {"note", midiNoteName(midiNote)},
                {"velocity", velocityText(state.defaultVelocity)}};
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
        }};
}

} // namespace arachno
