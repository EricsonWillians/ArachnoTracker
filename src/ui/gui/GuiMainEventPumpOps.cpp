#include "ui/gui/GuiMainEventPumpOps.h"

#include "ui/gui/GuiMainEventScaffoldDispatchOps.h"
#include "ui/gui/GuiMainWindowEventDispatchOps.h"
#include "ui/gui/GuiMainXEventDispatchWrappersOps.h"

namespace arachno {

void pumpPendingMainEvents(const GuiMainEventPumpContext& context) {
    while (XPending(context.display) > 0) {
        XEvent event;
        XNextEvent(context.display, &event);

        if (dispatchMainWindowEvent(
                GuiMainWindowEventDispatchContext {
                    context.mainWindow,
                    context.running,
                    context.needsRedraw,
                    context.synthWindowNeedsRedraw,
                    [&](const XEvent& nextEvent) {
                        return handleSynthWindowEvent(context.makeSynthWindowEventContext(), nextEvent);
                    },
                    [&](const XEvent& nextEvent) {
                        return dispatchMainEventScaffold(
                            GuiMainEventScaffoldDispatchContext {
                                nextEvent,
                                context.wmDelete,
                                context.windowWidth,
                                context.windowHeight,
                                context.resizingSidebar,
                                context.resizingTopPanel,
                                context.draggingSelection,
                                context.draggingPatternRows,
                                context.paintingNotes,
                                context.lastPaintRow,
                                context.lastPaintTrack,
                                context.ensureTrackerBackbuffer,
                                context.isAutoRepeatRelease,
                                context.releaseSynthPreviewKey,
                                [&](int mx, int my, unsigned int stateMask) {
                                    return context.handleMainMotionNotify(
                                        context.makeMainMotionContext(mx, my, stateMask));
                                }});
                    },
                    [&](const XButtonEvent& buttonEvent) {
                        return dispatchMainButtonXEvent(
                            GuiMainButtonXEventDispatchContext {
                                context.playbackSampleRate,
                                context.makeButtonPressFactoryInput,
                                context.sidebarViewport,
                                context.layout.verticalSplitterX,
                                context.layout.horizontalSplitterY,
                                context.pointerX,
                                context.pointerY,
                                context.resizingSidebar,
                                context.resizingTopPanel,
                                context.makeMainModalMouseContext,
                                context.handleMainModalButtonPress,
                                context.makeMainWheelContext,
                                context.handleMainWheel,
                                context.handleMainPrimaryLeftClick,
                                context.handleMainSidebarLeftClick,
                                context.makeMainGridClickContext,
                                context.handleMainGridClick},
                            buttonEvent);
                    },
                    [&](const XKeyEvent& keyEvent) {
                        return dispatchMainKeyXEvent(
                            GuiMainKeyXEventDispatchContext {
                                context.playbackSampleRate,
                                context.makeMainKeyModalContext,
                                context.handleMainKeyModal,
                                context.makeMainKeyNoteContext,
                                context.handleMainKeyNoteOps,
                                context.makeMainKeyCommandContext,
                                context.handleMainKeyCommands,
                                context.makeMainKeyEditContext,
                                context.handleMainKeyEditOps},
                            keyEvent);
                    }},
                event)) {
            continue;
        }
    }
}

} // namespace arachno
