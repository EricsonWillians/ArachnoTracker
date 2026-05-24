#pragma once

#include <map>
#include <optional>
#include <string>

#include "Note.h"

namespace arachno {

struct EffectCommand {
    std::string name;
    std::map<std::string, double> parameters;
};

struct PatternStep {
    std::optional<Note> note;
    int instrument = 0;
    double gate = 0.88;
    double microOffsetRows = 0.0;
    std::optional<double> probability;
    std::map<std::string, double> automation;

    bool empty() const { return !note.has_value() && automation.empty(); }
};

} // namespace arachno
