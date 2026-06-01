#include "ui/gui/GuiMainEventScaffoldDispatchOps.h"

namespace arachno {

GuiMainEventScaffoldDispatchResult dispatchMainEventScaffold(const GuiMainEventScaffoldDispatchContext& context) {
    if (context.event.type == Expose) {
        return GuiMainEventScaffoldDispatchResult {true, true, false};
    }
    if (context.event.type == ConfigureNotify) {
        context.windowWidth = context.event.xconfigure.width;
        context.windowHeight = context.event.xconfigure.height;
        context.ensureTrackerBackbuffer();
        return GuiMainEventScaffoldDispatchResult {true, true, false};
    }
    if (context.event.type == ClientMessage) {
        const bool shouldQuit = static_cast<Atom>(context.event.xclient.data.l[0]) == context.wmDelete;
        return GuiMainEventScaffoldDispatchResult {true, false, shouldQuit};
    }
    if (context.event.type == KeyRelease) {
        if (!context.isAutoRepeatRelease(context.event)) {
            context.releaseSynthPreviewKey(context.event.xkey.keycode);
        }
        return GuiMainEventScaffoldDispatchResult {true, false, false};
    }
    if (context.event.type == ButtonRelease) {
        context.resizingSidebar = false;
        context.resizingTopPanel = false;
        context.draggingSelection = false;
        context.draggingPatternRows = false;
        context.paintingNotes = false;
        context.lastPaintRow = -1;
        context.lastPaintTrack = -1;
        return GuiMainEventScaffoldDispatchResult {true, false, false};
    }
    if (context.event.type == MotionNotify) {
        const GuiMainMotionResult motionResult = context.dispatchMotion(
            context.event.xmotion.x,
            context.event.xmotion.y,
            static_cast<unsigned int>(context.event.xmotion.state));
        return GuiMainEventScaffoldDispatchResult {true, motionResult.needsRedraw, false};
    }
    return GuiMainEventScaffoldDispatchResult {};
}

} // namespace arachno
