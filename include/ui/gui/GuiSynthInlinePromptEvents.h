#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <X11/X.h>
#include <X11/keysym.h>

#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiSynthInlinePromptEventContext {
    InlinePromptState& inlinePrompt;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    bool synthInlinePromptButtonsVisible = false;
    UiRect synthInlinePromptAcceptButton;
    UiRect synthInlinePromptCancelButton;
    std::vector<FileBrowserHit>& fileBrowserHits;
    UiRect fileBrowserListRect;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::filesystem::path& fileBrowserDirectory;
    std::function<void()> refreshFileBrowserEntries;
    std::function<void()> executeInlinePrompt;
    std::function<void()> cancelInlinePrompt;
};

bool handleSynthInlinePromptButtonPress(
    const GuiSynthInlinePromptEventContext& context,
    int button,
    int x,
    int y);

bool handleSynthInlinePromptKeyPress(
    const GuiSynthInlinePromptEventContext& context,
    KeySym key,
    bool ctrlDown,
    bool altDown,
    const char* lookupBuffer,
    int lookupCount);

} // namespace arachno
