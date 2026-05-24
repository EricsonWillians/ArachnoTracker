#pragma once

#include <string>

#include "Tracker.h"

namespace arachno {

std::string renderPatternTable(const Song& song, int patternIndex, int startRow = 0, int rowCount = -1);
std::string renderArrangementTable(const Song& song);
std::string renderInstrumentTable(const Song& song);

} // namespace arachno
