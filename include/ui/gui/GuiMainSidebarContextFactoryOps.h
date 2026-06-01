#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "ui/gui/GuiMainSidebarClickOps.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainSidebarContextFactoryInput {
    bool pointerInSidebar = false;
    int mx = 0;
    int my = 0;

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

    std::function<void(int)> setArmedOctave;
    std::function<void()> applyArmedOctaveToSelection;
    std::function<AppSessionSnapshot()> activeSnapshot;
    std::function<void(int, int, int)> paintNoteAt;
    std::function<void(int)> ensurePatternRowsForRow;
    std::function<void(const std::string&)> runActionById;
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
    std::function<void(const std::string&)> runFileButtonAction;
    std::function<void(int)> selectInstrument;
    std::function<void()> auditionArmedInstrument;
    std::function<void()> openInstrumentBrowser;
};

GuiMainSidebarClickContext makeMainSidebarClickContextFromState(const GuiMainSidebarContextFactoryInput& input);

} // namespace arachno
