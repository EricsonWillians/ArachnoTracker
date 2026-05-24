#pragma once

#include <string>

#include "Tracker.h"

namespace arachno {

struct EditorCursor {
    int pattern = 0;
    int row = 0;
    int track = 0;
};

class PatternEditorSession {
public:
    explicit PatternEditorSession(Song& song);

    const EditorCursor& cursor() const { return cursor_; }

    void selectPattern(int pattern);
    void moveTo(int row, int track);
    void moveBy(int rowDelta, int trackDelta);
    void enterNote(const Note& note, float velocity = 1.0f);
    void clearStep();
    void setInstrument(int instrument);
    void setGate(double gateRows);
    void setAutomation(const std::string& parameter, double value);
    void clearAutomation(const std::string& parameter);
    void clearAllAutomation();
    void transposeCurrent(int semitones);
    void transposeTrack(int track, int semitones);
    void fillScale(
        int track,
        int startRow,
        int count,
        int stride,
        int rootMidi,
        const std::string& scaleName,
        int instrument,
        float velocity,
        double gateRows);
    void fillEuclidean(
        int track,
        int startRow,
        int steps,
        int pulses,
        int rootMidi,
        int instrument,
        float velocity,
        double gateRows);

    std::string applyCommand(const std::string& command);

private:
    Pattern& activePattern();
    const Pattern& activePattern() const;
    PatternStep& activeStep();
    void clampCursor();

    Song& song_;
    EditorCursor cursor_;
};

} // namespace arachno
