// PatternRow.h
#ifndef PATTERN_ROW_H
#define PATTERN_ROW_H

#include "PatternStep.h"
#include <vector>

class PatternRow {
public:
    std::vector<PatternStep> steps;

    PatternRow(int numTracks) {
        steps.resize(numTracks); // Initialize a row with a number of tracks
    }

    PatternStep& getStep(int index) {
        return steps.at(index);
    }
};

#endif // PATTERN_ROW_H
