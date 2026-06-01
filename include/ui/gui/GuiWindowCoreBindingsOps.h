#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <X11/Xlib.h>

#include "AppActions.h"
#include "ui/gui/GuiWindowCoreStateOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiWindowCoreBindingsInput {
    GuiWindowCoreStateContext& coreStateContext;
    GuiAudioRuntime& audioRuntime;
    Display* display = nullptr;
    Window window = 0;
    int screen = 0;
    int& windowWidth;
    int& windowHeight;
    Pixmap& trackerBackbuffer;
    int& trackerBackbufferWidth;
    int& trackerBackbufferHeight;

    InlinePromptState& inlinePrompt;
    std::function<bool(InlinePromptKind)>& inlinePromptUsesFileBrowser;
    std::function<void()>& initFileBrowserFromPrompt;
    std::filesystem::path& fileBrowserDirectory;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::vector<FileBrowserHit>& fileBrowserHits;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    bool& inlinePromptButtonsVisible;
    bool& synthInlinePromptButtonsVisible;
};

struct GuiWindowCoreBindings {
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(int)> lockManualScroll;
    std::function<void()> releaseTrackerBackbuffer;
    std::function<void()> ensureTrackerBackbuffer;
    std::function<int()> audibleTrackCount;
    std::function<void(int)> tuneRealtimeAudioForLoad;
    std::function<void(AudioPerformanceMode, int)> setAudioPerformanceMode;
    std::function<void(int, int)> adjustAudioCustomLevel;
    std::function<void()> closeAudioOutput;
    std::function<bool(int)> openAudioOutput;
    std::function<bool(const float*, const float*, int)> writeAudioOutput;
    std::function<void()> refreshSnapshot;
    std::function<AppActionResult(const AppActionRequest&, bool)> runAction;
    std::function<void(const std::string&)> runSync;
    std::function<void()> runEvents;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
    std::function<void()> clearInlinePrompt;
    std::function<bool(InlinePromptKind)> isSynthInlinePromptKind;
};

GuiWindowCoreBindings makeWindowCoreBindingsFromState(const GuiWindowCoreBindingsInput& input);

} // namespace arachno
