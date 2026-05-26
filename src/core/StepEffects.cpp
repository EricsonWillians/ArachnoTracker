#include "StepEffects.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace arachno {

namespace {
double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch == '-') {
            return '_';
        }
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool readMainValue(const EffectCommand& effect, double& value) {
    const auto valueIt = effect.parameters.find("value");
    if (valueIt != effect.parameters.end()) {
        value = valueIt->second;
        return true;
    }
    if (effect.parameters.size() == 1) {
        value = effect.parameters.begin()->second;
        return true;
    }
    return false;
}
} // namespace

std::string normalizeStepEffectName(const std::string& name) {
    return lowerCopy(name);
}

bool isKnownStepEffectCommand(const EffectCommand& effect) {
    const std::string name = normalizeStepEffectName(effect.name);
    if (name.empty()) {
        return false;
    }
    if (name == "velocity"
        || name == "velocity_set"
        || name == "vel"
        || name == "gate"
        || name == "gate_scale"
        || name == "micro"
        || name == "transpose"
        || name == "mute") {
        return true;
    }

    double value = 0.0;
    if (!readMainValue(effect, value)) {
        return false;
    }
    SynthPatch patch;
    return setSynthPatchParameter(patch, name, value);
}

bool applyStepSynthesisState(const PatternStep& step, StepSynthesisState& state) {
    for (const auto& [parameter, value] : step.automation) {
        setSynthPatchParameter(state.patch, parameter, value);
    }

    for (const EffectCommand& effect : step.effects) {
        const std::string name = normalizeStepEffectName(effect.name);
        double value = 0.0;
        const bool hasMainValue = readMainValue(effect, value);
        if (name == "velocity" || name == "vel") {
            if (!hasMainValue) {
                return false;
            }
            state.note.velocity = static_cast<float>(std::clamp(
                static_cast<double>(state.note.velocity) * value,
                0.0,
                1.0));
            continue;
        }
        if (name == "velocity_set") {
            if (!hasMainValue) {
                return false;
            }
            state.note.velocity = static_cast<float>(std::clamp(value, 0.0, 1.0));
            continue;
        }
        if (name == "gate") {
            if (!hasMainValue) {
                return false;
            }
            state.gateRows = std::max(0.001, value);
            continue;
        }
        if (name == "gate_scale") {
            if (!hasMainValue) {
                return false;
            }
            state.gateRows = std::max(0.001, state.gateRows * value);
            continue;
        }
        if (name == "micro") {
            if (!hasMainValue) {
                return false;
            }
            state.microOffsetRows += value;
            continue;
        }
        if (name == "transpose") {
            if (!hasMainValue) {
                return false;
            }
            state.note.midi = std::clamp(
                static_cast<int>(std::lround(static_cast<double>(state.note.midi) + value)),
                0,
                127);
            continue;
        }
        if (name == "mute") {
            if (!hasMainValue) {
                return false;
            }
            state.muted = value >= 0.5;
            continue;
        }
        if (name == "pan") {
            if (!hasMainValue) {
                return false;
            }
            state.pan = std::clamp(value, -1.0, 1.0);
            continue;
        }
        if (hasMainValue && setSynthPatchParameter(state.patch, name, value)) {
            continue;
        }
        return false;
    }

    state.note.velocity = static_cast<float>(clamp01(state.note.velocity));
    state.gateRows = std::max(0.001, state.gateRows);
    state.pan = std::clamp(state.pan, -1.0, 1.0);
    return true;
}

} // namespace arachno
