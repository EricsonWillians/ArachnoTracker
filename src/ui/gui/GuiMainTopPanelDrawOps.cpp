#include "ui/gui/GuiMainTopPanelDrawOps.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "AudioRuntime.h"

namespace arachno {

void drawMainTopPanelSection(const GuiMainTopPanelDrawContext& context) {
    auto fitText = [&](const std::string& text, int maxWidth) {
        if (maxWidth <= 10 || context.textWidth(text) <= maxWidth) {
            return text;
        }
        const std::string ellipsis = "...";
        std::string trimmed = text;
        while (!trimmed.empty() && context.textWidth(trimmed + ellipsis) > maxWidth) {
            trimmed.pop_back();
        }
        return trimmed.empty() ? ellipsis : (trimmed + ellipsis);
    };

    auto drawToolbarButton = [&](const UiRect& rect, const std::string& label, bool active) {
        context.drawFilledRect(
            rect.x,
            rect.y,
            rect.width,
            rect.height,
            active ? context.colorButtonActive : context.colorButton);
        context.drawRect(rect.x, rect.y, rect.width, rect.height, context.colorGridLine);
        const int tw = context.textWidth(label);
        const int tx = rect.x + std::max(6, (rect.width - tw) / 2);
        context.drawText(
            tx,
            context.controlTextBaseline(rect.y, rect.height),
            label,
            active ? context.colorButtonLabelActive : context.colorButtonLabel);
    };

    auto synthTierLabel = [](SynthQualityTier tier) {
        switch (tier) {
            case SynthQualityTier::Ultra:
                return "ULTRA";
            case SynthQualityTier::High:
                return "HIGH";
            case SynthQualityTier::Balanced:
                return "BAL";
            case SynthQualityTier::Eco:
                return "ECO";
        }
        return "UNK";
    };

    std::string playbackState = "stopped";
    if (context.playback.state == TransportState::Playing) {
        playbackState = "playing";
    } else if (context.playback.state == TransportState::Paused) {
        playbackState = "paused";
    }

    context.drawFilledRect(
        context.margin,
        context.margin,
        context.windowWidth - (context.margin * 2),
        context.headerHeight,
        context.colorPanel);
    context.drawRect(
        context.margin,
        context.margin,
        context.windowWidth - (context.margin * 2),
        context.headerHeight,
        context.colorGridLine);
    context.drawFilledRect(
        context.margin,
        context.margin + 34,
        context.windowWidth - (context.margin * 2),
        1,
        context.colorGridLine);
    context.drawFilledRect(
        context.margin,
        context.margin + 86,
        context.windowWidth - (context.margin * 2),
        1,
        context.colorGridLine);
    context.drawFilledRect(
        context.margin,
        context.margin + 132,
        context.windowWidth - (context.margin * 2),
        1,
        context.colorGridLine);

    const int toolbarY = context.margin + 6;
    const int toolbarH = 22;
    const int toolbarGap = 6;
    context.drawText(context.margin + 10, toolbarY + 16, "ArachnoTracker", context.colorText);

    struct ToolbarItem {
        std::string label;
        std::string compactLabel;
        std::string actionId;
    };
    std::vector<ToolbarItem> fileItems {
        {"NEW", "N", "project.new"},
        {"LOAD", "L", "project.open"},
        {"SAVE", "S", "project.save"},
        {"TUNE", "TN", "audio.tuning.toggle"},
        {"PATCH", "PT", "synth.window.toggle"},
        {"EXPORT", "X", "export.mixdown"}};
    std::vector<ToolbarItem> transportItems {
        {"PATTERN", "PAT", "playback.play_pattern"},
        {"SONG", "SNG", "playback.play_song"},
        {"PAUSE", "PAU", "playback.pause"},
        {"STOP", "STP", "playback.stop"},
        {"PREVIEW", "PRV", "preview.cursor"}};
    const int groupGap = 10;
    const int rightLimit = context.windowWidth - context.margin - 10;
    const int brandRight = context.margin + 10 + context.textWidth("ArachnoTracker") + 16;
    int leftCursor = brandRight;
    int rightCursor = rightLimit;

    auto itemsWidth =
        [&](const std::vector<ToolbarItem>& items, bool compact, int padding, int minWidth, int gap) {
            int width = 0;
            for (std::size_t index = 0; index < items.size(); ++index) {
                const std::string& label = compact ? items[index].compactLabel : items[index].label;
                width += std::max(minWidth, context.textWidth(label) + padding);
                if (index + 1 < items.size()) {
                    width += gap;
                }
            }
            return width;
        };

    auto drawRightGroup = [&](const std::vector<ToolbarItem>& items,
                              bool compact,
                              int padding,
                              int minWidth,
                              int gap,
                              bool transportGroup) -> bool {
        const int width = itemsWidth(items, compact, padding, minWidth, gap);
        const int startX = rightCursor - width;
        if (startX <= leftCursor + 8) {
            return false;
        }
        int x = startX;
        for (const ToolbarItem& item : items) {
            const std::string& label = compact ? item.compactLabel : item.label;
            const int buttonW = std::max(minWidth, context.textWidth(label) + padding);
            const UiRect rect {x, toolbarY + 1, buttonW, toolbarH};
            const bool active = (item.actionId == "playback.play_pattern"
                    && context.playback.state == TransportState::Playing
                    && context.playback.loop.enabled)
                || ((item.actionId == "playback.play_song" || item.actionId == "playback.play")
                    && context.playback.state == TransportState::Playing
                    && !context.playback.loop.enabled)
                || (item.actionId == "playback.pause" && context.playback.state == TransportState::Paused)
                || (item.actionId == "playback.stop" && context.playback.state == TransportState::Stopped)
                || (item.actionId == "theme.dos" && context.themeMode == GuiThemeMode::Dos)
                || (item.actionId == "theme.high_contrast" && context.themeMode == GuiThemeMode::HighContrast)
                || (item.actionId == "synth.window.toggle" && context.synthWindowVisible);
            drawToolbarButton(rect, label, active);
            if (item.actionId.rfind("theme.", 0) == 0) {
                context.themeButtons.push_back(
                    {rect, item.actionId == "theme.dos" ? GuiThemeMode::Dos : GuiThemeMode::HighContrast});
            } else if (transportGroup) {
                context.transportButtons.push_back({rect, item.actionId});
            } else {
                context.fileButtons.push_back({rect, item.actionId});
            }
            x += buttonW + gap;
        }
        rightCursor = startX - groupGap;
        return true;
    };

    auto drawLeftGroup =
        [&](const std::vector<ToolbarItem>& items, bool compact, int padding, int minWidth, int gap) -> bool {
        const int width = itemsWidth(items, compact, padding, minWidth, gap);
        if (leftCursor + width >= rightCursor - 8) {
            return false;
        }
        int x = leftCursor;
        for (const ToolbarItem& item : items) {
            const std::string& label = compact ? item.compactLabel : item.label;
            const int buttonW = std::max(minWidth, context.textWidth(label) + padding);
            const UiRect rect {x, toolbarY + 1, buttonW, toolbarH};
            const bool active = (item.actionId == "synth.window.toggle" && context.synthWindowVisible)
                || (item.actionId == "audio.tuning.toggle" && context.audioTuningDialogActive);
            drawToolbarButton(rect, label, active);
            context.fileButtons.push_back({rect, item.actionId});
            x += buttonW + gap;
        }
        leftCursor = x + groupGap;
        return true;
    };

    bool compact = false;
    int padding = 14;
    int minWidth = 54;
    int gap = toolbarGap;

    if (!drawRightGroup(transportItems, compact, padding, minWidth, gap, true)) {
        compact = true;
        padding = 10;
        minWidth = 40;
        gap = 4;
        if (!drawRightGroup(transportItems, compact, padding, minWidth, gap, true)) {
            std::vector<ToolbarItem> tinyTransport {
                {"PATTERN", "PAT", "playback.play_pattern"},
                {"SONG", "SNG", "playback.play_song"},
                {"STOP", "STP", "playback.stop"}};
            (void)drawRightGroup(tinyTransport, true, padding, minWidth, gap, true);
        }
    }

    if (!drawLeftGroup(fileItems, compact, padding, minWidth, gap)) {
        std::vector<ToolbarItem> compactFileItems {
            {"NEW", "N", "project.new"},
            {"LOAD", "L", "project.open"},
            {"SAVE", "S", "project.save"},
            {"TUNE", "TN", "audio.tuning.toggle"},
            {"PATCH", "PT", "synth.window.toggle"}};
        if (!drawLeftGroup(compactFileItems, true, 10, 36, 4)) {
            std::vector<ToolbarItem> tinyFileItems {
                {"NEW", "N", "project.new"},
                {"SAVE", "S", "project.save"},
                {"TUNE", "TN", "audio.tuning.toggle"},
                {"PATCH", "PT", "synth.window.toggle"}};
            (void)drawLeftGroup(tinyFileItems, true, 10, 34, 3);
        }
    }

    const int subtitleX = leftCursor;
    const int subtitleWidth = context.textWidth("Tracker Workbench");
    if (subtitleX + subtitleWidth + 10 < rightCursor) {
        context.drawText(subtitleX, toolbarY + 16, "Tracker Workbench", context.colorMutedText);
    }

    int audioPerfReservedLeft = context.windowWidth - context.margin - 8;
    {
        const int perfY = context.margin + 40;
        const int perfH = 20;
        const int perfGap = 3;
        const std::array<AudioPerformanceMode, 5> perfModes {
            AudioPerformanceMode::Auto,
            AudioPerformanceMode::Live,
            AudioPerformanceMode::Balanced,
            AudioPerformanceMode::Heavy,
            AudioPerformanceMode::Custom};
        int totalButtonsWidth = 0;
        std::array<int, 5> buttonWidths {};
        for (std::size_t index = 0; index < perfModes.size(); ++index) {
            const std::string label = audioPerformanceModeLabel(perfModes[index]);
            buttonWidths[index] = std::max(36, context.textWidth(label) + 12);
            totalButtonsWidth += buttonWidths[index];
            if (index + 1 < perfModes.size()) {
                totalButtonsWidth += perfGap;
            }
        }
        const int perfLabelWidth = context.textWidth("AUDIO");
        const int perfTotalWidth = perfLabelWidth + 8 + totalButtonsWidth;
        const int perfStartX = context.windowWidth - context.margin - 8 - perfTotalWidth;
        if (perfStartX > context.margin + 300) {
            std::string telemetryText;
            unsigned long telemetryColor = context.colorMutedText;
            {
                std::ostringstream telemetry;
                const char* outputBackend = context.audioRuntime.usesAlsa()
                    ? "ALSA"
                    : (context.audioRuntime.hasOutput() ? "APLAY" : "OFF");
                telemetry << outputBackend << " " << context.audioRuntime.frameMin() << "-"
                          << context.audioRuntime.frameMax();
                if (context.audioRuntime.performanceMode() == AudioPerformanceMode::Custom) {
                    telemetry << " lvl " << context.audioRuntime.customLevel();
                }
                telemetry << "  DSP " << static_cast<int>(std::lround(std::clamp(context.playback.synth.dspLoadPercent, 0.0, 999.0)))
                          << "%";
                telemetry << "  RISK " << static_cast<int>(std::lround(std::clamp(context.playback.synth.underrunRisk, 0.0, 1.0) * 100.0))
                          << "%";
                telemetry << "  PRE " << static_cast<int>(std::lround(std::clamp(context.playback.synth.preLimiterHeadroomDb, 0.0, 99.0)))
                          << "dB";
                telemetry << "  POST " << static_cast<int>(std::lround(std::clamp(context.playback.synth.postLimiterHeadroomDb, 0.0, 99.0)))
                          << "dB";
                telemetry << "  LIM " << static_cast<int>(std::lround(std::clamp(context.playback.synth.limiterReductionDb, 0.0, 99.0)))
                          << "dB";
                telemetry << "  U " << std::max(0, context.snapshot.audio.underrunCount);
                telemetry << "  " << synthTierLabel(context.playback.synth.qualityTier);
                const auto [queued, capacity] = context.audioRuntime.alsaQueueUsage();
                if (context.audioRuntime.usesAlsa() && capacity > 0) {
                    const int queuePct = static_cast<int>((queued * 100) / capacity);
                    telemetry << " q" << std::clamp(queuePct, 0, 100) << "%";
                    if (queuePct > 85) {
                        telemetryColor = context.colorCursor;
                    } else if (queuePct > 65) {
                        telemetryColor = context.colorText;
                    } else {
                        telemetryColor = context.colorMutedText;
                    }
                }
                if (context.playback.synth.underrunRisk >= 0.85) {
                    telemetryColor = context.colorCursor;
                }
                telemetryText = telemetry.str();
                if (!context.audioRuntime.hasOutput()) {
                    telemetryColor = context.colorMutedText;
                }
            }
            context.drawText(perfStartX, perfY + 14, "AUDIO", context.colorMutedText);
            int x = perfStartX + perfLabelWidth + 8;
            for (std::size_t index = 0; index < perfModes.size(); ++index) {
                const AudioPerformanceMode mode = perfModes[index];
                const UiRect rect {x, perfY, buttonWidths[index], perfH};
                const std::string label = audioPerformanceModeLabel(mode);
                drawToolbarButton(rect, label, context.audioRuntime.performanceMode() == mode);
                context.audioPerformanceButtons.push_back({rect, mode});
                x += buttonWidths[index] + perfGap;
            }
            const int telemetryY = perfY + perfH + 13;
            const int telemetryWidth = perfTotalWidth;
            context.drawText(
                perfStartX,
                telemetryY,
                fitText(telemetryText, telemetryWidth),
                telemetryColor);
            const int meterY = telemetryY + 4;
            const int meterH = 5;
            const int meterGap = 4;
            const int meterW = std::max(24, std::min(56, (telemetryWidth - meterGap * 2) / 3));
            auto drawMeter = [&](int x, double value01, unsigned long fillColor) {
                const double v = std::clamp(value01, 0.0, 1.0);
                const int fillW = std::clamp(static_cast<int>(std::lround(v * meterW)), 0, meterW);
                context.drawRect(x, meterY, meterW, meterH, context.colorGridLine);
                if (fillW > 0) {
                    context.drawFilledRect(x + 1, meterY + 1, std::max(0, fillW - 2), std::max(1, meterH - 2), fillColor);
                }
            };
            const double dspLoad01 = std::clamp(context.playback.synth.dspLoadPercent / 100.0, 0.0, 1.0);
            const double risk01 = std::clamp(context.playback.synth.underrunRisk, 0.0, 1.0);
            const double headroom01 = std::clamp(context.playback.synth.postLimiterHeadroomDb / 12.0, 0.0, 1.0);
            drawMeter(perfStartX, dspLoad01, dspLoad01 >= 0.92 ? context.colorCursor : context.colorText);
            drawMeter(perfStartX + meterW + meterGap, risk01, risk01 >= 0.75 ? context.colorCursor : context.colorText);
            drawMeter(perfStartX + (meterW + meterGap) * 2, headroom01, context.colorPlayhead);
            audioPerfReservedLeft = perfStartX - 10;
        }
    }

    const int hintMaxWidth = std::max(160, audioPerfReservedLeft - (context.margin + 10));
    const std::string hintLine1 =
        "Enter inserts selected note | Ctrl+0..8 octave | Arrows move | Shift+Arrows select | Space pattern play/stop";
    context.drawText(
        context.margin + 10,
        context.margin + 54,
        fitText(hintLine1, hintMaxWidth),
        context.colorMutedText);
    const std::string hintLine2 =
        "F5/F6/F7 transport | Ctrl+Shift+Up/Down velocity | Ctrl+Alt+PgUp/PgDn order | Ctrl+Alt+Ins/Del insert/remove order";
    context.drawText(
        context.margin + 10,
        context.margin + 71,
        fitText(hintLine2, hintMaxWidth),
        context.colorMutedText);

    const int patternCount = static_cast<int>(context.snapshot.editor.patterns.size());
    const int activePattern = patternCount > 0
        ? std::clamp(context.snapshot.editor.status.activePattern, 0, patternCount - 1)
        : 0;
    const std::string activePatternName = (patternCount > 0
            && activePattern < static_cast<int>(context.snapshot.editor.patterns.size()))
        ? context.snapshot.editor.patterns[static_cast<std::size_t>(activePattern)].name
        : "None";
    const int orderCount = static_cast<int>(context.snapshot.editor.order.size());
    const int activeOrderIndex = (context.playback.position.orderIndex >= 0
            && context.playback.position.orderIndex < orderCount)
        ? context.playback.position.orderIndex
        : -1;
    const int orderIndex = orderCount > 0 ? std::clamp(context.selectedOrderIndex, 0, orderCount - 1) : 0;
    const OrderSlotSummary* selectedOrder = (orderCount > 0)
        ? &context.snapshot.editor.order[static_cast<std::size_t>(orderIndex)]
        : nullptr;

    const int railLabelX = context.margin + 10;
    const int railLabelWidth = std::max(context.textWidth("PATTERN"), context.textWidth("ORDER"));
    const int rowLeft = railLabelX + railLabelWidth + 16;
    const int rowRight = context.windowWidth - context.margin - 8;
    const int navW = 22;
    const int navGap = 4;
    const int actionGap = 4;
    const int patternActionW = 56;
    const int orderActionW = 52;
    const int rowH = 16;
    const int rowGap = 6;
    const int patternRowY = context.margin + 94;
    const int orderRowY = patternRowY + rowH + rowGap;
    const int slotY = orderRowY + rowH + rowGap;
    const int patternLabelY = context.controlTextBaseline(patternRowY, rowH);
    const int orderLabelY = context.controlTextBaseline(orderRowY, rowH);

    context.drawText(railLabelX, patternLabelY, "PATTERN", context.colorMutedText);
    const int patternActionTotalW = (patternActionW * 3) + (actionGap * 2);
    const bool showPatternActions =
        (rowRight - rowLeft) >= (navW + navGap + 120 + navGap + navW + navGap + patternActionTotalW);
    context.patternPrevButton = UiRect {rowLeft, patternRowY, navW, rowH};
    if (showPatternActions) {
        const int actionStart = rowRight - patternActionTotalW;
        context.patternNextButton = UiRect {actionStart - navGap - navW, patternRowY, navW, rowH};
        context.patternValueButton = UiRect {
            context.patternPrevButton.x + context.patternPrevButton.width + navGap,
            patternRowY,
            std::max(
                120,
                context.patternNextButton.x
                    - navGap
                    - (context.patternPrevButton.x + context.patternPrevButton.width + navGap)),
            rowH};
        context.patternNewButton = UiRect {actionStart, patternRowY, patternActionW, rowH};
        context.patternCloneButton = UiRect {actionStart + patternActionW + actionGap, patternRowY, patternActionW, rowH};
        context.patternDeleteButton =
            UiRect {actionStart + (patternActionW + actionGap) * 2, patternRowY, patternActionW, rowH};
    } else {
        context.patternNextButton = UiRect {rowRight - navW, patternRowY, navW, rowH};
        context.patternValueButton = UiRect {
            context.patternPrevButton.x + context.patternPrevButton.width + navGap,
            patternRowY,
            std::max(
                120,
                context.patternNextButton.x
                    - navGap
                    - (context.patternPrevButton.x + context.patternPrevButton.width + navGap)),
            rowH};
    }
    context.drawButton(context.patternPrevButton, "<", false);
    context.drawButton(context.patternNextButton, ">", false);
    if (showPatternActions) {
        context.drawButton(context.patternNewButton, "NEW", false);
        context.drawButton(context.patternCloneButton, "CLONE", false);
        context.drawButton(context.patternDeleteButton, "DEL", false);
    }
    context.drawFilledRect(
        context.patternValueButton.x,
        context.patternValueButton.y,
        context.patternValueButton.width,
        context.patternValueButton.height,
        context.colorButton);
    context.drawRect(
        context.patternValueButton.x,
        context.patternValueButton.y,
        context.patternValueButton.width,
        context.patternValueButton.height,
        context.colorGridLine);
    std::ostringstream patternLabel;
    patternLabel << "P " << (activePattern + 1) << "/" << std::max(1, patternCount) << "  " << activePatternName;
    context.drawText(
        context.patternValueButton.x + 6,
        context.controlTextBaseline(context.patternValueButton.y, context.patternValueButton.height),
        fitText(patternLabel.str(), context.patternValueButton.width - 10),
        context.colorText);

    context.drawText(railLabelX, orderLabelY, "ORDER", context.colorMutedText);
    const int orderActionTotalW = (orderActionW * 3) + (actionGap * 2);
    const bool showOrderActions =
        (rowRight - rowLeft) >= (navW + navGap + 120 + navGap + navW + navGap + orderActionTotalW);
    context.orderPrevButton = UiRect {rowLeft, orderRowY, navW, rowH};
    if (showOrderActions) {
        const int actionStart = rowRight - orderActionTotalW;
        context.orderNextButton = UiRect {actionStart - navGap - navW, orderRowY, navW, rowH};
        context.orderValueButton = UiRect {
            context.orderPrevButton.x + context.orderPrevButton.width + navGap,
            orderRowY,
            std::max(
                120,
                context.orderNextButton.x - navGap
                    - (context.orderPrevButton.x + context.orderPrevButton.width + navGap)),
            rowH};
        context.orderInsertButton = UiRect {actionStart, orderRowY, orderActionW, rowH};
        context.orderAppendButton = UiRect {actionStart + orderActionW + actionGap, orderRowY, orderActionW, rowH};
        context.orderDeleteButton =
            UiRect {actionStart + (orderActionW + actionGap) * 2, orderRowY, orderActionW, rowH};
    } else {
        context.orderNextButton = UiRect {rowRight - navW, orderRowY, navW, rowH};
        context.orderValueButton = UiRect {
            context.orderPrevButton.x + context.orderPrevButton.width + navGap,
            orderRowY,
            std::max(
                120,
                context.orderNextButton.x
                    - navGap
                    - (context.orderPrevButton.x + context.orderPrevButton.width + navGap)),
            rowH};
    }
    context.drawButton(context.orderPrevButton, "<", false);
    context.drawButton(context.orderNextButton, ">", false);
    if (showOrderActions) {
        context.drawButton(context.orderInsertButton, "INS", false);
        context.drawButton(context.orderAppendButton, "APP", false);
        context.drawButton(context.orderDeleteButton, "DEL", false);
    }
    context.drawFilledRect(
        context.orderValueButton.x,
        context.orderValueButton.y,
        context.orderValueButton.width,
        context.orderValueButton.height,
        context.colorButton);
    context.drawRect(
        context.orderValueButton.x,
        context.orderValueButton.y,
        context.orderValueButton.width,
        context.orderValueButton.height,
        context.colorGridLine);
    std::ostringstream orderLabel;
    if (selectedOrder != nullptr) {
        orderLabel << "O " << (orderIndex + 1) << "/" << orderCount << " ";
        if (selectedOrder->pattern >= 0) {
            orderLabel << "P" << (selectedOrder->pattern + 1);
        } else {
            orderLabel << "P--";
        }
        orderLabel << " " << selectedOrder->patternName;
    } else {
        orderLabel << "No order entries";
    }
    context.drawText(
        context.orderValueButton.x + 6,
        context.controlTextBaseline(context.orderValueButton.y, context.orderValueButton.height),
        fitText(orderLabel.str(), context.orderValueButton.width - 10),
        selectedOrder != nullptr && selectedOrder->missing ? context.colorMutedText : context.colorText);

    const int slotLeft = rowLeft;
    const int slotGap = 4;
    const int slotWidth = std::max(
        62,
        std::min(108, (context.windowWidth - slotLeft - context.margin - 8 - (slotGap * 3)) / 4));
    const int available = std::max(1, (context.windowWidth - slotLeft - context.margin - 8) / (slotWidth + slotGap));
    int firstSlot = 0;
    if (orderCount > available) {
        const int centerIndex = std::clamp(orderIndex - (available / 2), 0, std::max(0, orderCount - available));
        firstSlot = centerIndex;
    }
    const int drawCount = std::min(orderCount, available);
    for (int visual = 0; visual < drawCount; ++visual) {
        const int index = firstSlot + visual;
        const OrderSlotSummary& slot = context.snapshot.editor.order[static_cast<std::size_t>(index)];
        const UiRect rect {slotLeft + (visual * (slotWidth + slotGap)), slotY, slotWidth, 14};
        const bool isSelected = index == orderIndex;
        const bool isPlaying = index == activeOrderIndex;
        const unsigned long fill =
            isSelected ? context.colorButtonActive : (isPlaying ? context.colorPlayhead : context.colorButton);
        context.drawFilledRect(rect.x, rect.y, rect.width, rect.height, fill);
        context.drawRect(rect.x, rect.y, rect.width, rect.height, context.colorGridLine);
        std::ostringstream label;
        if (slot.pattern >= 0) {
            label << index << ":P" << (slot.pattern + 1);
        } else {
            label << index << ":P--";
        }
        context.drawText(
            rect.x + 4,
            rect.y + 11,
            fitText(label.str(), rect.width - 8),
            slot.missing ? context.colorMutedText : (isSelected ? context.colorActiveTagText : context.colorText));
        context.orderSlotHits.push_back({rect, index, slot.pattern, slot.startRow, !slot.missing && slot.pattern >= 0});
    }

    context.drawFilledRect(
        context.margin,
        context.margin + context.headerHeight + 8,
        context.windowWidth - (context.margin * 2),
        context.statusHeight,
        context.colorPanel);
    context.drawRect(
        context.margin,
        context.margin + context.headerHeight + 8,
        context.windowWidth - (context.margin * 2),
        context.statusHeight,
        context.colorGridLine);
    context.drawFilledRect(context.verticalSplitterX, context.gridTop, 5, context.gridHeight, context.colorGridLine);
    context.drawFilledRect(
        context.margin,
        context.horizontalSplitterY,
        context.windowWidth - (context.margin * 2),
        3,
        context.colorGridLine);

    int y = context.margin + context.headerHeight + 28;
    auto printLine = [&](const std::string& line) {
        context.drawText(context.margin + 10, y, line, context.colorText);
        y += context.lineHeight;
    };

    std::ostringstream status;
    status << "Project: " << (context.snapshot.hasProjectPath ? context.snapshot.projectPath : "<untitled>")
           << "  Dirty: " << (context.snapshot.dirty ? "yes" : "no")
           << "  Playback: " << playbackState
           << "  Row: " << context.snapshot.playback.position.patternRow
           << "  Pattern: " << context.snapshot.playback.position.pattern;
    printLine(status.str());

    std::ostringstream cursor;
    cursor << "Cursor: pattern " << context.snapshot.editor.status.activePattern
           << " row " << context.snapshot.editor.status.cursorRow
           << " track " << context.snapshot.editor.status.cursorTrack
           << "  Selection: " << context.snapshot.editor.status.selectionRows << "x"
           << context.snapshot.editor.status.selectionTracks;
    printLine(cursor.str());

    std::ostringstream counts;
    counts << "Patterns: " << context.snapshot.editor.patterns.size()
           << "  Tracks: " << context.snapshot.editor.tracks.size()
           << "  Instruments: " << context.snapshot.editor.instruments.size()
           << "  Audio: " << (context.snapshot.audio.active ? "active" : "inactive")
           << " (" << audioBackendName(context.snapshot.audio.backend) << ")";
    printLine(counts.str());

    std::ostringstream trackerState;
    trackerState << "Armed instrument: " << context.armedInstrument
                 << "  Octave: " << context.armedOctave
                 << "  Velocity: " << static_cast<int>(context.defaultVelocity * 100.0f)
                 << "  Step advance: " << (context.stepAdvance ? "on" : "off")
                 << "  Follow playback: " << (context.followPlayback ? "on" : "off")
                 << "  Audio mode: " << audioPerformanceModeLabel(context.audioRuntime.performanceMode());
    if (context.audioRuntime.performanceMode() == AudioPerformanceMode::Custom) {
        trackerState << " (" << context.audioRuntime.customLevel() << ")";
    }
    printLine(trackerState.str());
}

} // namespace arachno
