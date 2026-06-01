#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "ui/gui/SynthUiData.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiSynthParamSectionThemeColors {
    unsigned long background = 0;
    unsigned long panel = 0;
    unsigned long gridLine = 0;
    unsigned long mutedText = 0;
    unsigned long text = 0;
    unsigned long buttonActive = 0;
};

struct GuiSynthParamSectionContext {
    int synthWindowWidth = 0;
    int synthWindowHeight = 0;
    int panelRight = 0;
    int controlsTop = 0;
    int& synthParamPage;
    int& synthParamOscTarget;
    int& synthParamScroll;
    UiRect& synthParamViewport;
    int& synthParamContentHeight;
    std::string& synthTooltipParam;
    int synthPointerX = 0;
    int synthPointerY = 0;
    std::chrono::steady_clock::time_point synthTooltipHoverSince {};
    std::vector<SynthWindowHit>& synthWindowHits;
    const std::vector<SynthParamDef>& synthParamDefs;
    const SynthPatch& patch;
    GuiSynthParamSectionThemeColors colors;

    std::function<void(int, int, int, int, unsigned long)> drawFilledRect;
    std::function<void(int, int, int, int, unsigned long)> drawRect;
    std::function<void(int, int, const std::string&, unsigned long)> drawText;
    std::function<void(const UiRect&, const std::string&, bool)> drawButton;
    std::function<void(const UiRect&, double, bool)> drawKnob;
    std::function<std::string(const std::string&, int)> fitText;
    std::function<int(const std::string&)> textWidth;
    std::function<void(int, int, int, int)> setClipRect;
    std::function<void()> clearClip;
};

void drawSynthParameterSection(const GuiSynthParamSectionContext& context);

} // namespace arachno
