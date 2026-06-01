#include "ui/gui/GuiMainKeyCommandContextFactoryOps.h"

#include <algorithm>
#include <sstream>

#include "ui/gui/GuiPathDefaults.h"

namespace arachno {

GuiMainKeyCommandContext makeMainKeyCommandContextFromState(const GuiMainKeyCommandContextFactoryInput& input) {
    return GuiMainKeyCommandContext {
        input.key,
        input.ctrlDown,
        input.shiftDown,
        input.altDown,
        input.audioTuningDialogActive,
        input.midiImportSplitByTrack,
        input.synthWindowVisible,
        input.targetSongLengthMinutes,
        input.themeMode,
        input.playbackSampleRate,
        input.transportPlaying,
        input.keyMatches,
        input.setAudioPerformanceMode,
        input.currentPerformanceMode,
        input.runActionById,
        input.runSyncById,
        input.runEvents,
        input.openInstrumentBrowser,
        input.cycleInstrumentBy,
        input.runFileButtonAction,
        [&input]() {
            const AppSessionSnapshot snap = input.activeSnapshot();
            input.beginInlinePrompt(
                InlinePromptKind::SaveProjectPath,
                "Save project as",
                "Path to save project",
                defaultProjectPath(snap.hasProjectPath, snap.projectPath),
                -1,
                -1);
        },
        input.beginTemporalPastePrompt,
        input.beginPatternCreatePrompt,
        input.beginPatternClonePrompt,
        input.deleteActivePattern,
        [&input]() {
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
        },
        input.setSynthWindowVisible,
        [&input](int direction) {
            const AppSessionSnapshot snap = input.activeSnapshot();
            const int count = static_cast<int>(snap.editor.patterns.size());
            if (count > 0) {
                const int current = std::clamp(snap.editor.status.activePattern, 0, count - 1);
                const int next = direction < 0 ? (current + count - 1) % count : (current + 1) % count;
                (void)input.selectPatternIndex(next, true);
            }
        },
        [&input](int direction) {
            const AppSessionSnapshot snap = input.activeSnapshot();
            const int count = static_cast<int>(snap.editor.order.size());
            if (count > 0) {
                const int current = std::clamp(input.selectedOrderIndex, 0, count - 1);
                const int next = direction < 0 ? (current + count - 1) % count : (current + 1) % count;
                (void)input.selectOrderIndex(next, true);
            }
        },
        input.insertOrderAtSelection,
        input.appendOrderFromActivePattern,
        input.removeSelectedOrder,
        input.buildSongToTargetSeconds,
        input.trimSongToTargetSeconds,
        [&input](double delta) {
            AppActionRequest velocityNudge;
            velocityNudge.actionId = "editor.step.velocity_nudge";
            velocityNudge.parameters = {{"velocity_delta", std::to_string(delta)}};
            input.runAction(velocityNudge);
        },
        [&input](int semitones) {
            AppActionRequest transposeSelection;
            transposeSelection.actionId = "editor.selection.transpose";
            transposeSelection.parameters = {{"semitones", std::to_string(semitones)}};
            input.runAction(transposeSelection);
        },
        [&input]() {
            const AppSessionSnapshot snap = input.activeSnapshot();
            const int selectionRows = std::max(1, snap.editor.status.selectionRows);
            AppActionRequest repeatSelection;
            repeatSelection.actionId = "editor.selection.repeat";
            repeatSelection.parameters = {
                {"repeats", "1"},
                {"row_spacing", std::to_string(selectionRows)},
                {"track_spacing", "0"}};
            input.runAction(repeatSelection);
        }};
}

} // namespace arachno
