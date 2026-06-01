#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "AppActions.h"
#include "ui/gui/GuiAudioRuntime.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainModalDrawContext {
    int windowWidth = 0;
    int windowHeight = 0;

    unsigned long colorPanel = 0;
    unsigned long colorGridLine = 0;
    unsigned long colorText = 0;
    unsigned long colorMutedText = 0;
    unsigned long colorBackground = 0;
    unsigned long colorSelection = 0;
    unsigned long colorSelectionText = 0;
    unsigned long colorButtonActive = 0;

    const AppSessionSnapshot& snapshot;

    UnsavedDecisionPromptState& unsavedPrompt;
    std::vector<std::pair<UiRect, UnsavedChangesChoice>>& unsavedPromptChoices;

    bool& instrumentBrowserActive;
    std::string& instrumentBrowserQuery;
    int& instrumentBrowserScroll;
    int& instrumentBrowserSelected;
    std::vector<std::pair<UiRect, int>>& instrumentBrowserHitTargets;
    UiRect& instrumentBrowserListRect;
    UiRect& instrumentBrowserAcceptButton;
    UiRect& instrumentBrowserCancelButton;
    std::function<std::vector<int>(const AppSessionSnapshot&)> filteredInstrumentIndices;
    int armedInstrument = 0;

    bool& audioTuningDialogActive;
    UiRect& audioTuningDialogRect;
    std::vector<AudioTuningDialogHit>& audioTuningDialogHits;
    AudioPerformanceMode audioPerformanceMode = AudioPerformanceMode::Auto;
    int audioCustomLevel = 0;
    int audioFrameMin = 0;
    int audioFrameMax = 0;

    InlinePromptState& inlinePrompt;
    bool synthWindowVisible = false;
    std::function<bool(InlinePromptKind)> isSynthInlinePromptKind;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    std::function<void()> refreshFileBrowserEntries;

    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::filesystem::path& fileBrowserDirectory;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    std::vector<FileBrowserHit>& fileBrowserHits;
    UiRect& fileBrowserListRect;

    UiRect& inlinePromptAcceptButton;
    UiRect& inlinePromptCancelButton;
    bool& inlinePromptButtonsVisible;

    std::function<void(int, int, int, int, unsigned long)> drawFilledRect;
    std::function<void(int, int, int, int, unsigned long)> drawRect;
    std::function<void(int, int, const std::string&, unsigned long)> drawText;
    std::function<void(const UiRect&, const std::string&, bool)> drawButton;
    std::function<std::string(const std::string&, int)> fitText;
    std::function<int(const std::string&)> textWidth;
};

void drawMainModalOverlays(const GuiMainModalDrawContext& context);

} // namespace arachno
