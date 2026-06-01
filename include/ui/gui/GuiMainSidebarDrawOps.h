#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "AppActions.h"
#include "ApplicationSession.h"
#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainSidebarDrawContext {
    int sidebarLeft = 0;
    int sidebarWidth = 0;
    int gridTop = 0;
    int gridHeight = 0;

    unsigned long colorText = 0;
    unsigned long colorMutedText = 0;
    unsigned long colorSelection = 0;
    unsigned long colorGridLine = 0;
    unsigned long colorButtonActive = 0;
    unsigned long pianoWhite = 0;
    unsigned long pianoBlack = 0;

    const AppSessionSnapshot& snapshot;
    const AppActionResult& lastAction;

    int paintNoteMidi = 0;
    int armedOctave = 0;
    int armedInstrument = 0;
    int activePatternRows = 64;
    bool stepAdvance = true;
    bool followPlayback = true;
    double targetSongLengthMinutes = 0.0;
    int midiImportRowsPerBeat = 4;
    int midiImportPatternRows = 64;
    bool midiImportSplitByTrack = true;

    int& instrumentListVisibleRows;
    int& instrumentListStart;
    std::function<void()> clampInstrumentListWindow;

    std::vector<std::pair<UiRect, int>>& octaveHitTargets;
    std::vector<PianoKeyHit>& pianoKeyHits;
    UiRect& patternRowsMinus;
    UiRect& patternRowsValue;
    UiRect& patternRowsPlus;
    UiRect& stepAdvanceButton;
    UiRect& followPlaybackButton;
    std::vector<TrackMetadataHit>& trackMetadataHits;
    std::vector<SongLengthHit>& songLengthHits;
    std::vector<MidiImportSettingHit>& midiImportSettingHits;
    std::vector<std::pair<UiRect, std::string>>& instrumentControlHits;
    std::vector<std::pair<UiRect, int>>& instrumentHitTargets;
    UiRect& instrumentListRect;

    std::function<void(int, int, int, int, unsigned long)> drawFilledRect;
    std::function<void(int, int, int, int, unsigned long)> drawRect;
    std::function<void(int, int, const std::string&, unsigned long)> drawText;
    std::function<void(const UiRect&, const std::string&, bool)> drawButton;
};

int drawMainSidebarSections(const GuiMainSidebarDrawContext& context, int sy);

} // namespace arachno
