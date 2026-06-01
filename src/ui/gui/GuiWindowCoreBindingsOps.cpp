#include "ui/gui/GuiWindowCoreBindingsOps.h"

#include <algorithm>

#include "ui/gui/GuiInlinePromptOps.h"

namespace arachno {

GuiWindowCoreBindings makeWindowCoreBindingsFromState(const GuiWindowCoreBindingsInput& input) {
    auto* coreStateContext = &input.coreStateContext;
    auto* audioRuntime = &input.audioRuntime;
    auto display = input.display;
    const Window window = input.window;
    const int screen = input.screen;
    auto* windowWidth = &input.windowWidth;
    auto* windowHeight = &input.windowHeight;
    auto* trackerBackbuffer = &input.trackerBackbuffer;
    auto* trackerBackbufferWidth = &input.trackerBackbufferWidth;
    auto* trackerBackbufferHeight = &input.trackerBackbufferHeight;

    GuiWindowCoreBindings bindings;

    bindings.activeSnapshot = [coreStateContext]() {
        return activeSnapshotFromCoreState(*coreStateContext);
    };
    bindings.lockManualScroll = [coreStateContext](int milliseconds) {
        lockManualScrollFromCoreState(*coreStateContext, milliseconds);
    };
    bindings.releaseTrackerBackbuffer = [display, trackerBackbuffer, trackerBackbufferWidth, trackerBackbufferHeight]() {
        if (*trackerBackbuffer != 0) {
            XFreePixmap(display, *trackerBackbuffer);
            *trackerBackbuffer = 0;
        }
        *trackerBackbufferWidth = 0;
        *trackerBackbufferHeight = 0;
    };
    bindings.ensureTrackerBackbuffer = [=]() {
        const int targetWidth = std::max(1, *windowWidth);
        const int targetHeight = std::max(1, *windowHeight);
        if (*trackerBackbuffer != 0
            && *trackerBackbufferWidth == targetWidth
            && *trackerBackbufferHeight == targetHeight) {
            return;
        }
        bindings.releaseTrackerBackbuffer();
        *trackerBackbuffer = XCreatePixmap(
            display,
            window,
            static_cast<unsigned int>(targetWidth),
            static_cast<unsigned int>(targetHeight),
            static_cast<unsigned int>(DefaultDepth(display, screen)));
        if (*trackerBackbuffer != 0) {
            *trackerBackbufferWidth = targetWidth;
            *trackerBackbufferHeight = targetHeight;
        }
    };

    bindings.audibleTrackCount = [coreStateContext]() {
        const Song& song = coreStateContext->session.song();
        bool soloActive = false;
        for (const Track& track : song.tracks) {
            if (track.solo) {
                soloActive = true;
                break;
            }
        }
        int count = 0;
        for (const Track& track : song.tracks) {
            if (track.muted) {
                continue;
            }
            if (soloActive && !track.solo) {
                continue;
            }
            ++count;
        }
        return std::max(1, count);
    };
    bindings.tuneRealtimeAudioForLoad = [coreStateContext](int sampleRate) {
        tuneRealtimeAudioForLoadFromCoreState(*coreStateContext, sampleRate);
    };
    bindings.setAudioPerformanceMode = [coreStateContext](AudioPerformanceMode mode, int sampleRate) {
        setAudioPerformanceModeFromCoreState(*coreStateContext, mode, sampleRate);
    };
    bindings.adjustAudioCustomLevel = [coreStateContext](int delta, int sampleRate) {
        adjustAudioCustomLevelFromCoreState(*coreStateContext, delta, sampleRate);
    };
    bindings.closeAudioOutput = [audioRuntime]() {
        audioRuntime->close();
    };
    bindings.openAudioOutput = [audioRuntime](int sampleRate) {
        return audioRuntime->open(sampleRate);
    };
    bindings.writeAudioOutput = [audioRuntime](const float* left, const float* right, int frames) {
        return audioRuntime->write(left, right, frames);
    };
    bindings.refreshSnapshot = [coreStateContext]() {
        refreshSnapshotFromCoreState(*coreStateContext);
    };
    bindings.runAction = [coreStateContext](const AppActionRequest& request, bool refresh) {
        return runActionFromCoreState(*coreStateContext, request, refresh);
    };
    bindings.runSync = [coreStateContext](const std::string& mode) {
        runSyncFromCoreState(*coreStateContext, mode);
    };
    bindings.runEvents = [coreStateContext]() {
        runEventsFromCoreState(*coreStateContext);
    };

    auto* inlinePrompt = &input.inlinePrompt;
    auto* inlinePromptUsesFileBrowser = &input.inlinePromptUsesFileBrowser;
    auto* initFileBrowserFromPrompt = &input.initFileBrowserFromPrompt;
    auto* fileBrowserDirectory = &input.fileBrowserDirectory;
    auto* fileBrowserEntries = &input.fileBrowserEntries;
    auto* fileBrowserHits = &input.fileBrowserHits;
    auto* fileBrowserScroll = &input.fileBrowserScroll;
    auto* fileBrowserSelected = &input.fileBrowserSelected;
    auto* inlinePromptButtonsVisible = &input.inlinePromptButtonsVisible;
    auto* synthInlinePromptButtonsVisible = &input.synthInlinePromptButtonsVisible;

    bindings.beginInlinePrompt = [=](
                                   InlinePromptKind kind,
                                   const std::string& title,
                                   const std::string& hint,
                                   const std::string& initialValue,
                                   int targetTrack,
                                   int targetInstrument) {
        beginInlinePromptFromWindowState(
            GuiBeginInlinePromptContext {
                *inlinePrompt,
                *inlinePromptUsesFileBrowser,
                *initFileBrowserFromPrompt,
                *fileBrowserDirectory,
                *fileBrowserEntries,
                *fileBrowserHits,
                *fileBrowserScroll,
                *fileBrowserSelected},
            kind,
            title,
            hint,
            initialValue,
            targetTrack,
            targetInstrument);
    };
    bindings.clearInlinePrompt = [=]() {
        clearInlinePromptFromWindowState(
            GuiClearInlinePromptContext {
                *inlinePrompt,
                *inlinePromptButtonsVisible,
                *synthInlinePromptButtonsVisible,
                *fileBrowserDirectory,
                *fileBrowserEntries,
                *fileBrowserHits,
                *fileBrowserScroll,
                *fileBrowserSelected});
    };
    bindings.isSynthInlinePromptKind = [](InlinePromptKind kind) {
        return kind == InlinePromptKind::ImportPatchAsNewPath
            || kind == InlinePromptKind::ImportPatchReplacePath
            || kind == InlinePromptKind::ImportPatchReplaceAllPath
            || kind == InlinePromptKind::ExportPatchPath
            || kind == InlinePromptKind::RenameInstrument;
    };

    return bindings;
}

} // namespace arachno
