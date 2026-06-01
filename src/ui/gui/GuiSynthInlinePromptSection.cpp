#include "ui/gui/GuiSynthInlinePromptSection.h"

#include <algorithm>

namespace arachno {

void drawSynthInlinePromptSection(const GuiSynthInlinePromptSectionContext& context) {
    if (!context.inlinePrompt.active) {
        context.synthInlinePromptButtonsVisible = false;
        return;
    }

    const bool browserMode = context.inlinePromptUsesFileBrowser(context.inlinePrompt.kind);
    const int modalW = browserMode ? std::min(context.synthWindowWidth - 40, 760) : std::min(context.synthWindowWidth - 40, 620);
    const int modalH = browserMode ? std::min(context.synthWindowHeight - 40, 460) : 136;
    const int modalX = std::max(12, (context.synthWindowWidth - modalW) / 2);
    const int modalY = std::max(12, (context.synthWindowHeight - modalH) / 2);
    context.drawFilledRect(modalX, modalY, modalW, modalH, context.colors.panel);
    context.drawRect(modalX, modalY, modalW, modalH, context.colors.gridLine);
    context.drawText(modalX + 12, modalY + 24, context.inlinePrompt.title, context.colors.text);
    context.drawText(modalX + 12, modalY + 42, context.inlinePrompt.hint, context.colors.mutedText);
    if (browserMode) {
        if (context.fileBrowserEntries.empty()) {
            context.refreshFileBrowserEntries();
        }
        const UiRect homeRect {modalX + 12, modalY + 54, 56, 22};
        const UiRect upRect {modalX + 72, modalY + 54, 46, 22};
        const UiRect refreshRect {modalX + 122, modalY + 54, 72, 22};
        const UiRect dirRect {modalX + 198, modalY + 54, modalW - 210, 22};
        context.drawButton(homeRect, "HOME", false);
        context.drawButton(upRect, "UP", false);
        context.drawButton(refreshRect, "REFRESH", false);
        context.drawFilledRect(dirRect.x, dirRect.y, dirRect.width, dirRect.height, context.colors.background);
        context.drawRect(dirRect.x, dirRect.y, dirRect.width, dirRect.height, context.colors.gridLine);
        context.drawText(
            dirRect.x + 6,
            dirRect.y + 15,
            context.fitText(context.fileBrowserDirectory.string(), dirRect.width - 10),
            context.colors.text);
        context.fileBrowserHits.push_back({homeRect, "nav_home", -1});
        context.fileBrowserHits.push_back({upRect, "nav_up", -1});
        context.fileBrowserHits.push_back({refreshRect, "nav_refresh", -1});

        context.fileBrowserListRect = UiRect {modalX + 12, modalY + 82, modalW - 24, modalH - 156};
        context.drawFilledRect(
            context.fileBrowserListRect.x,
            context.fileBrowserListRect.y,
            context.fileBrowserListRect.width,
            context.fileBrowserListRect.height,
            context.colors.background);
        context.drawRect(
            context.fileBrowserListRect.x,
            context.fileBrowserListRect.y,
            context.fileBrowserListRect.width,
            context.fileBrowserListRect.height,
            context.colors.gridLine);
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
                context.drawFilledRect(rowRect.x, rowRect.y, rowRect.width, rowRect.height, context.colors.selection);
            }
            context.drawText(
                rowRect.x + 6,
                rowRect.y + 13,
                std::string(entry.directory ? "[DIR] " : "      ") + entry.name,
                selected ? context.colors.selectionText : context.colors.text);
            context.fileBrowserHits.push_back({rowRect, "entry", index});
        }
        const UiRect valueRect {modalX + 12, modalY + modalH - 66, modalW - 24, 24};
        context.drawFilledRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, context.colors.background);
        context.drawRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, context.colors.gridLine);
        context.drawText(valueRect.x + 8, valueRect.y + 16, context.inlinePrompt.value + "_", context.colors.text);
        context.fileBrowserHits.push_back({valueRect, "value", -1});
    } else {
        const UiRect valueRect {modalX + 12, modalY + 50, modalW - 24, 30};
        context.drawFilledRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, context.colors.background);
        context.drawRect(valueRect.x, valueRect.y, valueRect.width, valueRect.height, context.colors.gridLine);
        context.drawText(valueRect.x + 8, valueRect.y + 20, context.inlinePrompt.value + "_", context.colors.text);
    }
    context.synthInlinePromptAcceptButton = UiRect {modalX + modalW - 196, modalY + modalH - 34, 88, 22};
    context.synthInlinePromptCancelButton = UiRect {modalX + modalW - 102, modalY + modalH - 34, 88, 22};
    context.drawButton(context.synthInlinePromptAcceptButton, "APPLY", false);
    context.drawButton(context.synthInlinePromptCancelButton, "CANCEL", false);
    context.synthInlinePromptButtonsVisible = true;
}

} // namespace arachno
