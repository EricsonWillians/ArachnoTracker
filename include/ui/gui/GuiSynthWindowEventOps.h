#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <X11/Xlib.h>

#include "ui/gui/GuiSynthKeyOps.h"
#include "ui/gui/SynthUiData.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiSynthWindowEventResult {
    bool consumed = false;
    bool needsRedraw = false;
    bool synthWindowNeedsRedraw = false;
};

struct GuiSynthWindowEventContext {
    Display* display = nullptr;
    Window synthWindow = 0;
    Atom wmDelete = 0;

    int& synthWindowWidth;
    int& synthWindowHeight;
    int synthWindowMinWidth = 0;
    int synthWindowMinHeight = 0;
    std::function<void()> ensureSynthBackbuffer;
    std::function<void(bool)> setSynthWindowVisible;

    int& synthPointerX;
    int& synthPointerY;
    bool& synthPointerDown;
    int& synthLastPointerMidi;

    bool& synthParamDragActive;
    bool& synthParamDragKnob;
    bool& synthParamDragDirty;
    std::string& synthParamDragName;
    UiRect& synthParamDragRect;
    int& synthParamDragStartX;
    int& synthParamDragStartY;
    double& synthParamDragStartValue;
    double& synthParamDragLastValue;

    std::string& synthTooltipParam;
    std::chrono::steady_clock::time_point& synthTooltipHoverSince;

    UiRect& synthParamViewport;
    int synthParamContentHeight = 0;
    int& synthParamScroll;

    const std::vector<SynthWindowHit>& synthWindowHits;

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

    std::function<bool(int, int, bool)> triggerSynthKeyboardPointer;
    std::function<bool(int, int)> handleSynthWindowClick;
    std::function<void()> refreshSnapshot;

    std::function<int()> clampInstrumentIndex;
    std::function<const SynthParamDef*(const std::string&)> findSynthParamDef;
    std::function<bool(int, const std::string&, double, bool)> setSynthParameter;

    std::function<GuiSynthKeyContext()> makeSynthKeyContext;
    std::function<bool(const XEvent&)> isAutoRepeatRelease;
};

GuiSynthWindowEventResult handleSynthWindowEvent(
    const GuiSynthWindowEventContext& context,
    const XEvent& event);

} // namespace arachno
