// Pattern.h
#ifndef PATTERN_H
#define PATTERN_H

#include "PatternRow.h"
#include <vector>

class Pattern {
public:
    Pattern(int numRows, int numTracks) {
        rows.resize(numRows, PatternRow(numTracks)); // Initialize pattern with rows
    }

    PatternRow& getRow(int index) {
        return rows.at(index); // Get a specific row by index
    }

    int getNumRows() const {
        return rows.size(); // Get the number of rows in the pattern
    }

private:
    std::vector<PatternRow> rows; // Collection of pattern rows
};

#endif // PATTERN_H
