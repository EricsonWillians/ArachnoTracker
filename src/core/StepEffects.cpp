#include "StepEffects.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

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

std::optional<double> findParameterValue(
    const EffectCommand& effect,
    const std::vector<std::string>& names) {
    for (const auto& [rawName, rawValue] : effect.parameters) {
        const std::string normalized = lowerCopy(rawName);
        for (const std::string& candidate : names) {
            if (normalized == candidate) {
                return rawValue;
            }
        }
    }
    return std::nullopt;
}

double normalizedToRange(double value, double minimum, double maximum) {
    if (value >= 0.0 && value <= 1.0) {
        return minimum + (maximum - minimum) * value;
    }
    return std::clamp(value, minimum, maximum);
}

bool applyChorusEffectAlias(const EffectCommand& effect, StepSynthesisState& state) {
    bool touched = false;
    if (const auto mix = findParameterValue(effect, {"value", "mix", "amount", "wet"}); mix.has_value()) {
        state.patch.chorusMix = clamp01(*mix);
        touched = true;
    }
    if (const auto rate = findParameterValue(effect, {"rate", "speed"}); rate.has_value()) {
        state.patch.chorusRate = normalizedToRange(*rate, 0.05, 5.0);
        touched = true;
    }
    if (const auto depth = findParameterValue(effect, {"depth"}); depth.has_value()) {
        state.patch.chorusDepth = clamp01(*depth);
        touched = true;
    }
    if (touched) {
        state.patch.chorusEnabled = state.patch.chorusMix > 0.0001;
    }
    return touched;
}

bool applyDelayEffectAlias(const EffectCommand& effect, StepSynthesisState& state) {
    bool touched = false;
    if (const auto mix = findParameterValue(effect, {"value", "mix", "amount", "wet"}); mix.has_value()) {
        state.patch.combMix = clamp01(*mix);
        state.patch.delayMix = clamp01(*mix);
        touched = true;
    }
    if (const auto time = findParameterValue(effect, {"time", "delay_time", "seconds"}); time.has_value()) {
        state.patch.combTime = normalizedToRange(*time, 0.01, 0.55);
        state.patch.delayTime = normalizedToRange(*time, 0.0, 1.0);
        touched = true;
    }
    if (const auto feedback = findParameterValue(effect, {"feedback", "fb"}); feedback.has_value()) {
        state.patch.combFeedback = clamp01(*feedback);
        state.patch.delayFeedback = clamp01(*feedback);
        touched = true;
    }
    if (const auto tone = findParameterValue(effect, {"tone", "damping", "damp"}); tone.has_value()) {
        const double damp = clamp01(*tone);
        state.patch.cutoff = std::clamp(0.98 - damp * 0.62, 0.2, 1.0);
        state.patch.delayTone = 1.0 - damp;
        touched = true;
    }
    return touched;
}

bool applyReverbEffectAlias(const EffectCommand& effect, StepSynthesisState& state) {
    bool touched = false;
    const double mix = clamp01(findParameterValue(effect, {"value", "mix", "amount", "wet"}).value_or(0.0));
    const double size = clamp01(findParameterValue(effect, {"size", "room"}).value_or(mix));
    const double damp = clamp01(findParameterValue(effect, {"damping", "damp"}).value_or(0.45));
    const double width = clamp01(findParameterValue(effect, {"width", "stereo"}).value_or(0.6));

    if (findParameterValue(effect, {"value", "mix", "amount", "wet"}).has_value()) {
        state.patch.combMix = std::max(state.patch.combMix, mix * 0.78);
        state.patch.reverbMix = std::max(state.patch.reverbMix, mix);
        state.patch.chorusMix = std::max(state.patch.chorusMix, mix * 0.46);
        state.patch.chorusEnabled = state.patch.chorusMix > 0.0001;
        touched = true;
    }
    if (findParameterValue(effect, {"size", "room"}).has_value()) {
        state.patch.combTime = normalizedToRange(size, 0.03, 0.8);
        state.patch.reverbSize = size;
        state.patch.reverbPreDelay = std::clamp(size * 0.55, 0.0, 1.0);
        touched = true;
    }
    if (findParameterValue(effect, {"damping", "damp"}).has_value()) {
        state.patch.cutoff = std::clamp(0.96 - damp * 0.68, 0.18, 1.0);
        state.patch.toneTilt = std::clamp(state.patch.toneTilt - damp * 0.32, -1.0, 1.0);
        state.patch.reverbDamping = damp;
        touched = true;
    }
    if (findParameterValue(effect, {"width", "stereo"}).has_value()) {
        state.patch.stereoSpread = std::max(state.patch.stereoSpread, width);
        touched = true;
    }
    if (touched) {
        state.patch.combFeedback = std::clamp(
            std::max(state.patch.combFeedback, 0.35 + size * 0.52) * (0.76 + mix * 0.24),
            0.0,
            0.985);
        state.patch.chorusDepth = std::clamp(
            std::max(state.patch.chorusDepth, 0.12 + mix * 0.52 + size * 0.2),
            0.0,
            1.0);
        state.patch.chorusRate = std::clamp(0.1 + (1.0 - damp) * 0.45, 0.05, 5.0);
    }
    return touched;
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
        || name == "mute"
        || name == "chorus"
        || name == "delay"
        || name == "reverb") {
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
        if (name == "chorus") {
            if (!applyChorusEffectAlias(effect, state)) {
                return false;
            }
            continue;
        }
        if (name == "delay") {
            if (!applyDelayEffectAlias(effect, state)) {
                return false;
            }
            continue;
        }
        if (name == "reverb") {
            if (!applyReverbEffectAlias(effect, state)) {
                return false;
            }
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
