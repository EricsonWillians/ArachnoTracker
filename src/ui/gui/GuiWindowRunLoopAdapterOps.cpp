#include "ui/gui/GuiWindowRunLoopAdapterOps.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

#include "AppSettings.h"
#include "PatchIO.h"
#include "ui/gui/GuiFileBrowserOps.h"
#include "ui/gui/GuiPathDefaults.h"
#include "ui/gui/GuiMainGridClickOps.h"
#include "ui/gui/GuiMainKeyCommandOps.h"
#include "ui/gui/GuiMainKeyEditOps.h"
#include "ui/gui/GuiMainKeyModalOps.h"
#include "ui/gui/GuiMainKeyNoteOps.h"
#include "ui/gui/GuiMainModalMouseOps.h"
#include "ui/gui/GuiMainMotionOps.h"
#include "ui/gui/GuiMainPrimaryClickOps.h"
#include "ui/gui/GuiMainRunLoopOps.h"
#include "ui/gui/GuiMainSidebarClickOps.h"
#include "ui/gui/GuiMainWheelOps.h"

namespace arachno {

void runMainLoopFromAdapter(const GuiWindowRunLoopAdapterContext& context) {
    auto pollMidiInput = [&]() {
        return pollSynthMidiInputFromWindowState(context.synthInteractionContext);
    };

    // Patch browser UX poll: remembers the last used patch folder across launches and
    // auditions the highlighted patch file as the browser selection moves.
    std::string lastPatchDirectory;
    try {
        lastPatchDirectory = loadAppSettings(defaultSettingsPath().string()).browser.lastPatchDirectory;
    } catch (const std::exception&) {
        lastPatchDirectory.clear();
    }
    bool patchPromptWasActive = false;
    std::string lastPatchPreviewValue;
    auto pollPatchBrowserUx = [&]() {
        auto& ux = context.synthInteractionContext;
        const bool patchPromptActive = ux.inlinePrompt.active
            && inlinePromptKindIsPatch(ux.inlinePrompt.kind);
        if (!patchPromptActive) {
            patchPromptWasActive = false;
            lastPatchPreviewValue.clear();
            return false;
        }
        bool changed = false;
        if (!patchPromptWasActive) {
            patchPromptWasActive = true;
            lastPatchPreviewValue = ux.inlinePrompt.value;
            if (!lastPatchDirectory.empty()) {
                std::error_code ec;
                if (std::filesystem::is_directory(std::filesystem::path(lastPatchDirectory), ec) && !ec) {
                    ux.fileBrowserDirectory = std::filesystem::path(lastPatchDirectory);
                    ux.inlinePrompt.value = ux.fileBrowserDirectory.string();
                    ux.refreshFileBrowserEntries();
                    changed = true;
                }
            }
        }
        const std::string currentDir = ux.fileBrowserDirectory.string();
        if (!currentDir.empty() && currentDir != lastPatchDirectory) {
            lastPatchDirectory = currentDir;
        }
        if (inlinePromptKindPreviewsPatchFile(ux.inlinePrompt.kind)
            && ux.inlinePrompt.value != lastPatchPreviewValue) {
            lastPatchPreviewValue = ux.inlinePrompt.value;
            const std::filesystem::path candidate(ux.inlinePrompt.value);
            std::string extension = candidate.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            std::error_code ec;
            if (extension == ".arachnopatch" && std::filesystem::is_regular_file(candidate, ec) && !ec) {
                try {
                    const SynthPatch patch = loadPatch(candidate.string());
                    ux.session.playback().auditionPatch(
                        patch,
                        std::clamp(ux.synthPreviewMidi, 0, 127),
                        std::clamp(ux.synthDefaultVelocity, 0.05f, 1.0f),
                        0.6,
                        0.0,
                        false);
                    changed = true;
                } catch (const std::exception&) {
                    // Unreadable file while browsing: skip preview.
                }
            }
        }
        return changed;
    };
    auto pollInputAndPatchBrowserUx = [&]() {
        const bool patchBrowserChanged = pollPatchBrowserUx();
        return pollMidiInput() || patchBrowserChanged;
    };

    auto makeSynthWindowEventContext = [&]() {
        return makeSynthWindowEventContextFromWindowState(context.synthInteractionContext, context.isAutoRepeatRelease);
    };

    auto makeMainModalMouseContext = [&](int playbackSampleRate) {
        return makeMainModalMouseContextFromWindowState(context.mainInteractionContext, playbackSampleRate);
    };

    auto makeMainWheelContext = [&](int button, int mx, int my, unsigned int stateMask) {
        return makeMainWheelContextFromWindowState(context.mainInteractionContext, button, mx, my, stateMask);
    };

    auto makeMainGridClickContext = [&](int button, int mx, int my, bool altDown, bool shiftDown) {
        return makeMainGridClickContextFromWindowState(context.mainInteractionContext, button, mx, my, altDown, shiftDown);
    };

    auto makeMainMotionContext = [&](int mx, int my, unsigned int stateMask) {
        return makeMainMotionContextFromWindowState(context.mainInteractionContext, mx, my, stateMask);
    };

    auto makeMainKeyModalContext = [&](KeySym key,
                                       bool ctrlDown,
                                       bool shiftDown,
                                       bool altDown,
                                       int lookupCount,
                                       const char* lookupBuffer,
                                       const std::function<bool(KeySym)>& keyMatches,
                                       const std::function<int()>& resolvedDigit,
                                       int playbackSampleRate) {
        return makeMainKeyModalContextFromWindowState(
            context.mainInteractionContext,
            key,
            ctrlDown,
            shiftDown,
            altDown,
            lookupCount,
            lookupBuffer,
            keyMatches,
            resolvedDigit,
            playbackSampleRate);
    };

    auto makeMainKeyEditContext = [&](KeySym key,
                                      bool ctrlDown,
                                      bool shiftDown,
                                      bool altDown,
                                      const std::function<bool(KeySym)>& keyMatches,
                                      const std::function<int()>& resolvedDigit) {
        return makeMainKeyEditContextFromWindowState(
            context.mainInteractionContext,
            key,
            ctrlDown,
            shiftDown,
            altDown,
            keyMatches,
            resolvedDigit);
    };

    auto makeMainKeyCommandContext = [&](KeySym key,
                                         bool ctrlDown,
                                         bool shiftDown,
                                         bool altDown,
                                         const std::function<bool(KeySym)>& keyMatches) {
        return makeMainKeyCommandContextFromWindowState(
            context.mainInteractionContext,
            key,
            ctrlDown,
            shiftDown,
            altDown,
            keyMatches);
    };

    auto makeMainKeyNoteContext = [&](KeySym key,
                                      unsigned int keycode,
                                      bool ctrlDown,
                                      bool shiftDown,
                                      bool altDown,
                                      const std::function<bool(KeySym)>& keyMatches,
                                      const std::function<int()>& resolvedDigit) {
        return makeMainKeyNoteContextFromWindowState(
            context.mainInteractionContext,
            key,
            keycode,
            ctrlDown,
            shiftDown,
            altDown,
            keyMatches,
            resolvedDigit);
    };

    auto makeButtonPressFactoryInput = [&]() {
        return makeButtonPressFactoryInputFromWindowState(context.mainInteractionContext);
    };

    runMainLoopFromState(
        GuiMainRunLoopContext {
            context.display,
            context.window,
            context.wmDelete,
            context.running,
            context.needsRedraw,
            context.synthWindowNeedsRedraw,
            context.windowWidth,
            context.windowHeight,
            context.resizingSidebar,
            context.resizingTopPanel,
            context.draggingSelection,
            context.draggingPatternRows,
            context.paintingNotes,
            context.lastPaintRow,
            context.lastPaintTrack,
            context.sidebarViewport,
            context.layout,
            context.pointerX,
            context.pointerY,
            makeSynthWindowEventContext,
            context.isAutoRepeatRelease,
            context.releaseSynthPreviewKey,
            context.ensureTrackerBackbuffer,
            makeMainMotionContext,
            [&](const GuiMainMotionContext& motionContext) { return handleMainMotionNotify(motionContext); },
            context.playbackSampleRate,
            makeButtonPressFactoryInput,
            makeMainModalMouseContext,
            [&](const GuiMainModalMouseContext& modalContext, int button, int mx, int my, unsigned int stateMask) {
                return handleMainModalButtonPress(modalContext, button, mx, my, stateMask);
            },
            makeMainWheelContext,
            [&](const GuiMainWheelContext& wheelContext) { return handleMainWheel(wheelContext); },
            [&](const GuiMainPrimaryClickContext& primaryContext) { return handleMainPrimaryLeftClick(primaryContext); },
            [&](const GuiMainSidebarClickContext& sidebarContext) { return handleMainSidebarLeftClick(sidebarContext); },
            makeMainGridClickContext,
            [&](const GuiMainGridClickContext& gridContext) { return handleMainGridClick(gridContext); },
            makeMainKeyModalContext,
            [&](const GuiMainKeyModalContext& modalContext) { return handleMainKeyModal(modalContext); },
            makeMainKeyNoteContext,
            [&](const GuiMainKeyNoteContext& noteContext) { return handleMainKeyNoteOps(noteContext); },
            makeMainKeyCommandContext,
            [&](const GuiMainKeyCommandContext& commandContext) { return handleMainKeyCommands(commandContext); },
            makeMainKeyEditContext,
            [&](const GuiMainKeyEditContext& editContext) { return handleMainKeyEditOps(editContext); },
            context.synthWindowVisible,
            context.previousAudioStreamActive,
            context.synthTooltipParam,
            context.synthTooltipHoverSince,
            context.lastRefresh,
            context.lastPlayheadPoll,
            [&]() { return pollInputAndPatchBrowserUx(); },
            context.processRealtimeAudio,
            context.refreshSnapshot,
            context.pollPlayhead,
            context.drawMainWindow,
            context.drawSynthWindow,
            context.syncArmedInstrument,
            context.audioProducerActive});

    // Persist the last patch folder for the next launch; never block shutdown on this.
    try {
        AppSettings settings;
        try {
            settings = loadAppSettings(defaultSettingsPath().string());
        } catch (const std::exception&) {
            settings = AppSettings {};
        }
        settings.browser.lastPatchDirectory = lastPatchDirectory;
        std::error_code ec;
        std::filesystem::create_directories(defaultSettingsPath().parent_path(), ec);
        saveAppSettings(settings, defaultSettingsPath().string());
    } catch (const std::exception&) {
    }
}

} // namespace arachno
