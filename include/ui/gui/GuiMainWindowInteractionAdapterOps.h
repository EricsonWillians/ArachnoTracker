#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "AppActions.h"
#include "ui/gui/GuiMainButtonPressContextFactoryOps.h"
#include "ui/gui/GuiMainGridClickOps.h"
#include "ui/gui/GuiMainKeyCommandOps.h"
#include "ui/gui/GuiMainKeyEditOps.h"
#include "ui/gui/GuiMainKeyModalOps.h"
#include "ui/gui/GuiMainKeyNoteOps.h"
#include "ui/gui/GuiMainModalMouseOps.h"
#include "ui/gui/GuiMainMotionOps.h"
#include "ui/gui/GuiMainWheelOps.h"
#include "ui/gui/GuiMainWindowEventAdapterOps.h"

namespace arachno {

struct GuiMainWindowInteractionAdapterContext {
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
    std::function<void(bool)> closeInstrumentBrowser;

    bool& audioTuningDialogActive;
    std::vector<AudioTuningDialogHit>& audioTuningDialogHits;
    UiRect& audioTuningDialogRect;
    std::function<void(AudioPerformanceMode, int)> setAudioPerformanceMode;
    std::function<void(int, int)> adjustAudioCustomLevel;
    std::function<void(int)> resetAudioCustomLevel;

    InlinePromptState& inlinePrompt;
    bool synthWindowVisible = false;
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
    int windowWidth = 1280;
    int windowHeight = 800;
    bool resizingSidebar = false;
    bool resizingTopPanel = false;
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

    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(int)> scrollInstrumentList;
    std::function<void(int)> resizePatternRows;
    std::function<void()> lockManualScroll;
    std::function<void()> refreshSnapshot;
    std::function<bool(int, int, int&, int&)> gridPositionToCell;
    std::function<void(int, int, int, int)> applySelectionRange;
    std::function<void(int, int)> moveCursor;
    std::function<void(int, int, int)> paintNoteAt;

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
    std::function<void()> auditionArmedInstrument;
    std::function<void(int)> cycleInstrumentBy;
    std::function<void(int)> selectInstrument;
    std::function<void(int)> auditionSynthPreviewMidi;
    std::function<bool(unsigned int, int)> claimSynthPreviewKey;
    std::function<void()> beginTemporalPastePrompt;
    std::function<void()> beginPatternCreatePrompt;
    std::function<void()> beginPatternClonePrompt;
    std::function<void()> deleteActivePattern;
    std::function<void(bool)> setSynthWindowVisible;
    std::function<bool(int, bool)> selectPatternIndex;
    std::function<bool(int, bool)> selectOrderIndex;
    std::function<void()> insertOrderAtSelection;
    std::function<void()> appendOrderFromActivePattern;
    std::function<void()> removeSelectedOrder;
    std::function<void(double)> buildSongToTargetSeconds;
    std::function<void(double)> trimSongToTargetSeconds;
    std::function<void(const std::string&)> runFileButtonAction;
    std::function<void()> openInstrumentBrowser;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
    std::function<std::vector<int>(const AppSessionSnapshot&)> filteredInstrumentIndices;
    std::function<void(int)> ensureSynthKeyboardShowsMidi;
    std::function<void(const std::string&, int)> runTrackMetadataAction;
};

GuiMainModalMouseContext makeMainModalMouseContextFromWindowState(
    const GuiMainWindowInteractionAdapterContext& context,
    int playbackSampleRate);

GuiMainWheelContext makeMainWheelContextFromWindowState(
    const GuiMainWindowInteractionAdapterContext& context,
    int button,
    int mx,
    int my,
    unsigned int stateMask);

GuiMainGridClickContext makeMainGridClickContextFromWindowState(
    const GuiMainWindowInteractionAdapterContext& context,
    int button,
    int mx,
    int my,
    bool altDown,
    bool shiftDown);

GuiMainMotionContext makeMainMotionContextFromWindowState(
    const GuiMainWindowInteractionAdapterContext& context,
    int mx,
    int my,
    unsigned int stateMask);

void syncInstrumentBrowserSelectionFromArmedFromWindowState(const GuiMainWindowInteractionAdapterContext& context);

GuiMainKeyModalContext makeMainKeyModalContextFromWindowState(
    const GuiMainWindowInteractionAdapterContext& context,
    KeySym key,
    bool ctrlDown,
    bool shiftDown,
    bool altDown,
    int lookupCount,
    const char* lookupBuffer,
    const std::function<bool(KeySym)>& keyMatches,
    const std::function<int()>& resolvedDigit,
    int playbackSampleRate);

GuiMainKeyEditContext makeMainKeyEditContextFromWindowState(
    const GuiMainWindowInteractionAdapterContext& context,
    KeySym key,
    bool ctrlDown,
    bool shiftDown,
    bool altDown,
    const std::function<bool(KeySym)>& keyMatches,
    const std::function<int()>& resolvedDigit);

GuiMainKeyCommandContext makeMainKeyCommandContextFromWindowState(
    const GuiMainWindowInteractionAdapterContext& context,
    KeySym key,
    bool ctrlDown,
    bool shiftDown,
    bool altDown,
    const std::function<bool(KeySym)>& keyMatches);

GuiMainKeyNoteContext makeMainKeyNoteContextFromWindowState(
    const GuiMainWindowInteractionAdapterContext& context,
    KeySym key,
    unsigned int keycode,
    bool ctrlDown,
    bool shiftDown,
    bool altDown,
    const std::function<bool(KeySym)>& keyMatches,
    const std::function<int()>& resolvedDigit);

GuiMainButtonPressContextFactoryInput makeButtonPressFactoryInputFromWindowState(
    const GuiMainWindowInteractionAdapterContext& context);

} // namespace arachno
