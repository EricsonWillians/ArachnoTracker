#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "AppActions.h"
#include "ui/gui/GuiAudioRuntime.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainTopPanelDrawContext {
    const AppSessionSnapshot& snapshot;
    const PlaybackSnapshot& playback;
    GuiAudioRuntime& audioRuntime;

    int windowWidth = 0;
    int margin = 0;
    int headerHeight = 0;
    int statusHeight = 0;
    int lineHeight = 17;
    int gridTop = 0;
    int gridHeight = 0;
    int verticalSplitterX = 0;
    int horizontalSplitterY = 0;

    bool synthWindowVisible = false;
    bool audioTuningDialogActive = false;
    int selectedOrderIndex = 0;
    int armedInstrument = 0;
    int armedOctave = 0;
    float defaultVelocity = 0.8f;
    bool stepAdvance = true;
    bool followPlayback = true;
    GuiThemeMode themeMode = GuiThemeMode::Dos;

    unsigned long colorPanel = 0;
    unsigned long colorGridLine = 0;
    unsigned long colorText = 0;
    unsigned long colorMutedText = 0;
    unsigned long colorCursor = 0;
    unsigned long colorPlayhead = 0;
    unsigned long colorButton = 0;
    unsigned long colorButtonActive = 0;
    unsigned long colorButtonLabel = 0;
    unsigned long colorButtonLabelActive = 0;
    unsigned long colorActiveTagText = 0;

    std::vector<std::pair<UiRect, std::string>>& transportButtons;
    std::vector<std::pair<UiRect, std::string>>& fileButtons;
    std::vector<std::pair<UiRect, GuiThemeMode>>& themeButtons;
    std::vector<std::pair<UiRect, AudioPerformanceMode>>& audioPerformanceButtons;
    std::vector<OrderSlotHit>& orderSlotHits;

    UiRect& patternPrevButton;
    UiRect& patternNextButton;
    UiRect& patternValueButton;
    UiRect& patternNewButton;
    UiRect& patternCloneButton;
    UiRect& patternDeleteButton;
    UiRect& orderPrevButton;
    UiRect& orderNextButton;
    UiRect& orderValueButton;
    UiRect& orderInsertButton;
    UiRect& orderAppendButton;
    UiRect& orderDeleteButton;

    std::function<void(int, int, int, int, unsigned long)> drawFilledRect;
    std::function<void(int, int, int, int, unsigned long)> drawRect;
    std::function<void(int, int, const std::string&, unsigned long)> drawText;
    std::function<void(const UiRect&, const std::string&, bool)> drawButton;
    std::function<int(int, int)> controlTextBaseline;
    std::function<int(const std::string&)> textWidth;
};

void drawMainTopPanelSection(const GuiMainTopPanelDrawContext& context);

} // namespace arachno
