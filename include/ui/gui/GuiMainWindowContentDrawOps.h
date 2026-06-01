#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "AppActions.h"
#include "ui/gui/GuiAudioRuntime.h"
#include "ui/gui/GuiThemePaletteOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainWindowContentDrawContext {
    const AppSessionSnapshot& snapshot;
    const PlaybackSnapshot& playback;
    GuiAudioRuntime& audioRuntime;

    int windowWidth = 0;
    int windowHeight = 0;
    int margin = 0;
    int headerHeight = 0;
    int statusHeight = 0;
    int lineHeight = 17;
    int gridTop = 0;
    int gridHeight = 0;
    int sidebarWidth = 0;
    int gridWidth = 0;
    int gridLeft = 0;
    int sidebarLeft = 0;

    int& gridTrackStart;
    int& requestedRowCount;
    int pointerX = 0;
    int pointerY = 0;
    int hoveredTrackHeader = -1;

    unsigned long colorBackground = 0;
    unsigned long colorPanel = 0;
    unsigned long colorGridHeader = 0;
    unsigned long colorGridLine = 0;
    unsigned long colorSelection = 0;
    unsigned long colorCursor = 0;
    unsigned long colorText = 0;
    unsigned long colorMutedText = 0;
    unsigned long colorPlayhead = 0;
    unsigned long colorButton = 0;
    unsigned long colorButtonActive = 0;
    unsigned long colorButtonLabel = 0;
    unsigned long colorButtonLabelActive = 0;
    unsigned long colorSelectionText = 0;
    unsigned long colorCursorText = 0;
    unsigned long colorPlayheadText = 0;
    unsigned long colorActiveTagText = 0;

    const UiThemePalette& theme;
    TrackerWindowLayout& layout;

    bool synthWindowVisible = false;
    bool& audioTuningDialogActive;
    int selectedOrderIndex = 0;
    int armedInstrument = 0;
    int armedOctave = 0;
    float defaultVelocity = 0.8f;
    bool stepAdvance = true;
    bool followPlayback = true;
    GuiThemeMode themeMode = GuiThemeMode::Dos;
    int activePatternRows = 64;
    int paintNoteMidi = 60;
    const AppActionResult& lastAction;
    double targetSongLengthMinutes = 0.0;
    int midiImportRowsPerBeat = 4;
    int midiImportPatternRows = 64;
    bool midiImportSplitByTrack = true;

    int& instrumentListVisibleRows;
    int& instrumentListStart;
    std::function<void()> clampInstrumentListWindow;

    std::vector<std::pair<UiRect, std::string>>& transportButtons;
    std::vector<std::pair<UiRect, std::string>>& fileButtons;
    std::vector<std::pair<UiRect, GuiThemeMode>>& themeButtons;
    std::vector<std::pair<UiRect, AudioPerformanceMode>>& audioPerformanceButtons;
    std::vector<OrderSlotHit>& orderSlotHits;
    std::vector<TrackHeaderHit>& trackHeaderHits;
    std::vector<std::pair<UiRect, int>>& octaveHitTargets;
    std::vector<PianoKeyHit>& pianoKeyHits;
    std::vector<TrackMetadataHit>& trackMetadataHits;
    std::vector<SongLengthHit>& songLengthHits;
    std::vector<MidiImportSettingHit>& midiImportSettingHits;
    std::vector<std::pair<UiRect, std::string>>& instrumentControlHits;
    std::vector<std::pair<UiRect, int>>& instrumentHitTargets;
    UiRect& instrumentListRect;

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
    UiRect& patternRowsMinus;
    UiRect& patternRowsValue;
    UiRect& patternRowsPlus;
    UiRect& stepAdvanceButton;
    UiRect& followPlaybackButton;
    UiRect& gridTrackPrevButton;
    UiRect& gridTrackNextButton;

    UiRect& sidebarViewport;
    int& sidebarScrollOffset;
    int& sidebarContentHeight;

    UnsavedDecisionPromptState& unsavedPrompt;
    std::vector<std::pair<UiRect, UnsavedChangesChoice>>& unsavedPromptChoices;
    bool& instrumentBrowserActive;
    std::string& instrumentBrowserQuery;
    int& instrumentBrowserScroll;
    int& instrumentBrowserSelected;
    std::vector<std::pair<UiRect, int>>& instrumentBrowserHitTargets;
    UiRect& instrumentBrowserListRect;
    UiRect& instrumentBrowserAcceptButton;
    UiRect& instrumentBrowserCancelButton;

    bool& inlinePromptButtonsVisible;
    InlinePromptState& inlinePrompt;
    std::function<bool(InlinePromptKind)> isSynthInlinePromptKind;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    std::function<void()> refreshFileBrowserEntries;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::filesystem::path& fileBrowserDirectory;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    std::vector<FileBrowserHit>& fileBrowserHits;
    UiRect& fileBrowserListRect;
    UiRect& inlinePromptAcceptButton;
    UiRect& inlinePromptCancelButton;

    UiRect& audioTuningDialogRect;
    std::vector<AudioTuningDialogHit>& audioTuningDialogHits;
    std::function<std::vector<int>(const AppSessionSnapshot&)> filteredInstrumentIndices;

    std::function<void(int, int, int, int, unsigned long)> drawFilledRect;
    std::function<void(int, int, int, int, unsigned long)> drawRect;
    std::function<void(int, int, const std::string&, unsigned long)> drawText;
    std::function<void(const UiRect&, const std::string&, bool)> drawButton;
    std::function<int(int, int)> controlTextBaseline;
    std::function<std::string(const std::string&, int)> fitText;
    std::function<int(const std::string&)> textWidth;
    std::function<void(int, int, int, int)> setClipRect;
    std::function<void()> clearClip;
};

void drawMainWindowContent(const GuiMainWindowContentDrawContext& context);

} // namespace arachno
