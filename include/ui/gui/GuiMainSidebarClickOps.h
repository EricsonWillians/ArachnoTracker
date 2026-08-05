#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ui/gui/GuiWindowTypes.h"

namespace arachno {

struct GuiMainSidebarClickResult {
    bool consumed = false;
    bool needsRedraw = false;
};

struct GuiMainSidebarClickContext {
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
    UiRect& legatoButton;
    UiRect& followPlaybackButton;

    bool& draggingPatternRows;
    int& patternResizeAnchorY;
    int& patternResizeStartRows;
    int activePatternRows = 64;
    bool& stepAdvance;
    bool& followPlayback;
    int& paintNoteMidi;

    std::function<void(int)> applyOctaveHit;
    std::function<void(int)> paintCursorMidiWithStepAdvance;
    std::function<void(const std::string&, int)> runTrackMetadataAction;
    std::function<void(int)> resizePatternRows;
    std::function<void(const std::string&)> handleSongLengthRole;
    std::function<void(const std::string&)> handleMidiImportRole;
    std::function<void(const std::string&)> handleInstrumentControlRole;
    std::function<void(int)> selectInstrument;
    std::function<void()> toggleLegatoInput;
};

GuiMainSidebarClickResult handleMainSidebarLeftClick(const GuiMainSidebarClickContext& context);

} // namespace arachno
