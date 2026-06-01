#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <X11/Xlib.h>

#include "ProjectLifecycle.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainKeyModalResult {
    bool consumed = false;
    bool needsRedraw = false;
};

struct GuiMainKeyModalContext {
    KeySym key = NoSymbol;
    bool ctrlDown = false;
    bool shiftDown = false;
    bool altDown = false;
    int lookupCount = 0;
    const char* lookupBuffer = nullptr;

    std::function<bool(KeySym)> keyMatches;
    std::function<int()> resolvedDigit;

    bool unsavedPromptActive = false;
    std::function<void(UnsavedChangesChoice)> resolveUnsavedPrompt;

    bool& audioTuningDialogActive;
    int playbackSampleRate = 48000;
    std::function<void(int, int)> adjustAudioCustomLevel;
    std::function<void(AudioPerformanceMode, int)> setAudioPerformanceMode;

    InlinePromptState& inlinePrompt;
    bool synthWindowVisible = false;
    std::function<bool(InlinePromptKind)> isSynthInlinePromptKind;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    int& fileBrowserSelected;
    int& fileBrowserScroll;
    UiRect& fileBrowserListRect;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::filesystem::path& fileBrowserDirectory;
    std::function<void()> refreshFileBrowserEntries;
    std::function<void()> executeInlinePrompt;
    std::function<void()> cancelInlinePrompt;

    bool& instrumentBrowserActive;
    int& instrumentBrowserSelected;
    int& instrumentBrowserScroll;
    std::string& instrumentBrowserQuery;
    std::function<void(bool)> closeInstrumentBrowser;
    std::function<void(int)> cycleInstrumentBy;
    std::function<void()> syncInstrumentBrowserSelectionFromArmed;
    std::function<int()> filteredInstrumentCount;
};

GuiMainKeyModalResult handleMainKeyModal(const GuiMainKeyModalContext& context);

} // namespace arachno
