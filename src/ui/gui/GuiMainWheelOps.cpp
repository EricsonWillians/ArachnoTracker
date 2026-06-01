#include "ui/gui/GuiMainWheelOps.h"

#include <algorithm>

#include <X11/X.h>

namespace arachno {

GuiMainWheelResult handleMainWheel(const GuiMainWheelContext& context) {
    GuiMainWheelResult result;
    if (context.button != Button4 && context.button != Button5) {
        return result;
    }

    if (context.shiftDown
        && context.mx >= context.layout.gridLeft
        && context.mx < context.layout.gridLeft + context.layout.gridWidth
        && context.my >= context.layout.gridTop
        && context.my < context.layout.gridTop + context.layout.gridHeight) {
        const int horizontalDelta = context.button == Button4 ? -1 : 1;
        context.shiftGridTrackWindow(horizontalDelta);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    const int delta = context.button == Button4 ? -4 : 4;
    if (!context.ctrlDown && context.sidebarViewport.contains(context.mx, context.my)) {
        if (context.instrumentListRect.contains(context.mx, context.my)) {
            const int listDelta = context.button == Button4 ? -1 : 1;
            context.scrollInstrumentList(listDelta);
        } else {
            const int scrollDelta = context.button == Button4 ? -28 : 28;
            const int sidebarViewportHeight = std::max(1, context.sidebarViewport.height - 8);
            const int maxSidebarScroll = std::max(0, context.sidebarContentHeight - sidebarViewportHeight);
            context.sidebarScrollOffset = std::clamp(context.sidebarScrollOffset + scrollDelta, 0, maxSidebarScroll);
        }
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }
    if (!context.ctrlDown && context.instrumentListRect.contains(context.mx, context.my)) {
        const int listDelta = context.button == Button4 ? -1 : 1;
        context.scrollInstrumentList(listDelta);
        result.consumed = true;
        result.needsRedraw = true;
        return result;
    }

    if (context.ctrlDown) {
        context.resizePatternRowsByWheelDelta(delta);
    } else {
        context.lockManualScroll();
        context.viewStartRow = std::max(0, context.viewStartRow + delta);
        context.refreshSnapshot();
        if (context.draggingSelection && context.pointerButton1Down) {
            int row = 0;
            int track = 0;
            if (context.gridPositionToCell(context.mx, context.my, row, track)) {
                context.applySelectionRange(context.dragAnchorRow, context.dragAnchorTrack, row, track);
            }
        }
    }

    result.consumed = true;
    result.needsRedraw = true;
    return result;
}

} // namespace arachno
