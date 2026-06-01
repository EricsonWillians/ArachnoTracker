#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ApplicationSession.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainGridDrawContext {
    const AppSessionSnapshot& snapshot;
    const PlaybackSnapshot& playback;

    int gridLeft = 0;
    int gridTop = 0;
    int gridWidth = 0;
    int gridHeight = 0;
    int gridTrackStart = 0;

    unsigned long colorPanel = 0;
    unsigned long colorGridLine = 0;
    unsigned long colorGridHeader = 0;
    unsigned long colorText = 0;
    unsigned long colorMutedText = 0;
    unsigned long colorButton = 0;
    unsigned long colorButtonActive = 0;
    unsigned long colorActiveTagText = 0;
    unsigned long colorSelection = 0;
    unsigned long colorCursor = 0;
    unsigned long colorSelectionText = 0;
    unsigned long colorCursorText = 0;
    unsigned long colorPlayhead = 0;
    unsigned long colorPlayheadText = 0;

    int margin = 0;
    int windowWidth = 0;
    int windowHeight = 0;
    int pointerX = 0;
    int pointerY = 0;
    int hoveredTrackHeader = -1;

    TrackerWindowLayout& layout;
    int& requestedRowCount;
    int& gridTrackStartRef;

    std::vector<TrackHeaderHit>& trackHeaderHits;
    UiRect& gridTrackPrevButton;
    UiRect& gridTrackNextButton;

    std::function<void(int, int, int, int, unsigned long)> drawFilledRect;
    std::function<void(int, int, int, int, unsigned long)> drawRect;
    std::function<void(int, int, const std::string&, unsigned long)> drawText;
    std::function<void(const UiRect&, const std::string&, bool)> drawButton;
    std::function<std::string(const std::string&, int)> fitText;
    std::function<int(const std::string&)> textWidth;
};

void drawMainGridSection(const GuiMainGridDrawContext& context);

} // namespace arachno
