#pragma once

#include <vector>
#include "PatternRow.h"

class Pattern {
public:
    void addRow(PatternRow* row) { rows.push_back(row); }
    const std::vector<PatternRow*>& getRows() const { return rows; }

private:
    std::vector<PatternRow*> rows;
};
