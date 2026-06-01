#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "ui/gui/SynthUiData.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiSynthMotionResult {
    bool needsRedraw = false;
    bool synthWindowNeedsRedraw = false;
};

struct GuiSynthMotionContext {
    int& synthPointerX;
    int& synthPointerY;
    const std::vector<SynthWindowHit>& synthWindowHits;
    std::string& synthTooltipParam;
    std::chrono::steady_clock::time_point& synthTooltipHoverSince;
    bool inlinePromptActive = false;
    bool& synthParamDragActive;
    bool synthParamDragKnob = false;
    const std::string& synthParamDragName;
    UiRect synthParamDragRect;
    int synthParamDragStartX = 0;
    int synthParamDragStartY = 0;
    double synthParamDragStartValue = 0.0;
    double& synthParamDragLastValue;
    bool& synthParamDragDirty;
    bool synthPointerDown = false;

    std::function<int()> clampInstrumentIndex;
    std::function<const SynthParamDef*(const std::string&)> findSynthParamDef;
    std::function<bool(int, const std::string&, double, bool)> setSynthParameter;
    std::function<bool(int, int, bool)> triggerSynthKeyboardPointer;
};

GuiSynthMotionResult handleSynthWindowMotion(
    const GuiSynthMotionContext& context,
    int x,
    int y,
    unsigned int stateMask);

} // namespace arachno
