#pragma once

#include <functional>

#include "ui/gui/GuiMainModalMouseOps.h"
#include "ui/gui/GuiMainMotionOps.h"

namespace arachno {

struct GuiMainModalMouseContextFactoryInput {
    int playbackSampleRate = 48000;

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

GuiMainModalMouseContext makeMainModalMouseContextFromState(const GuiMainModalMouseContextFactoryInput& input);

struct GuiMainMotionContextFactoryInput {
    int mx = 0;
    int my = 0;
    unsigned int stateMask = 0;

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

GuiMainMotionContext makeMainMotionContextFromState(const GuiMainMotionContextFactoryInput& input);

} // namespace arachno
