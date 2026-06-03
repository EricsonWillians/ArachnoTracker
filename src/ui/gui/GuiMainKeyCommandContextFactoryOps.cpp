#include "ui/gui/GuiMainKeyCommandContextFactoryOps.h"

#include <algorithm>
#include <sstream>

#include "ui/gui/GuiPathDefaults.h"

namespace arachno {

GuiMainKeyCommandContext makeMainKeyCommandContextFromState(const GuiMainKeyCommandContextFactoryInput& input) {
    const auto state = input;
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
        [state]() {
            const AppSessionSnapshot snap = state.activeSnapshot();
            state.beginInlinePrompt(
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
        [state]() {
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
        },
        input.setSynthWindowVisible,
        [state](int direction) {
            const AppSessionSnapshot snap = state.activeSnapshot();
            const int count = static_cast<int>(snap.editor.patterns.size());
            if (count > 0) {
                const int current = std::clamp(snap.editor.status.activePattern, 0, count - 1);
                const int next = direction < 0 ? (current + count - 1) % count : (current + 1) % count;
                (void)state.selectPatternIndex(next, true);
            }
        },
        [state](int direction) {
            const AppSessionSnapshot snap = state.activeSnapshot();
            const int count = static_cast<int>(snap.editor.order.size());
            if (count > 0) {
                const int current = std::clamp(state.selectedOrderIndex, 0, count - 1);
                const int next = direction < 0 ? (current + count - 1) % count : (current + 1) % count;
                (void)state.selectOrderIndex(next, true);
            }
        },
        input.insertOrderAtSelection,
        input.appendOrderFromActivePattern,
        input.removeSelectedOrder,
        input.buildSongToTargetSeconds,
        input.trimSongToTargetSeconds,
        [state](double delta) {
            AppActionRequest velocityNudge;
            velocityNudge.actionId = "editor.step.velocity_nudge";
            velocityNudge.parameters = {{"velocity_delta", std::to_string(delta)}};
            state.runAction(velocityNudge);
        },
        [state](int semitones) {
            AppActionRequest transposeSelection;
            transposeSelection.actionId = "editor.selection.transpose";
            transposeSelection.parameters = {{"semitones", std::to_string(semitones)}};
            state.runAction(transposeSelection);
        },
        [state]() {
            const AppSessionSnapshot snap = state.activeSnapshot();
            const int selectionRows = std::max(1, snap.editor.status.selectionRows);
            AppActionRequest repeatSelection;
            repeatSelection.actionId = "editor.selection.repeat";
            repeatSelection.parameters = {
                {"repeats", "1"},
                {"row_spacing", std::to_string(selectionRows)},
                {"track_spacing", "0"}};
            state.runAction(repeatSelection);
        }};
}

} // namespace arachno
