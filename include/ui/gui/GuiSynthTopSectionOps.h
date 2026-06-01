#pragma once

#include <functional>
#include <string>
#include <vector>

#include "Instrument.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiSynthTopSectionContext {
    int synthWindowWidth = 0;
    int instrument = 0;
    int synthParamPage = 0;
    const SynthPatch& patch;
    std::vector<SynthWindowHit>& synthWindowHits;
    unsigned long textColor = 0;

    std::function<void(int, int, const std::string&, unsigned long)> drawText;
    std::function<void(const UiRect&, const std::string&, bool)> drawButton;
    std::function<std::string(const std::string&, int)> fitText;
};

struct GuiSynthTopSectionResult {
    int panelLeft = 16;
    int panelRight = 120;
    int oscTop = 0;
    int oscRowHeight = 24;
};

GuiSynthTopSectionResult drawSynthTopSection(const GuiSynthTopSectionContext& context);

} // namespace arachno
