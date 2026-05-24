#pragma once

#include <vector>

#include "PatternStep.h"

namespace arachno {

struct PatternRow {
    std::vector<PatternStep> steps;

    explicit PatternRow(int trackCount = 0) : steps(static_cast<std::size_t>(trackCount)) {}
};

} // namespace arachno
