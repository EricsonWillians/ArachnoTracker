#pragma once

#include <string>

#include "Tracker.h"

namespace arachno {

std::string renderPatternTable(const Song& song, int patternIndex, int startRow = 0, int rowCount = -1);

} // namespace arachno
