#include "ui/gui/GuiMainSidebarContextFactoryOps.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

#include "ui/gui/GuiPathDefaults.h"

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
        input.legatoButton,
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
            const int count = static_cast<int>(snap.editor.instruments.size());
            if (count > 0) {
                const int resolvedInstrument = std::clamp(state.armedInstrument, 0, count - 1);
                state.selectInstrument(resolvedInstrument);
            }
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
            }
        },
        [state](const std::string& role) {
            if (role == "prev") {
                const AppSessionSnapshot snap = state.activeSnapshot();
                const int count = static_cast<int>(snap.editor.instruments.size());
                if (count > 0) {
                    const int armed = std::clamp(state.armedInstrument, 0, std::max(0, count - 1));
                    state.selectInstrument((armed + count - 1) % count);
                }
            } else if (role == "next") {
                const AppSessionSnapshot snap = state.activeSnapshot();
                const int count = static_cast<int>(snap.editor.instruments.size());
                if (count > 0) {
                    const int armed = std::clamp(state.armedInstrument, 0, std::max(0, count - 1));
                    state.selectInstrument((armed + 1) % count);
                }
            } else if (role == "audition") {
                state.auditionArmedInstrument();
            } else if (role == "browse") {
                state.openInstrumentBrowser();
            } else if (role == "load_patch" || role == "add_patch") {
                const AppSessionSnapshot snap = state.activeSnapshot();
                const int count = static_cast<int>(snap.editor.instruments.size());
                if (role == "load_patch" && count <= 0) {
                    return;
                }
                const int armed = count > 0 ? std::clamp(state.armedInstrument, 0, count - 1) : -1;
                const bool replace = role == "load_patch";
                state.beginInlinePrompt(
                    replace ? InlinePromptKind::ImportPatchReplacePath : InlinePromptKind::ImportPatchAsNewPath,
                    replace ? "Load patch into armed instrument" : "Import patch as new instrument",
                    "Path to .arachnopatch",
                    defaultPatchPath(
                        snap.hasProjectPath,
                        snap.projectPath,
                        replace && armed >= 0
                            ? snap.editor.instruments[static_cast<std::size_t>(armed)].name
                            : patchStemFromName("imported_patch"),
                        std::filesystem::current_path()),
                    -1,
                    armed);
            }
        },
        [state](int instrumentIndex) { state.selectInstrument(instrumentIndex); },
        [state]() { state.runActionById("editor.step.legato"); }};
}

} // namespace arachno
