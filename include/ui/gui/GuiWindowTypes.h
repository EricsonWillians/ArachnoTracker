#pragma once

#include <filesystem>
#include <string>

#include "AppActions.h"
#include "ui/gui/GuiAudioRuntime.h"

namespace arachno {

enum class GuiThemeMode {
    Dos,
    HighContrast
};

struct TrackerWindowLayout {
    int margin = 12;
    int topPanelHeight = 170;
    int sidebarWidth = 300;
    int gridLeft = 12;
    int gridTop = 182;
    int gridWidth = 800;
    int gridHeight = 560;
    int sidebarLeft = 824;
    int sidebarHeight = 560;
    int rowNumberWidth = 56;
    int trackCols = 1;
    int trackWidth = 84;
    int rowHeight = 18;
    int visibleRows = 16;
    int verticalSplitterX = 820;
    int horizontalSplitterY = 180;
};

struct UiRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool contains(int px, int py) const {
        return px >= x && py >= y && px < x + width && py < y + height;
    }
};

struct TrackHeaderHit {
    UiRect selectRect;
    UiRect muteRect;
    UiRect soloRect;
    int track = 0;
    std::string trackName;
    bool nameTruncated = false;
};

struct OrderSlotHit {
    UiRect rect;
    int index = -1;
    int pattern = -1;
    int startRow = 0;
    bool valid = false;
};

struct PianoKeyHit {
    UiRect rect;
    int midiNote = 60;
    bool black = false;
};

enum class InlinePromptKind {
    Inactive,
    OpenProjectPath,
    SaveProjectPath,
    ExportMixdownPath,
    ImportMidiPath,
    ImportPatchAsNewPath,
    ImportPatchReplacePath,
    ImportPatchReplaceAllPath,
    ExportPatchPath,
    RenameInstrument,
    RenameTrack,
    SongLengthMinutes,
    PatternCreateSpec,
    PatternCloneName,
    TemporalPasteSpec
};

struct InlinePromptState {
    InlinePromptKind kind = InlinePromptKind::Inactive;
    bool active = false;
    std::string title;
    std::string hint;
    std::string value;
    int targetTrack = -1;
    int targetInstrument = -1;
};

struct FileBrowserEntry {
    std::string name;
    std::filesystem::path path;
    bool directory = false;
};

struct FileBrowserHit {
    UiRect rect;
    std::string role;
    int index = -1;
};

struct UnsavedDecisionPromptState {
    bool active = false;
    std::string title;
    std::string detail;
    AppActionRequest request;
};

struct TrackMetadataHit {
    UiRect rect;
    std::string role;
    int track = 0;
};

struct SongLengthHit {
    UiRect rect;
    std::string role;
};

struct MidiImportSettingHit {
    UiRect rect;
    std::string role;
};

struct SynthWindowHit {
    UiRect rect;
    std::string kind;
    std::string parameter;
    std::string oscillator;
    std::string wave;
    double delta = 0.0;
    double value = 0.0;
};

struct AudioTuningDialogHit {
    UiRect rect;
    std::string role;
    AudioPerformanceMode mode = AudioPerformanceMode::Auto;
    int delta = 0;
};

} // namespace arachno
