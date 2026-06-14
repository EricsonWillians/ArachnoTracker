#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiSnapshotRefreshContext {
    ApplicationSession& session;
    AppActionResult& snapshotResult;
    int& viewStartRow;
    int requestedRowCount = 16;
    bool followPlayback = true;
    std::chrono::steady_clock::time_point manualScrollLockUntil {};

    int& selectedOrderIndex;
    int& gridTrackStart;
    const TrackerWindowLayout& layout;
    int& lastCursorTrackForGridFollow;
    int& activePatternRows;

    std::function<void(int)> ensureVisible;
};

void refreshSnapshotFromWindowState(const GuiSnapshotRefreshContext& context);

struct GuiBeginInlinePromptContext {
    InlinePromptState& inlinePrompt;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    std::function<void()> initFileBrowserFromPrompt;

    std::filesystem::path& fileBrowserDirectory;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::vector<FileBrowserHit>& fileBrowserHits;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
};

void beginInlinePromptFromWindowState(
    const GuiBeginInlinePromptContext& context,
    InlinePromptKind kind,
    const std::string& title,
    const std::string& hint,
    const std::string& initialValue,
    int targetTrack,
    int targetInstrument);

struct GuiClearInlinePromptContext {
    InlinePromptState& inlinePrompt;
    bool& inlinePromptButtonsVisible;
    bool& synthInlinePromptButtonsVisible;
    std::filesystem::path& fileBrowserDirectory;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::vector<FileBrowserHit>& fileBrowserHits;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
};

void clearInlinePromptFromWindowState(const GuiClearInlinePromptContext& context);

struct GuiCancelInlinePromptContext {
    const InlinePromptState& inlinePrompt;
    bool& hasDeferredPostSaveAction;
    std::function<void()> clearInlinePrompt;
};

void cancelInlinePromptFromWindowState(const GuiCancelInlinePromptContext& context);

void clearUnsavedPromptFromWindowState(
    UnsavedDecisionPromptState& unsavedPrompt,
    std::vector<std::pair<UiRect, UnsavedChangesChoice>>& unsavedPromptChoices);

struct GuiLifecycleActionContext {
    std::function<AppActionResult(const AppActionRequest&)> runAction;
    UnsavedDecisionPromptState& unsavedPrompt;
    bool& hasDeferredPostSaveAction;
    AppActionRequest& deferredPostSaveAction;
    std::function<void()> refreshSnapshot;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
};

void runLifecycleActionFromWindowState(const GuiLifecycleActionContext& context, const AppActionRequest& request);

void handleDeferredPostSaveActionFromWindowState(
    bool& hasDeferredPostSaveAction,
    AppActionRequest& deferredPostSaveAction,
    const std::function<void(const AppActionRequest&)>& runLifecycleAction);

} // namespace arachno
