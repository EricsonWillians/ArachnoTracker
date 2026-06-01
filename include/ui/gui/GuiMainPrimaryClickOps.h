#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainPrimaryClickResult {
    bool consumed = false;
    bool needsRedraw = false;
};

struct GuiMainPrimaryClickContext {
    int mx = 0;
    int my = 0;
    int playbackSampleRate = 48000;

    const std::vector<std::pair<UiRect, std::string>>& transportButtons;
    const std::vector<std::pair<UiRect, std::string>>& fileButtons;
    const std::vector<std::pair<UiRect, GuiThemeMode>>& themeButtons;
    const std::vector<std::pair<UiRect, AudioPerformanceMode>>& audioPerformanceButtons;
    const std::vector<OrderSlotHit>& orderSlotHits;
    const std::vector<TrackHeaderHit>& trackHeaderHits;

    const UiRect& gridTrackPrevButton;
    const UiRect& gridTrackNextButton;
    const UiRect& patternPrevButton;
    const UiRect& patternNextButton;
    const UiRect& patternNewButton;
    const UiRect& patternCloneButton;
    const UiRect& patternDeleteButton;
    const UiRect& orderPrevButton;
    const UiRect& orderNextButton;
    const UiRect& orderInsertButton;
    const UiRect& orderAppendButton;
    const UiRect& orderDeleteButton;

    std::function<void(const std::string&)> runTransportAction;
    std::function<void(const std::string&)> runFileButtonAction;
    std::function<void(GuiThemeMode)> setThemeMode;
    std::function<void(AudioPerformanceMode, int)> setAudioPerformanceMode;
    std::function<void(int)> shiftGridTrackWindow;

    std::function<void()> beginPatternCreatePrompt;
    std::function<void()> beginPatternClonePrompt;
    std::function<void()> deleteActivePattern;
    std::function<void()> insertOrderAtSelection;
    std::function<void()> appendOrderFromActivePattern;
    std::function<void()> removeSelectedOrder;
    std::function<void(int)> cyclePattern;
    std::function<void(int)> cycleOrder;
    std::function<void(int, bool)> selectOrderSlot;
    std::function<bool(const TrackHeaderHit&, int, int)> handleTrackHeaderClick;
};

GuiMainPrimaryClickResult handleMainPrimaryLeftClick(const GuiMainPrimaryClickContext& context);

} // namespace arachno
