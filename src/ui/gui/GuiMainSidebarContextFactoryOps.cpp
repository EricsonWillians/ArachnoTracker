#include "ui/gui/GuiMainSidebarContextFactoryOps.h"

#include <algorithm>
#include <sstream>

namespace arachno {

GuiMainSidebarClickContext makeMainSidebarClickContextFromState(const GuiMainSidebarContextFactoryInput& input) {
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
        [&input](int octaveHitValue) {
            if (octaveHitValue == -1) {
                input.setArmedOctave(input.armedOctave - 1);
            } else if (octaveHitValue == 100) {
                input.setArmedOctave(input.armedOctave + 1);
            } else {
                input.setArmedOctave(octaveHitValue);
            }
            input.applyArmedOctaveToSelection();
        },
        [&input](int midi) {
            input.paintNoteMidi = midi;
            const AppSessionSnapshot snap = input.activeSnapshot();
            input.paintNoteAt(snap.editor.status.cursorRow, snap.editor.status.cursorTrack, input.paintNoteMidi);
            if (input.stepAdvance) {
                input.ensurePatternRowsForRow(snap.editor.status.cursorRow + 1);
                input.runActionById("editor.navigation.down");
            } else {
                input.refreshSnapshot();
            }
        },
        [&input](const std::string& role, int track) { input.runTrackMetadataAction(role, track); },
        [&input](int rows) { input.resizePatternRows(rows); },
        [&input](const std::string& role) {
            if (role == "target_down") {
                input.targetSongLengthMinutes = std::max(0.1, input.targetSongLengthMinutes - 0.25);
            } else if (role == "target_up") {
                input.targetSongLengthMinutes = std::min(180.0, input.targetSongLengthMinutes + 0.25);
            } else if (role == "target_set") {
                std::ostringstream initial;
                initial.setf(std::ios::fixed);
                initial.precision(2);
                initial << input.targetSongLengthMinutes;
                input.beginInlinePrompt(
                    InlinePromptKind::SongLengthMinutes,
                    "Set track length (minutes)",
                    "Example: 4.50",
                    initial.str(),
                    -1,
                    -1);
            } else if (role == "build") {
                input.buildSongToTargetSeconds(input.targetSongLengthMinutes * 60.0);
            } else if (role == "trim") {
                input.trimSongToTargetSeconds(input.targetSongLengthMinutes * 60.0);
            }
        },
        [&input](const std::string& role) {
            if (role == "rpb_down") {
                input.midiImportRowsPerBeat = std::max(1, input.midiImportRowsPerBeat - 1);
            } else if (role == "rpb_up") {
                input.midiImportRowsPerBeat = std::min(32, input.midiImportRowsPerBeat + 1);
            } else if (role == "rows_down") {
                input.midiImportPatternRows = std::max(16, input.midiImportPatternRows - 16);
            } else if (role == "rows_up") {
                input.midiImportPatternRows = std::min(8192, input.midiImportPatternRows + 16);
            } else if (role == "split_toggle") {
                input.midiImportSplitByTrack = !input.midiImportSplitByTrack;
            } else if (role == "import") {
                input.runFileButtonAction("import.midi");
            }
        },
        [&input](const std::string& role) {
            if (role == "prev") {
                const AppSessionSnapshot snap = input.activeSnapshot();
                const int count = static_cast<int>(snap.editor.instruments.size());
                if (count > 0) {
                    input.armedInstrument = (input.armedInstrument + count - 1) % count;
                    input.selectInstrument(input.armedInstrument);
                }
            } else if (role == "next") {
                const AppSessionSnapshot snap = input.activeSnapshot();
                const int count = static_cast<int>(snap.editor.instruments.size());
                if (count > 0) {
                    input.armedInstrument = (input.armedInstrument + 1) % count;
                    input.selectInstrument(input.armedInstrument);
                }
            } else if (role == "audition") {
                input.auditionArmedInstrument();
            } else if (role == "browse") {
                input.openInstrumentBrowser();
            }
        },
        [&input](int instrumentIndex) { input.selectInstrument(instrumentIndex); }};
}

} // namespace arachno
