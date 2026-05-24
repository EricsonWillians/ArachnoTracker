#pragma once

#include <string>

#include "Tracker.h"

namespace arachno {

constexpr int projectFileVersion = 1;

void saveProject(const Song& song, const std::string& path);
Song loadProject(const std::string& path);

} // namespace arachno
