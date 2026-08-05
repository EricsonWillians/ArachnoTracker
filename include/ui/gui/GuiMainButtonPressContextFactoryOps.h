#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "ui/gui/GuiMainPrimaryClickOps.h"
#include "ui/gui/GuiMainSidebarClickOps.h"

namespace arachno {

struct GuiMainButtonPressContextFactoryInput {
    TrackerWindowLayout& layout;
    int& gridTrackStart;
    int& selectedOrderIndex;

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

    std::vector<std::pair<UiRect, int>>& octaveHitTargets;
    std::vector<PianoKeyHit>& pianoKeyHits;
    std::vector<TrackMetadataHit>& trackMetadataHits;
    std::vector<SongLengthHit>& songLengthHits;
    std::vector<MidiImportSettingHit>& midiImportSettingHits;
    std::vector<std::pair<UiRect, std::string>>& instrumentControlHits;
    std::vector<std::pair<UiRect, int>>& instrumentHitTargets;
    UiRect& patternRowsMinus;
    UiRect& patternRowsPlus;
    UiRect& patternRowsValue;
    UiRect& stepAdvanceButton;
    UiRect& legatoButton;
    UiRect& followPlaybackButton;

    bool& draggingPatternRows;
    int& patternResizeAnchorY;
    int& patternResizeStartRows;
    int& activePatternRows;
    bool& stepAdvance;
    bool& followPlayback;
    int& paintNoteMidi;
    int& armedOctave;
    int& armedInstrument;
    double& targetSongLengthMinutes;
    int& midiImportRowsPerBeat;
    int& midiImportPatternRows;
    bool& midiImportSplitByTrack;

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

    std::function<void(int)> setArmedOctave;
    std::function<void()> applyArmedOctaveToSelection;
    std::function<void(int, int, int)> paintNoteAt;
    std::function<void(int)> ensurePatternRowsForRow;
    std::function<void()> refreshSnapshot;
    std::function<void(const std::string&, int)> runTrackMetadataAction;
    std::function<void(int)> resizePatternRows;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
    std::function<void(double)> buildSongToTargetSeconds;
    std::function<void(double)> trimSongToTargetSeconds;
    std::function<void(int)> selectInstrument;
    std::function<void()> auditionArmedInstrument;
    std::function<void()> openInstrumentBrowser;
};

GuiMainPrimaryClickContext makeMainPrimaryClickContextForButtonPress(
    const GuiMainButtonPressContextFactoryInput& input,
    int mx,
    int my,
    int playbackSampleRate);

GuiMainSidebarClickContext makeMainSidebarClickContextForButtonPress(
    const GuiMainButtonPressContextFactoryInput& input,
    bool pointerInSidebar,
    int mx,
    int my);

} // namespace arachno
