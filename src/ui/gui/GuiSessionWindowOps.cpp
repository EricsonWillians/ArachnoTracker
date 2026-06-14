#include "ui/gui/GuiSessionWindowOps.h"

#include <algorithm>

namespace arachno {

void refreshSnapshotFromWindowState(const GuiSnapshotRefreshContext& context) {
    for (int attempt = 0; attempt < 2; ++attempt) {
        AppActionRequest snapshot;
        snapshot.actionId = "session.snapshot";
        snapshot.parameters = {
            {"grid_start_row", std::to_string(std::max(0, context.viewStartRow))},
            {"grid_row_count", std::to_string(std::max(1, context.requestedRowCount))}};
        context.snapshotResult = executeAppAction(context.session, snapshot);
        if (!context.snapshotResult.hasSessionSnapshot) {
            return;
        }

        const AppSessionSnapshot& snap = context.snapshotResult.sessionSnapshot;

        const bool autoFollowAllowed =
            context.followPlayback && std::chrono::steady_clock::now() >= context.manualScrollLockUntil;
        if (autoFollowAllowed
            && snap.playback.state == TransportState::Playing
            && !snap.playback.loop.enabled
            && snap.playback.position.pattern >= 0
            && snap.playback.position.pattern < static_cast<int>(snap.editor.patterns.size())
            && snap.playback.position.pattern != snap.editor.status.activePattern) {
            AppActionRequest followPattern;
            followPattern.actionId = "editor.navigation.pattern";
            followPattern.parameters = {{"index", std::to_string(snap.playback.position.pattern)}};
            const AppActionResult followResult = executeAppAction(context.session, followPattern);
            if (followResult.ok) {
                continue;
            }
        }

        const int orderCount = static_cast<int>(snap.editor.order.size());
        if (orderCount <= 0) {
            context.selectedOrderIndex = 0;
        } else if (snap.playback.state == TransportState::Playing
            && !snap.playback.loop.enabled
            && snap.playback.position.orderIndex >= 0
            && snap.playback.position.orderIndex < orderCount) {
            context.selectedOrderIndex = snap.playback.position.orderIndex;
        } else {
            context.selectedOrderIndex = std::clamp(context.selectedOrderIndex, 0, orderCount - 1);
        }

        const int cursorTrack = std::max(0, snap.editor.status.cursorTrack);
        if (cursorTrack != context.lastCursorTrackForGridFollow) {
            if (cursorTrack < context.gridTrackStart) {
                context.gridTrackStart = cursorTrack;
            } else if (context.layout.trackCols > 0 && cursorTrack >= context.gridTrackStart + context.layout.trackCols) {
                context.gridTrackStart = cursorTrack - context.layout.trackCols + 1;
            }
            context.lastCursorTrackForGridFollow = cursorTrack;
        }

        for (const PatternSummary& pattern : snap.editor.patterns) {
            if (pattern.active) {
                context.activePatternRows = std::max(8, pattern.rowCount);
                break;
            }
        }
        if (autoFollowAllowed) {
            const int focusRow = (snap.playback.state == TransportState::Playing
                    && snap.playback.position.pattern == snap.editor.status.activePattern)
                ? snap.playback.position.patternRow
                : snap.editor.status.cursorRow;
            context.ensureVisible(std::max(0, focusRow));
        }
        const int maxStart = std::max(0, context.activePatternRows - 1);
        const int clamped = std::clamp(context.viewStartRow, 0, maxStart);
        if (clamped == context.viewStartRow || attempt == 1) {
            context.viewStartRow = clamped;
            return;
        }
        context.viewStartRow = clamped;
    }
}

void beginInlinePromptFromWindowState(
    const GuiBeginInlinePromptContext& context,
    InlinePromptKind kind,
    const std::string& title,
    const std::string& hint,
    const std::string& initialValue,
    int targetTrack,
    int targetInstrument) {
    context.inlinePrompt.kind = kind;
    context.inlinePrompt.active = true;
    context.inlinePrompt.title = title;
    context.inlinePrompt.hint = hint;
    context.inlinePrompt.value = initialValue;
    context.inlinePrompt.targetTrack = targetTrack;
    context.inlinePrompt.targetInstrument = targetInstrument;
    if (context.inlinePromptUsesFileBrowser(kind)) {
        context.initFileBrowserFromPrompt();
        return;
    }
    context.fileBrowserDirectory.clear();
    context.fileBrowserEntries.clear();
    context.fileBrowserHits.clear();
    context.fileBrowserScroll = 0;
    context.fileBrowserSelected = -1;
}

void clearInlinePromptFromWindowState(const GuiClearInlinePromptContext& context) {
    context.inlinePrompt = InlinePromptState {};
    context.inlinePromptButtonsVisible = false;
    context.synthInlinePromptButtonsVisible = false;
    context.fileBrowserDirectory.clear();
    context.fileBrowserEntries.clear();
    context.fileBrowserHits.clear();
    context.fileBrowserScroll = 0;
    context.fileBrowserSelected = -1;
}

void cancelInlinePromptFromWindowState(const GuiCancelInlinePromptContext& context) {
    if (context.inlinePrompt.active && context.inlinePrompt.kind == InlinePromptKind::SaveProjectPath) {
        context.hasDeferredPostSaveAction = false;
    }
    context.clearInlinePrompt();
}

void clearUnsavedPromptFromWindowState(
    UnsavedDecisionPromptState& unsavedPrompt,
    std::vector<std::pair<UiRect, UnsavedChangesChoice>>& unsavedPromptChoices) {
    unsavedPrompt = UnsavedDecisionPromptState {};
    unsavedPromptChoices.clear();
}

void runLifecycleActionFromWindowState(const GuiLifecycleActionContext& context, const AppActionRequest& request) {
    const AppActionResult result = context.runAction(request);
    if (result.ok && !result.requiresUnsavedDecision && !result.requiresSaveAs) {
        context.refreshSnapshot();
    }
    if (result.requiresUnsavedDecision) {
        context.unsavedPrompt.active = true;
        context.unsavedPrompt.request = request;
        context.unsavedPrompt.title = result.lifecyclePlan.unsavedPrompt.title.empty()
            ? "Unsaved changes"
            : result.lifecyclePlan.unsavedPrompt.title;
        context.unsavedPrompt.detail = result.lifecyclePlan.unsavedPrompt.detail.empty()
            ? result.lifecyclePlan.unsavedPrompt.message
            : result.lifecyclePlan.unsavedPrompt.detail;
        return;
    }
    if (!result.requiresSaveAs) {
        return;
    }
    context.hasDeferredPostSaveAction = true;
    context.deferredPostSaveAction = request;
    context.beginInlinePrompt(
        InlinePromptKind::SaveProjectPath,
        "Save project before continue",
        "Path to save current project",
        "gui_project.arachno",
        -1,
        -1);
}

void handleDeferredPostSaveActionFromWindowState(
    bool& hasDeferredPostSaveAction,
    AppActionRequest& deferredPostSaveAction,
    const std::function<void(const AppActionRequest&)>& runLifecycleAction) {
    if (!hasDeferredPostSaveAction) {
        return;
    }
    AppActionRequest deferred = deferredPostSaveAction;
    hasDeferredPostSaveAction = false;
    deferred.unsavedChoice = UnsavedChangesChoice::NotNeeded;
    runLifecycleAction(deferred);
}

} // namespace arachno
