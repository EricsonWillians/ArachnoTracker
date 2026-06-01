#pragma once

#include <string>
#include <vector>

#include "Instrument.h"

namespace arachno {

struct SynthParamDef {
    std::string name;
    std::string label;
    double minimum = 0.0;
    double maximum = 1.0;
    double step = 0.01;
};

const std::vector<SynthParamDef>& synthParamDefinitions();
bool synthParamBelongsToPage(const std::string& name, int page);
double getSynthParameterValue(const SynthPatch& patch, const std::string& name);
std::string oscTargetName(int target);
std::string mapSynthParameterToOscTarget(const std::string& base, int target);
std::string synthParamBaseName(const std::string& name);
const SynthParamDef* findSynthParamDef(const std::string& name);
double clampQuantizedSynthParamValue(const SynthParamDef& def, double value);
std::string synthParamTooltipText(const std::string& name);

} // namespace arachno

