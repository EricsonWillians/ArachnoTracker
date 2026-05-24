#pragma once

#include <string>

#include "Tracker.h"

namespace arachno {

void exportMidiFile(const Song& song, const std::string& path, int ticksPerQuarterNote = 480);

} // namespace arachno
