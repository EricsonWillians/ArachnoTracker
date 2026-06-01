#include "ui/gui/GuiWindowEditingBindingsOps.h"

#include <memory>
#include <utility>

#include "ui/gui/GuiGridWindowOps.h"
#include "ui/gui/GuiTemporalPaste.h"
#include "ui/gui/GuiWindowArrangementAdapterOps.h"

namespace arachno {

GuiWindowEditingBindings makeWindowEditingBindingsFromState(const GuiWindowEditingBindingsContext& context) {
    auto arrangementAdapter = std::make_shared<GuiWindowArrangementAdapterContext>(
        GuiWindowArrangementAdapterContext {
            context.selectedOrderIndex,
            context.keyboardSelectionActive,
            context.viewStartRow,
            context.activePatternRows,
            context.activeSnapshot,
            context.runAction,
            context.beginInlinePrompt,
            context.normalizedPatternNameToken,
            context.ensurePatternRowsForRow,
            context.applyLastActionState});

    auto gridOpsContext = std::make_shared<GuiGridOpsContext>(makeGridOpsContextFromWindowState(
        context.activeSnapshot,
        context.runActionWithRefresh,
        context.ensurePatternRowsForRow));

    GuiWindowEditingBindings bindings;
    bindings.selectPatternIndex = [arrangementAdapter](int index, bool resetViewStart) {
        return selectPatternIndexFromAdapter(*arrangementAdapter, index, resetViewStart);
    };
    bindings.selectOrderIndex = [arrangementAdapter](int index, bool syncPatternSelection) {
        return selectOrderIndexFromAdapter(*arrangementAdapter, index, syncPatternSelection);
    };
    bindings.appendOrderFromActivePattern = [arrangementAdapter]() {
        return appendOrderFromActivePatternFromAdapter(*arrangementAdapter);
    };
    bindings.insertOrderAtSelection = [arrangementAdapter]() {
        return insertOrderAtSelectionFromAdapter(*arrangementAdapter);
    };
    bindings.removeSelectedOrder = [arrangementAdapter]() {
        return removeSelectedOrderFromAdapter(*arrangementAdapter);
    };
    bindings.beginPatternCreatePrompt = [arrangementAdapter]() {
        beginPatternCreatePromptFromAdapter(*arrangementAdapter);
    };
    bindings.beginPatternClonePrompt = [arrangementAdapter]() {
        beginPatternClonePromptFromAdapter(*arrangementAdapter);
    };
    bindings.deleteActivePattern = [arrangementAdapter]() {
        return deleteActivePatternFromAdapter(*arrangementAdapter);
    };
    bindings.beginTemporalPastePrompt = [beginInlinePrompt = context.beginInlinePrompt]() {
        arachno::beginTemporalPastePrompt(beginInlinePrompt);
    };
    bindings.executeTemporalPasteSpec = [arrangementAdapter](const std::string& specText) {
        return executeTemporalPasteSpecFromAdapter(*arrangementAdapter, specText);
    };
    bindings.runTrackMetadataAction = [arrangementAdapter](const std::string& role, int track) {
        runTrackMetadataActionFromAdapter(*arrangementAdapter, role, track);
    };
    bindings.buildSongToTargetSeconds = [arrangementAdapter](double targetSeconds) {
        buildSongToTargetSecondsFromAdapter(*arrangementAdapter, targetSeconds);
    };
    bindings.trimSongToTargetSeconds = [arrangementAdapter](double targetSeconds) {
        trimSongToTargetSecondsFromAdapter(*arrangementAdapter, targetSeconds);
    };
    bindings.moveCursor = [gridOpsContext](int row, int track, bool refresh) {
        return moveCursorFromWindowState(*gridOpsContext, row, track, refresh);
    };
    bindings.applySelectionRange = [gridOpsContext](int anchorRow, int anchorTrack, int targetRow, int targetTrack) {
        applySelectionRangeFromWindowState(*gridOpsContext, anchorRow, anchorTrack, targetRow, targetTrack);
    };
    int& armedInstrument = context.armedInstrument;
    float& defaultVelocity = context.defaultVelocity;
    TrackerWindowLayout& layout = context.layout;
    int& viewStartRow = context.viewStartRow;
    int& gridTrackStart = context.gridTrackStart;

    bindings.paintNoteAt = [gridOpsContext, &armedInstrument, &defaultVelocity](int row, int track, int midiNote) {
        paintNoteAtFromWindowState(*gridOpsContext, row, track, midiNote, armedInstrument, defaultVelocity);
    };
    bindings.gridPositionToCell = [&layout, &viewStartRow, &gridTrackStart](int x, int y, int& row, int& track) {
        return gridPositionToCellFromWindowState(layout, viewStartRow, gridTrackStart, x, y, row, track);
    };

    return bindings;
}

} // namespace arachno
