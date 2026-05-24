#include "Instrument.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace arachno {

namespace {
std::string normalizeParameterName(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
        if (ch == '-') {
            return '_';
        }
        return static_cast<char>(std::tolower(ch));
    });
    return name;
}
} // namespace

bool setSynthPatchParameter(SynthPatch& patch, const std::string& parameter, double value) {
    const std::string name = normalizeParameterName(parameter);

    if (name == "mix" || name == "osc_mix" || name == "oscillator_mix") {
        patch.oscillatorMix = value;
    } else if (name == "detune" || name == "detune_cents") {
        patch.detuneCents = value;
    } else if (name == "sub" || name == "sub_oscillator") {
        patch.subOscillator = value;
    } else if (name == "noise") {
        patch.noise = value;
    } else if (name == "cutoff") {
        patch.cutoff = value;
    } else if (name == "resonance") {
        patch.resonance = value;
    } else if (name == "filter_env" || name == "filter_envelope") {
        patch.filterEnvelopeAmount = value;
    } else if (name == "lfo_rate") {
        patch.lfoRate = value;
    } else if (name == "vibrato" || name == "vibrato_cents") {
        patch.vibratoCents = value;
    } else if (name == "tremolo" || name == "tremolo_depth") {
        patch.tremoloDepth = value;
    } else if (name == "drive") {
        patch.drive = value;
    } else if (name == "gain") {
        patch.gain = value;
    } else if (name == "pan") {
        patch.pan = value;
    } else if (name == "attack" || name == "amp_attack") {
        patch.ampEnvelope.attack = value;
    } else if (name == "decay" || name == "amp_decay") {
        patch.ampEnvelope.decay = value;
    } else if (name == "sustain" || name == "amp_sustain") {
        patch.ampEnvelope.sustain = value;
    } else if (name == "release" || name == "amp_release") {
        patch.ampEnvelope.release = value;
    } else if (name == "filter_attack") {
        patch.filterEnvelope.attack = value;
    } else if (name == "filter_decay") {
        patch.filterEnvelope.decay = value;
    } else if (name == "filter_sustain") {
        patch.filterEnvelope.sustain = value;
    } else if (name == "filter_release") {
        patch.filterEnvelope.release = value;
    } else {
        return false;
    }

    return true;
}

} // namespace arachno
