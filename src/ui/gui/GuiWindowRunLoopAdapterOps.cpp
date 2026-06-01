#include "ui/gui/GuiWindowRunLoopAdapterOps.h"

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
            [&]() { return pollMidiInput(); },
            context.processRealtimeAudio,
            context.refreshSnapshot,
            context.drawMainWindow,
            context.drawSynthWindow});
}

} // namespace arachno
