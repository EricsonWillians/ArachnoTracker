#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiSynthInlinePromptThemeColors {
    unsigned long panel = 0;
    unsigned long gridLine = 0;
    unsigned long text = 0;
    unsigned long mutedText = 0;
    unsigned long background = 0;
    unsigned long selection = 0;
    unsigned long selectionText = 0;
};

struct GuiSynthInlinePromptSectionContext {
    int synthWindowWidth = 0;
    int synthWindowHeight = 0;
    InlinePromptState& inlinePrompt;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    std::function<void()> refreshFileBrowserEntries;
    std::filesystem::path& fileBrowserDirectory;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::vector<FileBrowserHit>& fileBrowserHits;
    UiRect& fileBrowserListRect;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    UiRect& synthInlinePromptAcceptButton;
    UiRect& synthInlinePromptCancelButton;
    bool& synthInlinePromptButtonsVisible;
    GuiSynthInlinePromptThemeColors colors;

    std::function<void(int, int, int, int, unsigned long)> drawFilledRect;
    std::function<void(int, int, int, int, unsigned long)> drawRect;
    std::function<void(int, int, const std::string&, unsigned long)> drawText;
    std::function<void(const UiRect&, const std::string&, bool)> drawButton;
    std::function<std::string(const std::string&, int)> fitText;
};

void drawSynthInlinePromptSection(const GuiSynthInlinePromptSectionContext& context);

} // namespace arachno
