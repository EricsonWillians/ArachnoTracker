#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "AppActions.h"
#include "ui/gui/GuiAudioRuntime.h"
#include "ui/gui/GuiMainWindowInteractionAdapterOps.h"
#include "ui/gui/GuiWindowEditingBindingsOps.h"
#include "ui/gui/GuiWindowInstrumentSynthOps.h"

namespace arachno {

struct GuiWindowMainInteractionFactoryInput {
    UnsavedDecisionPromptState& unsavedPrompt;
    std::vector<std::pair<UiRect, UnsavedChangesChoice>>& unsavedPromptChoices;
    std::function<void(UnsavedChangesChoice)> resolveUnsavedPrompt;

    bool& instrumentBrowserActive;
    UiRect& instrumentBrowserAcceptButton;
    UiRect& instrumentBrowserCancelButton;
    UiRect& instrumentBrowserListRect;
    std::vector<std::pair<UiRect, int>>& instrumentBrowserHitTargets;
    int& instrumentBrowserSelected;
    int& instrumentBrowserScroll;
    std::string& instrumentBrowserQuery;

    bool& audioTuningDialogActive;
    std::vector<AudioTuningDialogHit>& audioTuningDialogHits;
    UiRect& audioTuningDialogRect;

    InlinePromptState& inlinePrompt;
    bool& synthWindowVisible;
    std::function<bool(InlinePromptKind)> isSynthInlinePromptKind;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    bool& inlinePromptButtonsVisible;
    UiRect& inlinePromptAcceptButton;
    UiRect& inlinePromptCancelButton;
    std::vector<FileBrowserHit>& fileBrowserHits;
    UiRect& fileBrowserListRect;
    int& fileBrowserScroll;
    int& fileBrowserSelected;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::filesystem::path& fileBrowserDirectory;
    std::function<void()> refreshFileBrowserEntries;
    std::function<void()> executeInlinePrompt;
    std::function<void()> cancelInlinePrompt;

    TrackerWindowLayout& layout;
    UiRect& sidebarViewport;
    UiRect& instrumentListRect;
    int& sidebarContentHeight;
    int& sidebarScrollOffset;
    int& viewStartRow;
    bool& draggingSelection;
    int& dragAnchorRow;
    int& dragAnchorTrack;
    int& gridTrackStart;
    int& activePatternRows;

    int& paintNoteMidi;
    bool& keyboardSelectionActive;
    int& keyboardSelectionAnchorRow;
    int& keyboardSelectionAnchorTrack;
    bool& paintingNotes;
    int& lastPaintRow;
    int& lastPaintTrack;

    int& pointerX;
    int& pointerY;
    int& windowWidth;
    int& windowHeight;
    bool& resizingSidebar;
    bool& resizingTopPanel;
    bool& draggingPatternRows;
    int& patternResizeAnchorY;
    int& patternResizeStartRows;
    int& topPanelHeightState;
    int& sidebarWidthState;
    int& hoveredTrackHeader;
    std::vector<TrackHeaderHit>& trackHeaderHits;

    int& armedInstrument;
    int& armedOctave;
    bool& stepAdvance;
    float& defaultVelocity;
    int& synthPreviewMidi;
    int& requestedRowCount;
    bool& midiImportSplitByTrack;
    double& targetSongLengthMinutes;
    GuiThemeMode& themeMode;
    int& selectedOrderIndex;
    int& midiImportRowsPerBeat;
    int& midiImportPatternRows;
    bool& followPlayback;

    std::vector<std::pair<UiRect, std::string>>& transportButtons;
    std::vector<std::pair<UiRect, std::string>>& fileButtons;
    std::vector<std::pair<UiRect, GuiThemeMode>>& themeButtons;
    std::vector<std::pair<UiRect, AudioPerformanceMode>>& audioPerformanceButtons;
    std::vector<OrderSlotHit>& orderSlotHits;
    UiRect& gridTrackPrevButton;
    UiRect& gridTrackNextButton;
    UiRect& patternPrevButton;
    UiRect& patternNextButton;
    UiRect& patternNewButton;
    UiRect& patternCloneButton;
    UiRect& patternDeleteButton;
    UiRect& orderPrevButton;
    UiRect& orderNextButton;
    UiRect& orderInsertButton;
    UiRect& orderAppendButton;
    UiRect& orderDeleteButton;
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
    UiRect& followPlaybackButton;

    GuiAudioRuntime& audioRuntime;
    std::function<int()> audibleTrackCount;
    std::function<void(AudioPerformanceMode, int)> setAudioPerformanceMode;
    std::function<void(int, int)> adjustAudioCustomLevel;
    std::function<void(int)> resizePatternRows;
    std::function<void()> lockManualScroll;
    std::function<void()> refreshSnapshot;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(const std::string&)> runActionById;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
    std::function<AppActionResult(const AppActionRequest&, bool)> runActionWithRefresh;
    std::function<void(const std::string&)> runSyncById;
    std::function<void()> runEvents;
    std::function<PlaybackSnapshot()> playbackSnapshot;
    std::function<AudioPerformanceMode()> currentPerformanceMode;
    std::function<void(int)> setArmedOctave;
    std::function<void()> applyArmedOctaveToSelection;
    std::function<void(int)> ensurePatternRowsForRow;
    std::function<bool(unsigned int, int)> claimSynthPreviewKey;
    std::function<void(bool)> setSynthWindowVisible;
    std::function<void(const std::string&)> runFileButtonAction;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
    std::function<void(int)> ensureSynthKeyboardShowsMidi;

    GuiWindowEditingBindings& editingBindings;
    GuiWindowInstrumentSynthBindings& instrumentBindings;
};

GuiMainWindowInteractionAdapterContext makeMainInteractionContextFromWindowState(
    const GuiWindowMainInteractionFactoryInput& input);

} // namespace arachno
