#pragma once

#include <string>

#include "Instrument.h"

namespace arachno {

constexpr int patchFileVersion = 1;

void savePatch(const SynthPatch& patch, const std::string& path);
SynthPatch loadPatch(const std::string& path);

} // namespace arachno
