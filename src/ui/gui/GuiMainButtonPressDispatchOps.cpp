#include "ui/gui/GuiMainButtonPressDispatchOps.h"

#include <X11/X.h>

#include <cmath>

namespace arachno {

GuiMainButtonPressDispatchResult dispatchMainButtonPress(const GuiMainButtonPressDispatchContext& context) {
    context.pointerX = context.mx;
    context.pointerY = context.my;
    const bool shiftDown = (context.stateMask & ShiftMask) != 0;
    const bool altDown = (context.stateMask & Mod1Mask) != 0;

    const int sampleRate = context.playbackSampleRate();
    const GuiMainModalMouseResult modalMouseResult = context.dispatchModalMouse(
        sampleRate,
        context.button,
        context.mx,
        context.my,
        context.stateMask);
    if (modalMouseResult.consumed) {
        return GuiMainButtonPressDispatchResult {true, modalMouseResult.needsRedraw};
    }

    if (std::abs(context.mx - context.verticalSplitterX) <= 6) {
        context.resizingSidebar = true;
        return GuiMainButtonPressDispatchResult {true, false};
    }
    if (std::abs(context.my - context.horizontalSplitterY) <= 6) {
        context.resizingTopPanel = true;
        return GuiMainButtonPressDispatchResult {true, false};
    }

    const GuiMainWheelResult wheelResult = context.dispatchWheel(
        context.button,
        context.mx,
        context.my,
        context.stateMask);
    if (wheelResult.consumed) {
        return GuiMainButtonPressDispatchResult {true, wheelResult.needsRedraw};
    }

    if (context.button == Button4 || context.button == Button5) {
        return GuiMainButtonPressDispatchResult {true, false};
    }

    if (context.button == Button1) {
        const bool pointerInSidebar = context.sidebarViewport.contains(context.mx, context.my);
        const GuiMainPrimaryClickResult primaryResult = context.dispatchPrimaryLeft(
            context.mx,
            context.my,
            sampleRate);
        if (primaryResult.consumed) {
            return GuiMainButtonPressDispatchResult {true, true};
        }
        const GuiMainSidebarClickResult sidebarResult = context.dispatchSidebarLeft(
            pointerInSidebar,
            context.mx,
            context.my);
        if (sidebarResult.consumed) {
            return GuiMainButtonPressDispatchResult {true, true};
        }
    }

    const GuiMainGridClickResult gridClickResult = context.dispatchGridClick(
        context.button,
        context.mx,
        context.my,
        altDown,
        shiftDown);
    if (gridClickResult.consumed) {
        return GuiMainButtonPressDispatchResult {true, gridClickResult.needsRedraw};
    }

    return GuiMainButtonPressDispatchResult {};
}

} // namespace arachno
