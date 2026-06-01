#include "ui/gui/GuiMainMotionOps.h"

#include <algorithm>

#include <X11/X.h>

namespace arachno {

GuiMainMotionResult handleMainMotionNotify(const GuiMainMotionContext& context) {
    GuiMainMotionResult result;
    result.consumed = true;

    context.pointerX = context.mx;
    context.pointerY = context.my;

    if (context.resizingSidebar) {
        const int newSidebar = context.windowWidth - context.mx - (context.layout.margin * 2);
        context.sidebarWidthState = std::clamp(newSidebar, 220, std::max(240, context.windowWidth - 420));
        result.needsRedraw = true;
        return result;
    }
    if (context.resizingTopPanel) {
        context.topPanelHeightState = std::clamp(context.my - context.layout.margin + 4, 240, std::max(240, context.windowHeight - 220));
        result.needsRedraw = true;
        return result;
    }
    if (context.draggingPatternRows && (context.stateMask & Button1Mask) != 0) {
        const int delta = (context.patternResizeAnchorY - context.my) / 5;
        const int targetRows = context.patternResizeStartRows + (delta * 4);
        context.resizePatternRows(targetRows);
        result.needsRedraw = true;
        return result;
    }
    if (context.paintingNotes && (context.stateMask & Button1Mask) != 0) {
        int row = 0;
        int track = 0;
        if (context.gridPositionToCell(context.mx, context.my, row, track)
            && (row != context.lastPaintRow || track != context.lastPaintTrack)) {
            context.paintNoteAt(row, track, context.paintNoteMidi);
            context.lastPaintRow = row;
            context.lastPaintTrack = track;
            context.refreshSnapshot();
            result.needsRedraw = true;
        }
        return result;
    }
    if (context.draggingSelection && (context.stateMask & Button1Mask) != 0) {
        int row = 0;
        int track = 0;
        if (context.gridPositionToCell(context.mx, context.my, row, track)) {
            context.applySelectionRange(context.dragAnchorRow, context.dragAnchorTrack, row, track);
            result.needsRedraw = true;
        } else if (context.mx >= context.layout.gridLeft && context.mx < context.layout.gridLeft + context.layout.gridWidth) {
            const int headerBottom = context.layout.gridTop + 26;
            const int bottom = context.layout.gridTop + context.layout.gridHeight;
            int autoScrollDelta = 0;
            if (context.my < headerBottom) {
                autoScrollDelta = -2;
            } else if (context.my >= bottom) {
                autoScrollDelta = 2;
            }
            if (autoScrollDelta != 0) {
                context.lockManualScroll();
                context.viewStartRow = std::max(0, context.viewStartRow + autoScrollDelta);
                context.refreshSnapshot();
                const int sampleY = context.my < headerBottom ? headerBottom : (bottom - 1);
                if (context.gridPositionToCell(context.mx, sampleY, row, track)) {
                    context.applySelectionRange(context.dragAnchorRow, context.dragAnchorTrack, row, track);
                }
                result.needsRedraw = true;
            }
        }
        return result;
    }

    int hovered = -1;
    for (int index = 0; index < static_cast<int>(context.trackHeaderHits.size()); ++index) {
        if (context.trackHeaderHits[static_cast<std::size_t>(index)].selectRect.contains(context.mx, context.my)) {
            hovered = index;
            break;
        }
    }
    if (hovered != context.hoveredTrackHeader) {
        context.hoveredTrackHeader = hovered;
        result.needsRedraw = true;
    }
    return result;
}

} // namespace arachno
