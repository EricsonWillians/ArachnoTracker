#include "ui/gui/GuiMainSidebarContextFactoryOps.h"

#include <algorithm>
#include <sstream>

namespace arachno {

GuiMainSidebarClickContext makeMainSidebarClickContextFromState(const GuiMainSidebarContextFactoryInput& input) {
    const auto state = input;
    return GuiMainSidebarClickContext {
        input.pointerInSidebar,
        input.mx,
        input.my,
        input.octaveHitTargets,
        input.pianoKeyHits,
        input.trackMetadataHits,
        input.songLengthHits,
        input.midiImportSettingHits,
        input.instrumentControlHits,
        input.instrumentHitTargets,
        input.patternRowsMinus,
        input.patternRowsPlus,
        input.patternRowsValue,
        input.stepAdvanceButton,
        input.followPlaybackButton,
        input.draggingPatternRows,
        input.patternResizeAnchorY,
        input.patternResizeStartRows,
        input.activePatternRows,
        input.stepAdvance,
        input.followPlayback,
        input.paintNoteMidi,
        [state](int octaveHitValue) {
            if (octaveHitValue == -1) {
                state.setArmedOctave(state.armedOctave - 1);
            } else if (octaveHitValue == 100) {
                state.setArmedOctave(state.armedOctave + 1);
            } else {
                state.setArmedOctave(octaveHitValue);
            }
            state.applyArmedOctaveToSelection();
        },
        [state](int midi) {
            state.paintNoteMidi = midi;
            const AppSessionSnapshot snap = state.activeSnapshot();
            state.paintNoteAt(snap.editor.status.cursorRow, snap.editor.status.cursorTrack, state.paintNoteMidi);
            if (state.stepAdvance) {
                state.ensurePatternRowsForRow(snap.editor.status.cursorRow + 1);
                state.runActionById("editor.navigation.down");
            } else {
                state.refreshSnapshot();
            }
        },
        [state](const std::string& role, int track) { state.runTrackMetadataAction(role, track); },
        [state](int rows) { state.resizePatternRows(rows); },
        [state](const std::string& role) {
            if (role == "target_down") {
                state.targetSongLengthMinutes = std::max(0.1, state.targetSongLengthMinutes - 0.25);
            } else if (role == "target_up") {
                state.targetSongLengthMinutes = std::min(180.0, state.targetSongLengthMinutes + 0.25);
            } else if (role == "target_set") {
                std::ostringstream initial;
                initial.setf(std::ios::fixed);
                initial.precision(2);
                initial << state.targetSongLengthMinutes;
                state.beginInlinePrompt(
                    InlinePromptKind::SongLengthMinutes,
                    "Set track length (minutes)",
                    "Example: 4.50",
                    initial.str(),
                    -1,
                    -1);
            } else if (role == "build") {
                state.buildSongToTargetSeconds(state.targetSongLengthMinutes * 60.0);
            } else if (role == "trim") {
                state.trimSongToTargetSeconds(state.targetSongLengthMinutes * 60.0);
            }
        },
        [state](const std::string& role) {
            if (role == "rpb_down") {
                state.midiImportRowsPerBeat = std::max(1, state.midiImportRowsPerBeat - 1);
            } else if (role == "rpb_up") {
                state.midiImportRowsPerBeat = std::min(32, state.midiImportRowsPerBeat + 1);
            } else if (role == "rows_down") {
                state.midiImportPatternRows = std::max(16, state.midiImportPatternRows - 16);
            } else if (role == "rows_up") {
                state.midiImportPatternRows = std::min(8192, state.midiImportPatternRows + 16);
            } else if (role == "split_toggle") {
                state.midiImportSplitByTrack = !state.midiImportSplitByTrack;
            } else if (role == "import") {
                state.runFileButtonAction("import.midi");
            }
        },
        [state](const std::string& role) {
            if (role == "prev") {
                const AppSessionSnapshot snap = state.activeSnapshot();
                const int count = static_cast<int>(snap.editor.instruments.size());
                if (count > 0) {
                    state.armedInstrument = (state.armedInstrument + count - 1) % count;
                    state.selectInstrument(state.armedInstrument);
                }
            } else if (role == "next") {
                const AppSessionSnapshot snap = state.activeSnapshot();
                const int count = static_cast<int>(snap.editor.instruments.size());
                if (count > 0) {
                    state.armedInstrument = (state.armedInstrument + 1) % count;
                    state.selectInstrument(state.armedInstrument);
                }
            } else if (role == "audition") {
                state.auditionArmedInstrument();
            } else if (role == "browse") {
                state.openInstrumentBrowser();
            }
        },
        [state](int instrumentIndex) { state.selectInstrument(instrumentIndex); }};
}

} // namespace arachno
