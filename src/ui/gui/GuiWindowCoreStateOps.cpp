#include "ui/gui/GuiWindowCoreStateOps.h"

#include <algorithm>

namespace arachno {

AppSessionSnapshot activeSnapshotFromCoreState(const GuiWindowCoreStateContext& context) {
    return context.snapshotResult.hasSessionSnapshot
        ? context.snapshotResult.sessionSnapshot
        : context.session.snapshot(std::max(0, context.viewStartRow), std::max(1, context.requestedRowCount));
}

void ensureVisibleRowFromCoreState(const GuiWindowCoreStateContext& context, int row) {
    if (row < context.viewStartRow) {
        context.viewStartRow = std::max(0, row);
    } else if (row >= context.viewStartRow + context.requestedRowCount) {
        context.viewStartRow = std::max(0, row - context.requestedRowCount + 1);
    }
}

void lockManualScrollFromCoreState(
    GuiWindowCoreStateContext& context,
    int milliseconds) {
    context.manualScrollLockUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
}

void tuneRealtimeAudioForLoadFromCoreState(GuiWindowCoreStateContext& context, int sampleRate) {
    context.audioRuntime.tuneForLoad(sampleRate, context.audibleTrackCount());
}

void setAudioPerformanceModeFromCoreState(
    GuiWindowCoreStateContext& context,
    AudioPerformanceMode mode,
    int sampleRate) {
    context.audioRuntime.setPerformanceMode(mode, sampleRate, context.audibleTrackCount());
}

void adjustAudioCustomLevelFromCoreState(
    GuiWindowCoreStateContext& context,
    int delta,
    int sampleRate) {
    context.audioRuntime.adjustCustomLevel(delta, sampleRate, context.audibleTrackCount());
}

void refreshSnapshotFromCoreState(GuiWindowCoreStateContext& context) {
    ::arachno::refreshSnapshotFromWindowState(
        GuiSnapshotRefreshContext {
            context.session,
            context.snapshotResult,
            context.viewStartRow,
            context.requestedRowCount,
            context.followPlayback,
            context.manualScrollLockUntil,
            context.selectedOrderIndex,
            context.gridTrackStart,
            context.layout,
            context.lastCursorTrackForGridFollow,
            context.activePatternRows,
            [&](int row) { ensureVisibleRowFromCoreState(context, row); }});
}

AppActionResult runActionFromCoreState(
    GuiWindowCoreStateContext& context,
    const AppActionRequest& request,
    bool refresh) {
    context.lastAction = executeAppAction(context.session, request);
    if (refresh) {
        refreshSnapshotFromCoreState(context);
    }
    return context.lastAction;
}

void runSyncFromCoreState(GuiWindowCoreStateContext& context, const std::string& mode) {
    AppActionRequest sync;
    sync.actionId = "session.sync";
    sync.parameters = {
        {"checkpoint_name", context.options.checkpointName},
        {"create_if_missing", context.options.createCheckpointIfMissing ? "true" : "false"},
        {"mode", mode},
        {"max_events", std::to_string(context.options.maxEvents)},
        {"snapshot_grid_start_row", std::to_string(std::max(0, context.viewStartRow))},
        {"snapshot_grid_row_count", std::to_string(std::max(1, context.requestedRowCount))},
        {"update_checkpoint", "true"}};
    (void)runActionFromCoreState(context, sync);
}

void runEventsFromCoreState(GuiWindowCoreStateContext& context) {
    AppActionRequest events;
    events.actionId = "session.events";
    events.parameters = {{"max_events", std::to_string(context.options.maxEvents)}};
    (void)runActionFromCoreState(context, events);
}

} // namespace arachno
