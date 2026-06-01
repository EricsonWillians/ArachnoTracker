#include "ui/gui/GuiArrangementWindowOps.h"

namespace arachno {

GuiArrangementOpsContext makeArrangementOpsContextFromWindowState(
    const std::function<AppSessionSnapshot()>& snapshot,
    const std::function<AppActionResult(const AppActionRequest&)>& runAction) {
    GuiArrangementOpsContext context;
    context.snapshot = snapshot;
    context.runAction = runAction;
    return context;
}

bool selectPatternIndexFromWindowState(
    int index,
    bool resetViewStart,
    const GuiArrangementOpsContext& arrangementContext,
    bool& keyboardSelectionActive,
    int& viewStartRow) {
    return arachno::selectPatternIndex(
        index,
        resetViewStart,
        arrangementContext,
        keyboardSelectionActive,
        viewStartRow);
}

bool selectOrderIndexFromWindowState(
    int index,
    bool syncPatternSelection,
    const GuiArrangementOpsContext& arrangementContext,
    int& selectedOrderIndex,
    bool& keyboardSelectionActive,
    int& viewStartRow) {
    return arachno::selectOrderIndex(
        index,
        syncPatternSelection,
        arrangementContext,
        selectedOrderIndex,
        keyboardSelectionActive,
        viewStartRow);
}

bool appendOrderFromActivePatternFromWindowState(
    const GuiArrangementOpsContext& arrangementContext,
    int& selectedOrderIndex) {
    return arachno::appendOrderFromActivePattern(arrangementContext, selectedOrderIndex);
}

bool insertOrderAtSelectionFromWindowState(
    const GuiArrangementOpsContext& arrangementContext,
    int& selectedOrderIndex) {
    return arachno::insertOrderAtSelection(arrangementContext, selectedOrderIndex);
}

bool removeSelectedOrderFromWindowState(
    const GuiArrangementOpsContext& arrangementContext,
    int& selectedOrderIndex) {
    return arachno::removeSelectedOrder(arrangementContext, selectedOrderIndex);
}

void buildSongToTargetSecondsFromWindowState(
    double targetSeconds,
    const GuiArrangementOpsContext& arrangementContext,
    const std::function<void(const GuiLastActionState&)>& applyLastActionState) {
    const GuiLastActionState state = arachno::buildSongToTargetSeconds(targetSeconds, arrangementContext);
    applyLastActionState(state);
}

void trimSongToTargetSecondsFromWindowState(
    double targetSeconds,
    const GuiArrangementOpsContext& arrangementContext,
    const std::function<void(const GuiLastActionState&)>& applyLastActionState) {
    const GuiLastActionState state = arachno::trimSongToTargetSeconds(targetSeconds, arrangementContext);
    applyLastActionState(state);
}

} // namespace arachno
