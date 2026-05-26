#pragma once

#include <string>

#include "Instrument.h"
#include "PatternStep.h"

namespace arachno {

struct StepSynthesisState {
    Note note;
    SynthPatch patch;
    double gateRows = 0.88;
    double microOffsetRows = 0.0;
    double pan = 0.0;
    bool muted = false;
};

bool applyStepSynthesisState(const PatternStep& step, StepSynthesisState& state);
bool isKnownStepEffectCommand(const EffectCommand& effect);
std::string normalizeStepEffectName(const std::string& name);

} // namespace arachno
