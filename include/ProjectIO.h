#pragma once

#include <string>

#include "Tracker.h"

namespace arachno {

constexpr int projectFileVersion = 2;
constexpr int minimumProjectFileVersion = 1;

void saveProject(const Song& song, const std::string& path);
Song loadProject(const std::string& path);

} // namespace arachno
