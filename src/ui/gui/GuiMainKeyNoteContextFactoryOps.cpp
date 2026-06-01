#include "ui/gui/GuiMainKeyNoteContextFactoryOps.h"

#include "GuiInput.h"

namespace arachno {

GuiMainKeyNoteContext makeMainKeyNoteContextFromState(const GuiMainKeyNoteContextFactoryInput& input) {
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
        [&input]() {
            const AppSessionSnapshot snap = input.activeSnapshot();
            return static_cast<int>(snap.editor.instruments.size());
        },
        [&input](int instrumentIndex, int midiNote, bool shouldStepAdvance) {
            AppActionRequest inst;
            inst.actionId = "editor.step.instrument";
            inst.parameters = {{"index", std::to_string(instrumentIndex)}};
            AppActionResult setInstrument = input.runActionWithRefresh(inst, true);
            if (!setInstrument.ok) {
                return false;
            }
            AppActionRequest note;
            note.actionId = "editor.step.note";
            note.parameters = {
                {"note", midiNoteName(midiNote)},
                {"velocity", velocityText(input.defaultVelocity)}};
            AppActionResult noteResult = input.runActionWithRefresh(note, true);
            if (!noteResult.ok) {
                return false;
            }
            AppActionRequest preview;
            preview.actionId = "preview.cursor";
            (void)input.runActionWithRefresh(preview, false);
            if (shouldStepAdvance) {
                const AppSessionSnapshot snap = input.activeSnapshot();
                input.ensurePatternRowsForRow(snap.editor.status.cursorRow + 1);
                AppActionRequest down;
                down.actionId = "editor.navigation.down";
                (void)input.runActionWithRefresh(down, true);
            }
            return true;
        }};
}

} // namespace arachno
