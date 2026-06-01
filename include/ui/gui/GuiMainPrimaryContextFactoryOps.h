#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "ui/gui/GuiMainPrimaryClickOps.h"

namespace arachno {

struct GuiMainPrimaryContextFactoryInput {
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

    TrackerWindowLayout& layout;
    int& gridTrackStart;
    int& selectedOrderIndex;

    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(const AppActionRequest&)> runActionRequest;
    std::function<void(const std::string&)> runFileButtonAction;
    std::function<void(GuiThemeMode)> setThemeMode;
    std::function<void(AudioPerformanceMode, int)> setAudioPerformanceMode;
    std::function<void()> beginPatternCreatePrompt;
    std::function<void()> beginPatternClonePrompt;
    std::function<void()> deleteActivePattern;
    std::function<void()> insertOrderAtSelection;
    std::function<void()> appendOrderFromActivePattern;
    std::function<void()> removeSelectedOrder;
    std::function<bool(int, bool)> selectPatternIndex;
    std::function<bool(int, bool)> selectOrderIndex;
    std::function<void(int, int)> moveCursor;
};

GuiMainPrimaryClickContext makeMainPrimaryClickContextFromState(const GuiMainPrimaryContextFactoryInput& input);

} // namespace arachno
