#pragma once

#include <utility>
#include <vector>

#include "ui/gui/GuiThemePaletteOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainDrawResetContext {
    std::vector<std::pair<UiRect, std::string>>& transportButtons;
    std::vector<std::pair<UiRect, int>>& instrumentHitTargets;
    std::vector<std::pair<UiRect, std::string>>& instrumentControlHits;
    std::vector<std::pair<UiRect, int>>& instrumentBrowserHitTargets;
    std::vector<TrackHeaderHit>& trackHeaderHits;
    std::vector<OrderSlotHit>& orderSlotHits;
    std::vector<std::pair<UiRect, std::string>>& fileButtons;
    std::vector<std::pair<UiRect, GuiThemeMode>>& themeButtons;
    std::vector<std::pair<UiRect, AudioPerformanceMode>>& audioPerformanceButtons;
    std::vector<AudioTuningDialogHit>& audioTuningDialogHits;
    std::vector<std::pair<UiRect, int>>& octaveHitTargets;
    std::vector<PianoKeyHit>& pianoKeyHits;
    std::vector<TrackMetadataHit>& trackMetadataHits;
    std::vector<SongLengthHit>& songLengthHits;
    std::vector<MidiImportSettingHit>& midiImportSettingHits;
    std::vector<FileBrowserHit>& fileBrowserHits;
    std::vector<std::pair<UiRect, UnsavedChangesChoice>>& unsavedPromptChoices;

    bool& inlinePromptButtonsVisible;
    UiRect& patternRowsMinus;
    UiRect& patternRowsPlus;
    UiRect& patternRowsValue;
    UiRect& gridTrackPrevButton;
    UiRect& gridTrackNextButton;
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
    UiRect& stepAdvanceButton;
    UiRect& followPlaybackButton;
    UiRect& instrumentListRect;
    UiRect& instrumentBrowserListRect;
    UiRect& instrumentBrowserAcceptButton;
    UiRect& instrumentBrowserCancelButton;
    UiRect& sidebarViewport;
    UiRect& fileBrowserListRect;
    UiRect& audioTuningDialogRect;
};

void resetMainDrawTransientState(const GuiMainDrawResetContext& context);

struct GuiMainDrawLayoutResult {
    int lineHeight = 17;
    int margin = 12;
    int headerHeight = 156;
    int statusHeight = 70;
    int gridTop = 0;
    int gridBottom = 0;
    int gridHeight = 120;
    int sidebarWidth = 300;
    int gridWidth = 800;
    int gridLeft = 12;
    int sidebarLeft = 824;
};

GuiMainDrawLayoutResult computeMainDrawLayout(
    int windowWidth,
    int windowHeight,
    int& topPanelHeightState,
    int& sidebarWidthState,
    TrackerWindowLayout& layout);

struct GuiMainDrawThemeColors {
    unsigned long background = 0;
    unsigned long panel = 0;
    unsigned long gridHeader = 0;
    unsigned long gridLine = 0;
    unsigned long selection = 0;
    unsigned long cursor = 0;
    unsigned long text = 0;
    unsigned long mutedText = 0;
    unsigned long playhead = 0;
    unsigned long button = 0;
    unsigned long buttonActive = 0;
    unsigned long buttonLabel = 0;
    unsigned long buttonLabelActive = 0;
    unsigned long selectionText = 0;
    unsigned long cursorText = 0;
    unsigned long playheadText = 0;
    unsigned long activeTagText = 0;
};

GuiMainDrawThemeColors makeMainDrawThemeColors(const UiThemePalette& theme);

} // namespace arachno
