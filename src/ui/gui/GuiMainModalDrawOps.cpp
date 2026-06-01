#include "ui/gui/GuiMainModalDrawOps.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace arachno {

void drawMainModalOverlays(const GuiMainModalDrawContext& context) {
    auto drawModalPanel = [&](int width, int height, int& x, int& y) {
        x = std::max(16, (context.windowWidth - width) / 2);
        y = std::max(16, (context.windowHeight - height) / 2);
        context.drawFilledRect(x, y, width, height, context.colorPanel);
        context.drawRect(x, y, width, height, context.colorGridLine);
    };

    if (context.unsavedPrompt.active) {
        const int modalW = std::min(context.windowWidth - 40, 560);
        const int modalH = 170;
        int modalX = 0;
        int modalY = 0;
        drawModalPanel(modalW, modalH, modalX, modalY);
        context.drawText(
            modalX + 12,
            modalY + 24,
            context.unsavedPrompt.title.empty() ? "Unsaved changes" : context.unsavedPrompt.title,
            context.colorText);
        context.drawText(
            modalX + 12,
            modalY + 46,
            context.unsavedPrompt.detail.empty() ? "Save changes before continuing?" : context.unsavedPrompt.detail,
            context.colorMutedText);
        const UiRect saveRect {modalX + 12, modalY + modalH - 34, 86, 22};
        const UiRect discardRect {modalX + 106, modalY + modalH - 34, 102, 22};
        const UiRect cancelRect {modalX + 216, modalY + modalH - 34, 86, 22};
        context.drawButton(saveRect, "SAVE", false);
        context.drawButton(discardRect, "DISCARD", false);
        context.drawButton(cancelRect, "CANCEL", false);
        context.unsavedPromptChoices.push_back({saveRect, UnsavedChangesChoice::Save});
        context.unsavedPromptChoices.push_back({discardRect, UnsavedChangesChoice::Discard});
        context.unsavedPromptChoices.push_back({cancelRect, UnsavedChangesChoice::Cancel});
        return;
    }

    if (context.instrumentBrowserActive) {
        const int modalW = std::min(context.windowWidth - 40, 760);
        const int modalH = std::min(context.windowHeight - 40, 520);
        int modalX = 0;
        int modalY = 0;
        drawModalPanel(modalW, modalH, modalX, modalY);
        context.drawText(modalX + 12, modalY + 24, "Instrument Browser", context.colorText);
        context.drawText(
            modalX + 12,
            modalY + 44,
            "Type to filter by name/index | Enter choose | Up/Down navigate | Ctrl+Up/Down cycle | Esc cancel",
            context.colorMutedText);

        const UiRect queryRect {modalX + 12, modalY + 54, modalW - 24, 22};
        context.drawFilledRect(
            queryRect.x,
            queryRect.y,
            queryRect.width,
            queryRect.height,
            context.colorBackground);
        context.drawRect(queryRect.x, queryRect.y, queryRect.width, queryRect.height, context.colorGridLine);
        context.drawText(
            queryRect.x + 6,
            queryRect.y + 15,
            context.instrumentBrowserQuery.empty() ? "<all>" : context.instrumentBrowserQuery,
            context.colorText);

        context.instrumentBrowserListRect = UiRect {modalX + 12, modalY + 84, modalW - 24, modalH - 142};
        context.drawFilledRect(
            context.instrumentBrowserListRect.x,
            context.instrumentBrowserListRect.y,
            context.instrumentBrowserListRect.width,
            context.instrumentBrowserListRect.height,
            context.colorBackground);
        context.drawRect(
            context.instrumentBrowserListRect.x,
            context.instrumentBrowserListRect.y,
            context.instrumentBrowserListRect.width,
            context.instrumentBrowserListRect.height,
            context.colorGridLine);

        const std::vector<int> filtered = context.filteredInstrumentIndices(context.snapshot);
        const int rowHeight = 18;
        const int visibleRows = std::max(1, (context.instrumentBrowserListRect.height - 4) / rowHeight);
        const int maxScroll = std::max(0, static_cast<int>(filtered.size()) - visibleRows);
        context.instrumentBrowserSelected = std::clamp(
            context.instrumentBrowserSelected,
            0,
            std::max(0, static_cast<int>(filtered.size()) - 1));
        context.instrumentBrowserScroll = std::clamp(context.instrumentBrowserScroll, 0, maxScroll);
        if (context.instrumentBrowserSelected < context.instrumentBrowserScroll) {
            context.instrumentBrowserScroll = context.instrumentBrowserSelected;
        } else if (context.instrumentBrowserSelected >= context.instrumentBrowserScroll + visibleRows) {
            context.instrumentBrowserScroll = std::max(0, context.instrumentBrowserSelected - visibleRows + 1);
        }

        if (filtered.empty()) {
            context.drawText(
                context.instrumentBrowserListRect.x + 8,
                context.instrumentBrowserListRect.y + 18,
                "No instruments match this filter.",
                context.colorMutedText);
        } else {
            for (int row = 0; row < visibleRows; ++row) {
                const int filteredRow = context.instrumentBrowserScroll + row;
                if (filteredRow >= static_cast<int>(filtered.size())) {
                    break;
                }
                const int instrumentIndex = filtered[static_cast<std::size_t>(filteredRow)];
                if (instrumentIndex < 0
                    || instrumentIndex >= static_cast<int>(context.snapshot.editor.instruments.size())) {
                    continue;
                }
                const InstrumentSummary& instrument =
                    context.snapshot.editor.instruments[static_cast<std::size_t>(instrumentIndex)];
                const UiRect rowRect {
                    context.instrumentBrowserListRect.x + 2,
                    context.instrumentBrowserListRect.y + 2 + (row * rowHeight),
                    context.instrumentBrowserListRect.width - 4,
                    rowHeight};
                const bool selected = filteredRow == context.instrumentBrowserSelected;
                if (selected) {
                    context.drawFilledRect(
                        rowRect.x,
                        rowRect.y,
                        rowRect.width,
                        rowRect.height,
                        context.colorSelection);
                }
                std::ostringstream line;
                line << (instrumentIndex < 10 ? "0" : "") << instrumentIndex
                     << "  " << instrument.name
                     << "  (" << instrument.noteUseCount << ")";
                context.drawText(
                    rowRect.x + 6,
                    rowRect.y + 13,
                    context.fitText(line.str(), rowRect.width - 12),
                    selected ? context.colorSelectionText : context.colorText);
                context.instrumentBrowserHitTargets.push_back({rowRect, filteredRow});
            }
        }

        context.instrumentBrowserAcceptButton = UiRect {modalX + 12, modalY + modalH - 34, 86, 22};
        context.instrumentBrowserCancelButton = UiRect {modalX + 106, modalY + modalH - 34, 86, 22};
        context.drawButton(context.instrumentBrowserAcceptButton, "SELECT", false);
        context.drawButton(context.instrumentBrowserCancelButton, "CANCEL", false);
        return;
    }

    if (context.audioTuningDialogActive) {
        const int modalW = std::min(context.windowWidth - 40, 760);
        const int modalH = 244;
        int modalX = 0;
        int modalY = 0;
        drawModalPanel(modalW, modalH, modalX, modalY);
        context.audioTuningDialogRect = UiRect {modalX, modalY, modalW, modalH};
        context.drawText(modalX + 12, modalY + 24, "Audio Performance Tuning", context.colorText);
        context.drawText(
            modalX + 12,
            modalY + 44,
            "Wheel or +/- controls to stretch from ultra-low latency to ultra-heavy buffering.",
            context.colorMutedText);

        const int modeY = modalY + 56;
        const int modeH = 20;
        const int modeGap = 4;
        const std::array<AudioPerformanceMode, 5> modes {
            AudioPerformanceMode::Auto,
            AudioPerformanceMode::Live,
            AudioPerformanceMode::Balanced,
            AudioPerformanceMode::Heavy,
            AudioPerformanceMode::Custom};
        int modeX = modalX + 12;
        for (AudioPerformanceMode mode : modes) {
            const std::string label = audioPerformanceModeLabel(mode);
            const int width = std::max(62, context.textWidth(label) + 14);
            const UiRect rect {modeX, modeY, width, modeH};
            context.drawButton(rect, label, context.audioPerformanceMode == mode);
            AudioTuningDialogHit hit;
            hit.rect = rect;
            hit.role = "mode";
            hit.mode = mode;
            context.audioTuningDialogHits.push_back(hit);
            modeX += width + modeGap;
        }

        const UiRect valueRect {modalX + 12, modalY + 84, modalW - 24, 40};
        context.drawFilledRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, context.colorBackground);
        context.drawRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, context.colorGridLine);
        std::ostringstream valueLine;
        valueLine << "Custom level " << context.audioCustomLevel
                  << "  |  Frame window " << context.audioFrameMin << "-" << context.audioFrameMax;
        context.drawText(
            valueRect.x + 8,
            valueRect.y + 17,
            context.fitText(valueLine.str(), valueRect.width - 12),
            context.colorText);
        context.drawText(
            valueRect.x + 8,
            valueRect.y + 33,
            "Wheel Up = lower latency, Wheel Down = heavier buffering",
            context.colorMutedText);

        const int controlsY = modalY + 134;
        const int controlH = 22;
        const int controlGap = 6;
        const std::array<int, 8> deltas {-250, -50, -10, -1, 1, 10, 50, 250};
        const std::array<const char*, 8> deltaLabels {"-250", "-50", "-10", "-1", "+1", "+10", "+50", "+250"};
        int controlX = modalX + 12;
        for (std::size_t index = 0; index < deltas.size(); ++index) {
            const UiRect rect {controlX, controlsY, 58, controlH};
            context.drawButton(rect, deltaLabels[index], false);
            AudioTuningDialogHit hit;
            hit.rect = rect;
            hit.role = "delta";
            hit.delta = deltas[index];
            context.audioTuningDialogHits.push_back(hit);
            controlX += rect.width + controlGap;
        }
        const UiRect resetRect {controlX + 8, controlsY, 72, controlH};
        context.drawButton(resetRect, "RESET", false);
        {
            AudioTuningDialogHit hit;
            hit.rect = resetRect;
            hit.role = "reset";
            context.audioTuningDialogHits.push_back(hit);
        }

        const UiRect closeRect {modalX + modalW - 92, modalY + modalH - 32, 80, 20};
        context.drawButton(closeRect, "CLOSE", false);
        {
            AudioTuningDialogHit hit;
            hit.rect = closeRect;
            hit.role = "close";
            context.audioTuningDialogHits.push_back(hit);
        }

        context.drawText(
            modalX + 12,
            modalY + modalH - 12,
            "Ctrl+F7 opens this panel | F7 cycles preset modes",
            context.colorMutedText);
        return;
    }

    if (context.inlinePrompt.active
        && !(context.synthWindowVisible && context.isSynthInlinePromptKind(context.inlinePrompt.kind))) {
        const bool browserMode = context.inlinePromptUsesFileBrowser(context.inlinePrompt.kind);
        const int modalW = browserMode ? std::min(context.windowWidth - 40, 920) : std::min(context.windowWidth - 40, 700);
        const int modalH = browserMode ? std::min(context.windowHeight - 40, 560) : 140;
        int modalX = 0;
        int modalY = 0;
        drawModalPanel(modalW, modalH, modalX, modalY);
        context.drawText(modalX + 12, modalY + 24, context.inlinePrompt.title, context.colorText);
        context.drawText(modalX + 12, modalY + 44, context.inlinePrompt.hint, context.colorMutedText);
        if (browserMode) {
            if (context.fileBrowserEntries.empty()) {
                context.refreshFileBrowserEntries();
            }
            const UiRect homeRect {modalX + 12, modalY + 54, 64, 22};
            const UiRect upRect {modalX + 82, modalY + 54, 48, 22};
            const UiRect refreshRect {modalX + 136, modalY + 54, 88, 22};
            const UiRect dirRect {modalX + 230, modalY + 54, modalW - 242, 22};
            context.drawButton(homeRect, "HOME", false);
            context.drawButton(upRect, "UP", false);
            context.drawButton(refreshRect, "REFRESH", false);
            context.drawFilledRect(dirRect.x, dirRect.y, dirRect.width, dirRect.height, context.colorBackground);
            context.drawRect(dirRect.x, dirRect.y, dirRect.width, dirRect.height, context.colorGridLine);
            context.drawText(
                dirRect.x + 6,
                dirRect.y + 15,
                context.fitText(context.fileBrowserDirectory.string(), dirRect.width - 10),
                context.colorText);
            context.fileBrowserHits.push_back({homeRect, "nav_home", -1});
            context.fileBrowserHits.push_back({upRect, "nav_up", -1});
            context.fileBrowserHits.push_back({refreshRect, "nav_refresh", -1});

            context.fileBrowserListRect = UiRect {modalX + 12, modalY + 82, modalW - 24, modalH - 156};
            context.drawFilledRect(
                context.fileBrowserListRect.x,
                context.fileBrowserListRect.y,
                context.fileBrowserListRect.width,
                context.fileBrowserListRect.height,
                context.colorBackground);
            context.drawRect(
                context.fileBrowserListRect.x,
                context.fileBrowserListRect.y,
                context.fileBrowserListRect.width,
                context.fileBrowserListRect.height,
                context.colorGridLine);
            const int rowHeight = 18;
            const int visibleRows = std::max(1, (context.fileBrowserListRect.height - 4) / rowHeight);
            const int maxScroll = std::max(0, static_cast<int>(context.fileBrowserEntries.size()) - visibleRows);
            context.fileBrowserScroll = std::clamp(context.fileBrowserScroll, 0, maxScroll);
            for (int row = 0; row < visibleRows; ++row) {
                const int index = context.fileBrowserScroll + row;
                if (index >= static_cast<int>(context.fileBrowserEntries.size())) {
                    break;
                }
                const FileBrowserEntry& entry = context.fileBrowserEntries[static_cast<std::size_t>(index)];
                const UiRect rowRect {
                    context.fileBrowserListRect.x + 2,
                    context.fileBrowserListRect.y + 2 + (row * rowHeight),
                    context.fileBrowserListRect.width - 4,
                    rowHeight};
                const bool selected = index == context.fileBrowserSelected;
                if (selected) {
                    context.drawFilledRect(
                        rowRect.x,
                        rowRect.y,
                        rowRect.width,
                        rowRect.height,
                        context.colorSelection);
                }
                context.drawText(
                    rowRect.x + 6,
                    rowRect.y + 13,
                    std::string(entry.directory ? "[DIR] " : "      ") + entry.name,
                    selected ? context.colorSelectionText : context.colorText);
                context.fileBrowserHits.push_back({rowRect, "entry", index});
            }
            if (static_cast<int>(context.fileBrowserEntries.size()) > visibleRows) {
                const int scrollTrackX = context.fileBrowserListRect.x + context.fileBrowserListRect.width - 8;
                const int scrollTrackY = context.fileBrowserListRect.y + 2;
                const int scrollTrackH = context.fileBrowserListRect.height - 4;
                context.drawFilledRect(scrollTrackX, scrollTrackY, 4, scrollTrackH, context.colorGridLine);
                const double visibleRatio = static_cast<double>(visibleRows)
                    / static_cast<double>(std::max(1, static_cast<int>(context.fileBrowserEntries.size())));
                const int thumbH = std::max(
                    16,
                    static_cast<int>(std::lround(static_cast<double>(scrollTrackH) * visibleRatio)));
                const int thumbTravel = std::max(0, scrollTrackH - thumbH);
                const double scrollRatio = static_cast<double>(context.fileBrowserScroll)
                    / static_cast<double>(std::max(1, maxScroll));
                const int thumbY =
                    scrollTrackY + static_cast<int>(std::lround(scrollRatio * static_cast<double>(thumbTravel)));
                context.drawFilledRect(scrollTrackX, thumbY, 4, thumbH, context.colorButtonActive);
            }

            const UiRect valueRect {modalX + 12, modalY + modalH - 66, modalW - 24, 24};
            context.drawFilledRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, context.colorBackground);
            context.drawRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, context.colorGridLine);
            context.drawText(valueRect.x + 8, valueRect.y + 16, context.inlinePrompt.value + "_", context.colorText);
            context.fileBrowserHits.push_back({valueRect, "value", -1});
        } else {
            const UiRect valueRect {modalX + 12, modalY + 52, modalW - 24, 30};
            context.drawFilledRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, context.colorBackground);
            context.drawRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, context.colorGridLine);
            context.drawText(valueRect.x + 8, valueRect.y + 20, context.inlinePrompt.value + "_", context.colorText);
        }
        context.inlinePromptAcceptButton = UiRect {modalX + modalW - 200, modalY + modalH - 34, 88, 22};
        context.inlinePromptCancelButton = UiRect {modalX + modalW - 104, modalY + modalH - 34, 88, 22};
        context.drawButton(context.inlinePromptAcceptButton, "APPLY", false);
        context.drawButton(context.inlinePromptCancelButton, "CANCEL", false);
        context.inlinePromptButtonsVisible = true;
    }
}

} // namespace arachno
