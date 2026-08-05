#include "ui/gui/GuiMainGridDrawOps.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>

namespace arachno {

void drawMainGridSection(const GuiMainGridDrawContext& context) {
    context.drawFilledRect(context.gridLeft, context.gridTop, context.gridWidth, context.gridHeight, context.colorPanel);
    context.drawRect(context.gridLeft, context.gridTop, context.gridWidth, context.gridHeight, context.colorGridLine);

    const PatternGrid& grid = context.snapshot.editor.activeGrid;
    const int rowNumberWidth = 56;
    const int totalTrackCols = std::max(1, grid.trackCount);
    const int trackWidth = 84;
    const int visibleTrackCols = std::max(1, (context.gridWidth - rowNumberWidth - 6) / trackWidth);
    const int maxTrackStart = std::max(0, totalTrackCols - visibleTrackCols);
    context.gridTrackStartRef = std::clamp(context.gridTrackStartRef, 0, maxTrackStart);
    const int gridHeaderY = context.gridTop + 22;
    const int rowHeight = 18;
    const int maxRows = std::max(1, (context.gridHeight - 36) / rowHeight);
    const int visibleRows = std::min(grid.rowCount, maxRows);
    context.layout.rowNumberWidth = rowNumberWidth;
    context.layout.trackCols = visibleTrackCols;
    context.layout.trackWidth = trackWidth;
    context.layout.rowHeight = rowHeight;
    context.layout.visibleRows = visibleRows;
    context.requestedRowCount = std::max(1, maxRows);

    context.drawFilledRect(
        context.gridLeft + 1,
        context.gridTop + 1,
        context.gridWidth - 2,
        24,
        context.colorGridHeader);
    context.drawText(context.gridLeft + 8, gridHeaderY, "ROW", context.colorText);
    if (totalTrackCols > visibleTrackCols) {
        const UiRect leftRect {context.gridLeft + 4, context.gridTop + 4, 12, 16};
        const UiRect rightRect {context.gridLeft + rowNumberWidth - 16, context.gridTop + 4, 12, 16};
        context.drawButton(leftRect, "<", false);
        context.drawButton(rightRect, ">", false);
        context.gridTrackPrevButton = leftRect;
        context.gridTrackNextButton = rightRect;
        std::ostringstream range;
        range << (context.gridTrackStartRef + 1) << "-"
              << std::min(totalTrackCols, context.gridTrackStartRef + visibleTrackCols)
              << "/" << totalTrackCols;
        context.drawText(
            context.gridLeft + 18,
            gridHeaderY,
            context.fitText(range.str(), rowNumberWidth - 38),
            context.colorMutedText);
    }

    for (int col = 0; col < visibleTrackCols; ++col) {
        const int track = context.gridTrackStartRef + col;
        if (track >= totalTrackCols) {
            break;
        }
        const int x = context.gridLeft + rowNumberWidth + 4 + (col * trackWidth);
        const TrackStripSummary* trackSummary = track < static_cast<int>(context.snapshot.editor.tracks.size())
            ? &context.snapshot.editor.tracks[static_cast<std::size_t>(track)]
            : nullptr;
        const std::string trackName = track < static_cast<int>(grid.trackNames.size())
            ? grid.trackNames[static_cast<std::size_t>(track)]
            : ("T" + std::to_string(track));
        const int nameWidth = std::max(12, trackWidth - 34);
        const std::string shownName = context.fitText(trackName, nameWidth);
        const bool nameTruncated = shownName != trackName;
        context.drawText(x + 4, gridHeaderY, shownName, context.colorText);
        const UiRect muteRect {x + trackWidth - 28, context.gridTop + 5, 11, 11};
        const UiRect soloRect {x + trackWidth - 14, context.gridTop + 5, 11, 11};
        context.drawFilledRect(
            muteRect.x,
            muteRect.y,
            muteRect.width,
            muteRect.height,
            (trackSummary != nullptr && trackSummary->muted) ? context.colorButtonActive : context.colorButton);
        context.drawFilledRect(
            soloRect.x,
            soloRect.y,
            soloRect.width,
            soloRect.height,
            (trackSummary != nullptr && trackSummary->solo) ? context.colorButtonActive : context.colorButton);
        context.drawRect(muteRect.x, muteRect.y, muteRect.width, muteRect.height, context.colorGridLine);
        context.drawRect(soloRect.x, soloRect.y, soloRect.width, soloRect.height, context.colorGridLine);
        context.drawText(
            muteRect.x + 2,
            muteRect.y + 9,
            "M",
            (trackSummary != nullptr && trackSummary->muted) ? context.colorActiveTagText : context.colorText);
        context.drawText(
            soloRect.x + 3,
            soloRect.y + 9,
            "S",
            (trackSummary != nullptr && trackSummary->solo) ? context.colorActiveTagText : context.colorText);
        TrackHeaderHit hit;
        hit.selectRect = UiRect {x, context.gridTop + 1, trackWidth, 24};
        hit.muteRect = muteRect;
        hit.soloRect = soloRect;
        hit.track = track;
        hit.trackName = trackName;
        hit.nameTruncated = nameTruncated;
        context.trackHeaderHits.push_back(hit);
        context.drawRect(x, context.gridTop + 1, trackWidth, context.gridHeight - 2, context.colorGridLine);
    }

    // Sustain bars: a slim marker in the note column for every row a note rings
    // through (gate > 1 row), stopping at the next note or note-off (===).
    for (int rowOffset = 0; rowOffset < visibleRows; ++rowOffset) {
        for (int col = 0; col < visibleTrackCols; ++col) {
            const int track = context.gridTrackStartRef + col;
            if (track >= totalTrackCols) {
                break;
            }
            const int cellIndex = (rowOffset * totalTrackCols) + track;
            if (cellIndex < 0 || cellIndex >= static_cast<int>(grid.cells.size())) {
                continue;
            }
            const PatternGridCell& cell = grid.cells[static_cast<std::size_t>(cellIndex)];
            if (!cell.hasNote || cell.gateRows <= 1.0) {
                continue;
            }
            const int gateSpan = std::max(1, static_cast<int>(std::ceil(cell.gateRows)) - 1);
            int rowsToCover = std::min(gateSpan, visibleRows - 1 - rowOffset);
            for (int look = rowOffset + 1; look <= rowOffset + rowsToCover; ++look) {
                const int lookIndex = (look * totalTrackCols) + track;
                if (lookIndex < 0 || lookIndex >= static_cast<int>(grid.cells.size())) {
                    break;
                }
                const PatternGridCell& below = grid.cells[static_cast<std::size_t>(lookIndex)];
                if (below.hasNote || below.noteOff) {
                    rowsToCover = look - rowOffset - 1;
                    break;
                }
            }
            if (rowsToCover <= 0) {
                continue;
            }
            const int x = context.gridLeft + rowNumberWidth + 4 + (col * trackWidth);
            const int yStart = context.gridTop + 26 + (rowOffset * rowHeight) + rowHeight - 4;
            const int yEnd = context.gridTop + 26 + ((rowOffset + rowsToCover) * rowHeight) + rowHeight - 4;
            context.drawFilledRect(x + 5, yStart, 2, yEnd - yStart, context.colorMutedText);
        }
    }

    auto formatCell = [](const PatternGridCell& cell) {
        if (!cell.hasNote) {
            if (cell.noteOff) {
                return std::string("=== .. ..");
            }
            return std::string("... .. ..");
        }
        char buffer[32];
        const int velocity = static_cast<int>(cell.velocity * 100.0f);
        std::snprintf(buffer, sizeof(buffer), "%-3s %02d %02d", cell.noteName.c_str(), cell.instrument, velocity);
        return std::string(buffer);
    };

    const bool playbackActive = context.playback.state == TransportState::Playing
        || context.playback.state == TransportState::Paused;
    const bool playingHere = playbackActive
        && context.playback.position.validPattern
        && context.playback.position.pattern == context.snapshot.editor.status.activePattern;
    // Fractional playhead position within the current row (for a smooth scan line).
    double playheadRowFraction = 0.0;
    if (playingHere) {
        int baseRows = 0;
        const std::vector<OrderSlotSummary>& order = context.snapshot.editor.order;
        const int orderIndex = context.playback.position.orderIndex;
        for (int slot = 0; slot < orderIndex && slot < static_cast<int>(order.size()); ++slot) {
            baseRows += order[static_cast<std::size_t>(slot)].rowCount;
        }
        playheadRowFraction = std::clamp(
            context.playback.position.absoluteRow
                - static_cast<double>(baseRows + context.playback.position.patternRow),
            0.0,
            1.0);
    }
    for (int rowOffset = 0; rowOffset < visibleRows; ++rowOffset) {
        const int yTop = context.gridTop + 26 + (rowOffset * rowHeight);
        const int textY = yTop + 13;
        const int absoluteRow = grid.startRow + rowOffset;
        bool rowOnPlayhead = false;
        if (playingHere && absoluteRow == context.playback.position.patternRow) {
            context.drawFilledRect(
                context.gridLeft + 1,
                yTop + 1,
                context.gridWidth - 2,
                rowHeight - 2,
                context.colorPlayhead);
            // Scan line at the exact fractional playhead position inside the row.
            const int scanY = yTop + 1 + static_cast<int>(playheadRowFraction * (rowHeight - 3));
            context.drawFilledRect(
                context.gridLeft + 1,
                scanY,
                context.gridWidth - 2,
                2,
                context.colorPlayheadText);
            rowOnPlayhead = true;
        }
        char rowBuf[16];
        std::snprintf(rowBuf, sizeof(rowBuf), "%03d", absoluteRow);
        context.drawText(
            context.gridLeft + 8,
            textY,
            rowBuf,
            rowOnPlayhead ? context.colorPlayheadText : context.colorMutedText);

        for (int col = 0; col < visibleTrackCols; ++col) {
            const int track = context.gridTrackStartRef + col;
            if (track >= totalTrackCols) {
                break;
            }
            const int cellIndex = (rowOffset * totalTrackCols) + track;
            if (cellIndex < 0 || cellIndex >= static_cast<int>(grid.cells.size())) {
                continue;
            }
            const PatternGridCell& cell = grid.cells[static_cast<std::size_t>(cellIndex)];
            const int x = context.gridLeft + rowNumberWidth + 4 + (col * trackWidth);
            if (cell.selected) {
                context.drawFilledRect(x + 1, yTop + 1, trackWidth - 2, rowHeight - 2, context.colorSelection);
            }
            if (cell.cursor) {
                context.drawFilledRect(x + 1, yTop + 1, trackWidth - 2, rowHeight - 2, context.colorCursor);
            }
            const unsigned long cellTextColor = cell.cursor
                ? context.colorCursorText
                : (cell.selected ? context.colorSelectionText : (rowOnPlayhead ? context.colorPlayheadText : context.colorText));
            context.drawText(x + 4, textY, formatCell(cell), cellTextColor);
            if (cell.cursor) {
                context.drawRect(x + 1, yTop + 1, trackWidth - 2, rowHeight - 2, context.colorCursorText);
            }
        }
    }

    if (context.hoveredTrackHeader >= 0
        && context.hoveredTrackHeader < static_cast<int>(context.trackHeaderHits.size())) {
        const TrackHeaderHit& hover = context.trackHeaderHits[static_cast<std::size_t>(context.hoveredTrackHeader)];
        if (hover.nameTruncated && !hover.trackName.empty()) {
            const int tooltipPad = 6;
            const int tooltipW = std::max(80, context.textWidth(hover.trackName) + (tooltipPad * 2));
            const int tooltipH = 20;
            int tx = context.pointerX + 14;
            int ty = context.pointerY + 14;
            tx = std::clamp(tx, context.margin + 2, context.windowWidth - tooltipW - context.margin - 2);
            ty = std::clamp(ty, context.margin + 2, context.windowHeight - tooltipH - context.margin - 2);
            context.drawFilledRect(tx, ty, tooltipW, tooltipH, context.colorPanel);
            context.drawRect(tx, ty, tooltipW, tooltipH, context.colorGridLine);
            context.drawText(tx + tooltipPad, ty + 14, hover.trackName, context.colorText);
        }
    }
}

} // namespace arachno
