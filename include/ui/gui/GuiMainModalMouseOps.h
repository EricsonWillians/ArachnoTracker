#pragma once

#include <filesystem>
#include <functional>
#include <vector>

#include <X11/X.h>

#include "ProjectLifecycle.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainModalMouseResult {
    bool consumed = false;
    bool needsRedraw = false;
};

struct GuiMainModalMouseContext {
    UnsavedDecisionPromptState& unsavedPrompt;
    std::vector<std::pair<UiRect, UnsavedChangesChoice>>& unsavedPromptChoices;
    std::function<void(UnsavedChangesChoice)> resolveUnsavedPrompt;

    bool& instrumentBrowserActive;
    UiRect& instrumentBrowserAcceptButton;
    UiRect& instrumentBrowserCancelButton;
    UiRect& instrumentBrowserListRect;
    std::vector<std::pair<UiRect, int>>& instrumentBrowserHitTargets;
    int& instrumentBrowserSelected;
    int& instrumentBrowserScroll;
    std::function<void(bool)> closeInstrumentBrowser;

    bool& audioTuningDialogActive;
    std::vector<AudioTuningDialogHit>& audioTuningDialogHits;
    UiRect& audioTuningDialogRect;
    int playbackSampleRate = 48000;
    std::function<void(AudioPerformanceMode, int)> setAudioPerformanceMode;
    std::function<void(int, int)> adjustAudioCustomLevel;
    std::function<void(int)> resetAudioCustomLevel;

    InlinePromptState& inlinePrompt;
    bool synthWindowVisible = false;
    std::function<bool(InlinePromptKind)> isSynthInlinePromptKind;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    bool& inlinePromptButtonsVisible;
    UiRect& inlinePromptAcceptButton;
    UiRect& inlinePromptCancelButton;
    std::vector<FileBrowserHit>& fileBrowserHits;
    UiRect& fileBrowserListRect;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::filesystem::path& fileBrowserDirectory;
    std::function<void()> refreshFileBrowserEntries;
    std::function<void()> executeInlinePrompt;
    std::function<void()> cancelInlinePrompt;
};

GuiMainModalMouseResult handleMainModalButtonPress(
    const GuiMainModalMouseContext& context,
    int button,
    int mx,
    int my,
    unsigned int stateMask);

} // namespace arachno
