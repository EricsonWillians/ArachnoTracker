#pragma once

#include <functional>
#include <vector>

#include "ProjectLifecycle.h"
#include "ui/gui/GuiMainButtonPressContextFactoryOps.h"
#include "ui/gui/GuiMainEventContextFactoryOps.h"
#include "ui/gui/GuiMainKeyCommandContextFactoryOps.h"
#include "ui/gui/GuiMainKeyEditContextFactoryOps.h"
#include "ui/gui/GuiMainKeyModalContextFactoryOps.h"
#include "ui/gui/GuiMainKeyNoteContextFactoryOps.h"
#include "ui/gui/GuiMainPointerContextFactoryOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainModalMouseWindowAdapterInput {
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
};

GuiMainModalMouseContext makeMainModalMouseContextFromWindowAdapterState(
    const GuiMainModalMouseWindowAdapterInput& input,
    int playbackSampleRate);

struct GuiMainWheelWindowAdapterInput {
    const TrackerWindowLayout& layout;
    UiRect& sidebarViewport;
    UiRect& instrumentListRect;
    int& sidebarContentHeight;
    int& sidebarScrollOffset;
    int& viewStartRow;
    bool draggingSelection = false;
    int dragAnchorRow = 0;
    int dragAnchorTrack = 0;
    int& gridTrackStart;
    int activePatternRows = 64;

    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(int)> scrollInstrumentList;
    std::function<void(int)> resizePatternRows;
    std::function<void()> lockManualScroll;
    std::function<void()> refreshSnapshot;
    std::function<bool(int, int, int&, int&)> gridPositionToCell;
    std::function<void(int, int, int, int)> applySelectionRange;
};

GuiMainWheelContext makeMainWheelContextFromWindowAdapterState(
    const GuiMainWheelWindowAdapterInput& input,
    int button,
    int mx,
    int my,
    unsigned int stateMask);

struct GuiMainGridClickWindowAdapterInput {
    int& paintNoteMidi;
    bool& keyboardSelectionActive;
    bool& paintingNotes;
    int& lastPaintRow;
    int& lastPaintTrack;
    bool& draggingSelection;
    int& dragAnchorRow;
    int& dragAnchorTrack;

    std::function<bool(int, int, int&, int&)> gridPositionToCell;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(int, int)> moveCursor;
    std::function<void(int, int, int)> paintNoteAt;
    std::function<void()> refreshSnapshot;
    std::function<void(const std::string&)> runActionById;
    std::function<void(int, int, int, int)> applySelectionRange;
};

GuiMainGridClickContext makeMainGridClickContextFromWindowAdapterState(
    const GuiMainGridClickWindowAdapterInput& input,
    int button,
    int mx,
    int my,
    bool altDown,
    bool shiftDown);

struct GuiMainMotionWindowAdapterInput {
    int& pointerX;
    int& pointerY;
    int windowWidth = 1280;
    int windowHeight = 800;
    const TrackerWindowLayout& layout;
    bool resizingSidebar = false;
    bool resizingTopPanel = false;
    bool& draggingPatternRows;
    bool& paintingNotes;
    bool draggingSelection = false;
    int& patternResizeAnchorY;
    int patternResizeStartRows = 64;
    int& topPanelHeightState;
    int& sidebarWidthState;
    int& paintNoteMidi;
    int& lastPaintRow;
    int& lastPaintTrack;
    int dragAnchorRow = 0;
    int dragAnchorTrack = 0;
    int& viewStartRow;
    int& hoveredTrackHeader;
    const std::vector<TrackHeaderHit>& trackHeaderHits;

    std::function<void(int)> resizePatternRows;
    std::function<bool(int, int, int&, int&)> gridPositionToCell;
    std::function<void(int, int, int)> paintNoteAt;
    std::function<void()> refreshSnapshot;
    std::function<void(int, int, int, int)> applySelectionRange;
    std::function<void()> lockManualScroll;
};

GuiMainMotionContext makeMainMotionContextFromWindowAdapterState(
    const GuiMainMotionWindowAdapterInput& input,
    int mx,
    int my,
    unsigned int stateMask);

struct GuiMainInstrumentBrowserSyncInput {
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<std::vector<int>(const AppSessionSnapshot&)> filteredInstrumentIndices;
    int armedInstrument = 0;
    int& instrumentBrowserSelected;
};

void syncInstrumentBrowserSelectionFromArmedInWindow(const GuiMainInstrumentBrowserSyncInput& input);

struct GuiMainKeyModalWindowAdapterInput {
    bool unsavedPromptActive = false;
    std::function<void(UnsavedChangesChoice)> resolveUnsavedPrompt;
    bool& audioTuningDialogActive;
    std::function<void(int, int)> adjustAudioCustomLevel;
    std::function<void(AudioPerformanceMode, int)> setAudioPerformanceMode;

    InlinePromptState& inlinePrompt;
    bool synthWindowVisible = false;
    std::function<bool(InlinePromptKind)> isSynthInlinePromptKind;
    std::function<bool(InlinePromptKind)> inlinePromptUsesFileBrowser;
    int& fileBrowserSelected;
    int& fileBrowserScroll;
    UiRect& fileBrowserListRect;
    std::vector<FileBrowserEntry>& fileBrowserEntries;
    std::filesystem::path& fileBrowserDirectory;
    std::function<void()> refreshFileBrowserEntries;
    std::function<void()> executeInlinePrompt;
    std::function<void()> cancelInlinePrompt;

    bool& instrumentBrowserActive;
    int& instrumentBrowserSelected;
    int& instrumentBrowserScroll;
    std::string& instrumentBrowserQuery;
    std::function<void(bool)> closeInstrumentBrowser;
    std::function<void(int)> cycleInstrumentBy;
    std::function<void()> syncInstrumentBrowserSelectionFromArmed;
    std::function<int()> filteredInstrumentCount;
};

GuiMainKeyModalContext makeMainKeyModalContextFromWindowAdapterState(
    const GuiMainKeyModalWindowAdapterInput& input,
    KeySym key,
    bool ctrlDown,
    bool shiftDown,
    bool altDown,
    int lookupCount,
    const char* lookupBuffer,
    const std::function<bool(KeySym)>& keyMatches,
    const std::function<int()>& resolvedDigit,
    int playbackSampleRate);

struct GuiMainKeyEditWindowAdapterInput {
    bool synthWindowVisible = false;
    bool& stepAdvance;
    float& defaultVelocity;
    int& armedOctave;
    int& synthPreviewMidi;
    int& paintNoteMidi;
    int activePatternRows = 64;
    int requestedRowCount = 16;
    int& viewStartRow;
    bool& keyboardSelectionActive;
    int& keyboardSelectionAnchorRow;
    int& keyboardSelectionAnchorTrack;

    std::function<void(int)> setArmedOctave;
    std::function<void()> applyArmedOctaveToSelection;
    std::function<void(int)> ensureSynthKeyboardShowsMidi;
    std::function<void(int)> cycleInstrumentBy;
    std::function<void(int)> selectInstrument;
    std::function<void()> auditionArmedInstrument;
    std::function<void(int, int)> moveCursor;
    std::function<void(int, int, int)> paintNoteAt;
    std::function<void()> refreshSnapshot;
    std::function<void(int)> ensurePatternRowsForRow;
    std::function<void(const std::string&)> runActionById;
    std::function<void(int, int, int, int)> applySelectionRange;
    std::function<void()> lockManualScroll;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<AppActionResult(const AppActionRequest&)> runAction;
};

GuiMainKeyEditContext makeMainKeyEditContextFromWindowAdapterState(
    const GuiMainKeyEditWindowAdapterInput& input,
    KeySym key,
    bool ctrlDown,
    bool shiftDown,
    bool altDown,
    const std::function<bool(KeySym)>& keyMatches,
    const std::function<int()>& resolvedDigit);

struct GuiMainKeyCommandWindowAdapterInput {
    bool& audioTuningDialogActive;
    bool& midiImportSplitByTrack;
    bool synthWindowVisible = false;
    double targetSongLengthMinutes = 0.0;
    GuiThemeMode& themeMode;
    std::function<PlaybackSnapshot()> playbackSnapshot;
    int selectedOrderIndex = 0;

    std::function<void(AudioPerformanceMode, int)> setAudioPerformanceMode;
    std::function<AudioPerformanceMode()> currentPerformanceMode;
    std::function<void(const std::string&)> runActionById;
    std::function<void(const std::string&)> runSyncById;
    std::function<void()> runEvents;
    std::function<void()> openInstrumentBrowser;
    std::function<void(int)> cycleInstrumentBy;
    std::function<void(const std::string&)> runFileButtonAction;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(
        InlinePromptKind,
        const std::string&,
        const std::string&,
        const std::string&,
        int,
        int)> beginInlinePrompt;
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
    std::function<AppActionResult(const AppActionRequest&)> runAction;
};

GuiMainKeyCommandContext makeMainKeyCommandContextFromWindowAdapterState(
    const GuiMainKeyCommandWindowAdapterInput& input,
    KeySym key,
    bool ctrlDown,
    bool shiftDown,
    bool altDown,
    const std::function<bool(KeySym)>& keyMatches);

struct GuiMainKeyNoteWindowAdapterInput {
    bool synthWindowVisible = false;
    int& armedOctave;
    int& armedInstrument;
    int& synthPreviewMidi;
    int& paintNoteMidi;
    bool stepAdvance = true;
    float defaultVelocity = 0.8f;

    std::function<void(int)> setArmedOctave;
    std::function<void(int)> ensureSynthKeyboardShowsMidi;
    std::function<bool(unsigned int, int)> claimSynthPreviewKey;
    std::function<void(int)> auditionSynthPreviewMidi;
    std::function<void(int)> selectInstrument;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(int)> ensurePatternRowsForRow;
    std::function<AppActionResult(const AppActionRequest&, bool)> runActionWithRefresh;
};

GuiMainKeyNoteContext makeMainKeyNoteContextFromWindowAdapterState(
    const GuiMainKeyNoteWindowAdapterInput& input,
    KeySym key,
    unsigned int keycode,
    bool ctrlDown,
    bool shiftDown,
    bool altDown,
    const std::function<bool(KeySym)>& keyMatches,
    const std::function<int()>& resolvedDigit);

struct GuiMainButtonPressWindowAdapterInput {
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

GuiMainButtonPressContextFactoryInput makeButtonPressFactoryInputFromWindowAdapterState(
    const GuiMainButtonPressWindowAdapterInput& input);

} // namespace arachno
