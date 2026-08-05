#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Note.h"

namespace arachno {

struct EffectCommand {
    std::string name;
    std::map<std::string, double> parameters;
};

struct PatternStep {
    std::optional<Note> note;
    int instrument = -1;
    double gate = 0.88;
    double microOffsetRows = 0.0;
    std::optional<double> probability;
    int retriggerCount = 1;
    double retriggerSpacingRows = 0.25;
    double retriggerVelocityDecay = 0.85;
    std::map<std::string, double> automation;
    std::vector<EffectCommand> effects;
    // Note-off step: releases the last note started on this track (rendered as "===").
    bool noteOff = false;

    bool empty() const { return !note.has_value() && !noteOff && automation.empty() && effects.empty(); }
};

} // namespace arachno
