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
    void transposeCurrent(int semitones);
    void transposeTrack(int track, int semitones);

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
