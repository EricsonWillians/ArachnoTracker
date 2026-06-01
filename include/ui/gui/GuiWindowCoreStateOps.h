#pragma once

#include <chrono>
#include <functional>
#include <string>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "GUI.h"
#include "ui/gui/GuiAudioRuntime.h"
#include "ui/gui/GuiSessionWindowOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiWindowCoreStateContext {
    ApplicationSession& session;
    const GuiShellOptions& options;
    AppActionResult& snapshotResult;
    AppActionResult& lastAction;

    int& viewStartRow;
    int& requestedRowCount;
    bool& followPlayback;
    std::chrono::steady_clock::time_point& manualScrollLockUntil;
    int& selectedOrderIndex;
    int& gridTrackStart;
    TrackerWindowLayout& layout;
    int& lastCursorTrackForGridFollow;
    int& activePatternRows;

    GuiAudioRuntime& audioRuntime;
    std::function<int()> audibleTrackCount;
};

AppSessionSnapshot activeSnapshotFromCoreState(const GuiWindowCoreStateContext& context);
void ensureVisibleRowFromCoreState(const GuiWindowCoreStateContext& context, int row);
void lockManualScrollFromCoreState(
    GuiWindowCoreStateContext& context,
    int milliseconds = 1600);

void tuneRealtimeAudioForLoadFromCoreState(GuiWindowCoreStateContext& context, int sampleRate);
void setAudioPerformanceModeFromCoreState(
    GuiWindowCoreStateContext& context,
    AudioPerformanceMode mode,
    int sampleRate);
void adjustAudioCustomLevelFromCoreState(
    GuiWindowCoreStateContext& context,
    int delta,
    int sampleRate);

void refreshSnapshotFromCoreState(GuiWindowCoreStateContext& context);
AppActionResult runActionFromCoreState(
    GuiWindowCoreStateContext& context,
    const AppActionRequest& request,
    bool refresh = true);
void runSyncFromCoreState(GuiWindowCoreStateContext& context, const std::string& mode);
void runEventsFromCoreState(GuiWindowCoreStateContext& context);

} // namespace arachno
