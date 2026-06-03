#include "ui/gui/GuiMainRunLoopOps.h"

namespace arachno {

void runMainLoopFromState(const GuiMainRunLoopContext& context) {
    while (context.running) {
        pumpPendingMainEvents(
            GuiMainEventPumpContext {
                context.display,
                context.mainWindow,
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
                context.makeSynthWindowEventContext,
                context.isAutoRepeatRelease,
                context.releaseSynthPreviewKey,
                context.ensureTrackerBackbuffer,
                context.makeMainMotionContext,
                context.handleMainMotionNotify,
                context.processRealtimeAudio,
                context.playbackSampleRate,
                context.makeButtonPressFactoryInput,
                context.makeMainModalMouseContext,
                context.handleMainModalButtonPress,
                context.makeMainWheelContext,
                context.handleMainWheel,
                context.handleMainPrimaryLeftClick,
                context.handleMainSidebarLeftClick,
                context.makeMainGridClickContext,
                context.handleMainGridClick,
                context.makeMainKeyModalContext,
                context.handleMainKeyModal,
                context.makeMainKeyNoteContext,
                context.handleMainKeyNoteOps,
                context.makeMainKeyCommandContext,
                context.handleMainKeyCommands,
                context.makeMainKeyEditContext,
                context.handleMainKeyEditOps});

        runMainLoopTick(
            GuiMainLoopTickContext {
                context.synthWindowVisible,
                context.synthWindowNeedsRedraw,
                context.needsRedraw,
                context.previousAudioStreamActive,
                context.synthTooltipParam,
                context.synthTooltipHoverSince,
                context.lastRefresh,
                context.pollMidiInput,
                context.processRealtimeAudio,
                context.refreshSnapshot,
                context.drawMainWindow,
                context.drawSynthWindow});
    }
}

} // namespace arachno
