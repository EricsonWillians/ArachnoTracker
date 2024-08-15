// PatternStep.h
#ifndef PATTERN_STEP_H
#define PATTERN_STEP_H

#include "Note.h"
#include <vector>

class PatternStep {
public:
    std::vector<Note> notes; // Multiple notes per step (polyphony)

    void addNote(const Note& note) {
        notes.push_back(note);
    }
};

#endif // PATTERN_STEP_H
