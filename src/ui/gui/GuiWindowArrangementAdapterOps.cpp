#include "ui/gui/GuiWindowArrangementAdapterOps.h"

namespace arachno {

GuiArrangementOpsContext makeArrangementOpsContextFromAdapter(const GuiWindowArrangementAdapterContext& context) {
    return GuiArrangementOpsContext {context.activeSnapshot, context.runAction};
}

bool selectPatternIndexFromAdapter(
    const GuiWindowArrangementAdapterContext& context,
    int index,
    bool resetViewStart) {
    return selectPatternIndex(
        index,
        resetViewStart,
        makeArrangementOpsContextFromAdapter(context),
        context.keyboardSelectionActive,
        context.viewStartRow);
}

bool selectOrderIndexFromAdapter(
    const GuiWindowArrangementAdapterContext& context,
    int index,
    bool syncPatternSelection) {
    return selectOrderIndex(
        index,
        syncPatternSelection,
        makeArrangementOpsContextFromAdapter(context),
        context.selectedOrderIndex,
        context.keyboardSelectionActive,
        context.viewStartRow);
}

bool appendOrderFromActivePatternFromAdapter(const GuiWindowArrangementAdapterContext& context) {
    return appendOrderFromActivePattern(
        makeArrangementOpsContextFromAdapter(context),
        context.selectedOrderIndex);
}

bool insertOrderAtSelectionFromAdapter(const GuiWindowArrangementAdapterContext& context) {
    return insertOrderAtSelection(
        makeArrangementOpsContextFromAdapter(context),
        context.selectedOrderIndex);
}

bool removeSelectedOrderFromAdapter(const GuiWindowArrangementAdapterContext& context) {
    return removeSelectedOrder(
        makeArrangementOpsContextFromAdapter(context),
        context.selectedOrderIndex);
}

void beginPatternCreatePromptFromAdapter(const GuiWindowArrangementAdapterContext& context) {
    arachno::beginPatternCreatePrompt(
        GuiPatternPromptContext {
            context.activeSnapshot,
            context.activePatternRows,
            context.beginInlinePrompt,
            context.normalizedPatternNameToken});
}

void beginPatternClonePromptFromAdapter(const GuiWindowArrangementAdapterContext& context) {
    arachno::beginPatternClonePrompt(
        GuiPatternPromptContext {
            context.activeSnapshot,
            context.activePatternRows,
            context.beginInlinePrompt,
            context.normalizedPatternNameToken});
}

bool deleteActivePatternFromAdapter(const GuiWindowArrangementAdapterContext& context) {
    return arachno::deleteActivePattern(
        GuiPatternDeleteContext {
            context.runAction,
            context.keyboardSelectionActive,
            context.viewStartRow});
}

bool executeTemporalPasteSpecFromAdapter(const GuiWindowArrangementAdapterContext& context, const std::string& specText) {
    const GuiLastActionState state = arachno::executeTemporalPasteSpec(
        specText,
        makeArrangementOpsContextFromAdapter(context),
        context.ensurePatternRowsForRow);
    context.applyLastActionState(state);
    return state.ok;
}

void runTrackMetadataActionFromAdapter(
    const GuiWindowArrangementAdapterContext& context,
    const std::string& role,
    int track) {
    arachno::runTrackMetadataAction(
        GuiTrackMetadataActionContext {
            context.activeSnapshot,
            context.beginInlinePrompt,
            context.runAction},
        role,
        track);
}

void buildSongToTargetSecondsFromAdapter(const GuiWindowArrangementAdapterContext& context, double targetSeconds) {
    const GuiLastActionState state = buildSongToTargetSeconds(
        targetSeconds,
        makeArrangementOpsContextFromAdapter(context));
    context.applyLastActionState(state);
}

void trimSongToTargetSecondsFromAdapter(const GuiWindowArrangementAdapterContext& context, double targetSeconds) {
    const GuiLastActionState state = trimSongToTargetSeconds(
        targetSeconds,
        makeArrangementOpsContextFromAdapter(context));
    context.applyLastActionState(state);
}

} // namespace arachno
