#pragma once

#include <vector>
#include "PatternStep.h"

class PatternRow {
public:
    void addStep(PatternStep* step) { steps.push_back(step); }
    const std::vector<PatternStep*>& getSteps() const { return steps; }

private:
    std::vector<PatternStep*> steps;
};
