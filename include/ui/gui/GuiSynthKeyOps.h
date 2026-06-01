#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <X11/Xlib.h>

#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiSynthKeyResult {
    bool consumed = false;
    bool needsRedraw = false;
    bool synthWindowNeedsRedraw = false;
};

struct GuiSynthKeyContext {
    InlinePromptState& inlinePrompt;
    std::function<bool(InlinePromptKind)> isSynthInlinePromptKind;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    bool& synthInlinePromptButtonsVisible;
    UiRect& synthInlinePromptAcceptButton;
    UiRect& synthInlinePromptCancelButton;
    std::vector<FileBrowserHit>& fileBrowserHits;
    UiRect& fileBrowserListRect;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::filesystem::path& fileBrowserDirectory;
    std::function<void()> refreshFileBrowserEntries;
    std::function<void()> executeInlinePrompt;
    std::function<void()> cancelInlinePrompt;

    int& armedOctave;
    int& synthPreviewMidi;
    int& paintNoteMidi;
    std::function<void(int)> setArmedOctave;
    std::function<void(int)> ensureSynthKeyboardShowsMidi;
    std::function<void(bool)> setSynthWindowVisible;
    std::function<void(int)> cycleInstrument;
    std::function<void(int)> auditionSynthPreviewMidi;
    std::function<bool(unsigned int, int)> claimSynthPreviewKey;
    std::function<void(unsigned int)> releaseSynthPreviewKey;
};

GuiSynthKeyResult handleSynthWindowKeyPress(
    GuiSynthKeyContext& context,
    KeySym key,
    const XKeyEvent& keyEvent);

GuiSynthKeyResult handleSynthWindowKeyRelease(
    const GuiSynthKeyContext& context,
    unsigned int keycode,
    bool autoRepeatRelease);

} // namespace arachno
