#pragma once

#include <string>
#include <vector>

#include "Tracker.h"

namespace arachno {

struct EditorCursor;
struct EditorSelection;

struct PatternGridCell {
    int row = 0;
    int track = 0;
    std::string trackName;
    bool hasNote = false;
    std::string noteName;
    int midiNote = -1;
    float velocity = 0.0f;
    int instrument = -1;
    double gateRows = 0.0;
    bool hasAutomation = false;
    bool hasEffects = false;
    bool selected = false;
    bool cursor = false;
};

struct PatternGrid {
    int patternIndex = 0;
    std::string patternName;
    int startRow = 0;
    int rowCount = 0;
    int trackCount = 0;
    std::vector<std::string> trackNames;
    std::vector<PatternGridCell> cells;
};

PatternGrid buildPatternGrid(
    const Song& song,
    int patternIndex,
    int startRow = 0,
    int rowCount = -1,
    const EditorCursor* cursor = nullptr,
    const EditorSelection* selection = nullptr);
std::string renderPatternTable(const Song& song, int patternIndex, int startRow = 0, int rowCount = -1);
std::string renderArrangementTable(const Song& song);
std::string renderInstrumentTable(const Song& song);
std::string renderProjectStats(const Song& song);

} // namespace arachno
