#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <X11/Xlib.h>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "ui/gui/GuiAudioRuntime.h"
#include "ui/gui/GuiThemePaletteOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainWindowRenderContext {
    Display* display = nullptr;
    GC gc = nullptr;
    XFontStruct* uiFont = nullptr;
    Window window = 0;
    Pixmap trackerBackbuffer = 0;
    int windowWidth = 0;
    int windowHeight = 0;

    GuiThemeMode themeMode = GuiThemeMode::Dos;
    const UiThemePalette& dosTheme;
    const UiThemePalette& highContrastTheme;

    ApplicationSession& session;
    GuiAudioRuntime& audioRuntime;

    int& topPanelHeightState;
    int& sidebarWidthState;
    TrackerWindowLayout& layout;

    int& selectedOrderIndex;
    int& armedInstrument;
    int& armedOctave;
    float& defaultVelocity;
    bool& stepAdvance;
    bool& followPlayback;
    int& activePatternRows;
    int& paintNoteMidi;
    int& requestedRowCount;
    int& gridTrackStart;
    int& pointerX;
    int& pointerY;
    int& hoveredTrackHeader;
    int& instrumentListVisibleRows;
    int& instrumentListStart;
    int& sidebarScrollOffset;
    int& sidebarContentHeight;
    bool& synthWindowVisible;
    bool& audioTuningDialogActive;
    bool& instrumentBrowserActive;
    std::string& instrumentBrowserQuery;
    int& instrumentBrowserScroll;
    int& instrumentBrowserSelected;
    InlinePromptState& inlinePrompt;
    bool& inlinePromptButtonsVisible;
    std::filesystem::path& fileBrowserDirectory;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::vector<FileBrowserHit>& fileBrowserHits;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    UnsavedDecisionPromptState& unsavedPrompt;
    std::vector<std::pair<UiRect, UnsavedChangesChoice>>& unsavedPromptChoices;
    double& targetSongLengthMinutes;
    int& midiImportRowsPerBeat;
    int& midiImportPatternRows;
    bool& midiImportSplitByTrack;

    const AppActionResult& lastAction;

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
    UiRect& inlinePromptAcceptButton;
    UiRect& inlinePromptCancelButton;

    std::function<void()> clampInstrumentListWindow;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<bool(InlinePromptKind)> isSynthInlinePromptKind;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    std::function<void()> refreshFileBrowserEntries;
    std::function<std::vector<int>(const AppSessionSnapshot&)> filteredInstrumentIndices;
};

void renderMainWindowFromState(const GuiMainWindowRenderContext& context);

} // namespace arachno
