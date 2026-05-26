#pragma once

#include <string>
#include <vector>

#include "PatternEditor.h"
#include "PatternView.h"
#include "Tracker.h"

namespace arachno {

struct EditorStatus {
    std::string title;
    double bpm = 0.0;
    int rowsPerBeat = 0;
    int totalRows = 0;
    double durationSeconds = 0.0;
    int activePattern = 0;
    std::string activePatternName;
    int cursorRow = 0;
    int cursorTrack = 0;
    std::string cursorTrackName;
    int selectionStartRow = 0;
    int selectionStartTrack = 0;
    int selectionRows = 1;
    int selectionTracks = 1;
    bool canUndo = false;
    bool canRedo = false;
};

struct PatternSummary {
    int index = 0;
    std::string name;
    int rowCount = 0;
    int trackCount = 0;
    int orderUseCount = 0;
    bool active = false;
};

struct OrderSlotSummary {
    int index = 0;
    int pattern = -1;
    std::string patternName;
    int startRow = 0;
    int rowCount = 0;
    bool missing = false;
    bool activePattern = false;
};

struct TrackStripSummary {
    int index = 0;
    std::string name;
    double volume = 0.0;
    double pan = 0.0;
    bool muted = false;
    bool solo = false;
    bool active = false;
    int noteCount = 0;
    int automatedStepCount = 0;
};

struct InstrumentSummary {
    int index = 0;
    std::string name;
    std::string oscillatorA;
    std::string oscillatorB;
    std::string oscillatorC;
    std::string oscillatorD;
    double cutoff = 0.0;
    double drive = 0.0;
    double gain = 0.0;
    int noteUseCount = 0;
    bool active = false;
};

struct AutomationValueSummary {
    std::string parameter;
    double value = 0.0;
};

struct EffectValueSummary {
    std::string name;
    std::vector<AutomationValueSummary> parameters;
};

struct ActiveStepSummary {
    int row = 0;
    int track = 0;
    std::string trackName;
    bool hasNote = false;
    std::string noteName;
    int midiNote = -1;
    float velocity = 0.0f;
    int instrument = -1;
    std::string instrumentName;
    double gateRows = 0.0;
    double microOffsetRows = 0.0;
    bool hasProbability = false;
    double probability = 1.0;
    int retriggerCount = 1;
    double retriggerSpacingRows = 0.25;
    double retriggerVelocityDecay = 0.85;
    std::vector<AutomationValueSummary> automation;
    std::vector<EffectValueSummary> effects;
};

struct SelectionSummary {
    int startRow = 0;
    int startTrack = 0;
    int rowCount = 1;
    int trackCount = 1;
    int stepCount = 1;
    int noteCount = 0;
    int automatedStepCount = 0;
    bool containsCursor = true;
};

struct ClipboardSummary {
    bool available = false;
    int rowCount = 0;
    int trackCount = 0;
};

struct EditorViewModel {
    EditorStatus status;
    std::vector<PatternSummary> patterns;
    std::vector<OrderSlotSummary> order;
    std::vector<TrackStripSummary> tracks;
    std::vector<InstrumentSummary> instruments;
    ActiveStepSummary activeStep;
    SelectionSummary selection;
    ClipboardSummary clipboard;
    PatternGrid activeGrid;
};

EditorViewModel buildEditorViewModel(
    const Song& song,
    const PatternEditorSession& editor,
    int gridStartRow = 0,
    int gridRowCount = -1);

} // namespace arachno
