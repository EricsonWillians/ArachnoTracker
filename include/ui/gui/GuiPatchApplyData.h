#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Instrument.h"

namespace arachno {

struct SynthWaveAssignment {
    std::string oscillator;
    std::string wave;
};

std::vector<SynthWaveAssignment> synthPatchWaveAssignments(const SynthPatch& patch);
std::vector<std::pair<std::string, double>> synthPatchParameterAssignments(const SynthPatch& patch);

} // namespace arachno

