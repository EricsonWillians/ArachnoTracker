#include "PatchIO.h"

#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace arachno {

namespace {
void expectToken(std::istream& in, const std::string& expected) {
    std::string token;
    in >> token;
    if (token != expected) {
        throw std::runtime_error("expected token '" + expected + "', got '" + token + "'");
    }
}

template <typename T>
T readValue(std::istream& in, const std::string& field) {
    T value {};
    if (!(in >> value)) {
        throw std::runtime_error("failed to read " + field);
    }
    return value;
}

std::string readQuoted(std::istream& in, const std::string& field) {
    std::string value;
    if (!(in >> std::quoted(value))) {
        throw std::runtime_error("failed to read " + field);
    }
    return value;
}
} // namespace

void savePatch(const SynthPatch& patch, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("failed to open patch for writing: " + path);
    }

    out << "arachno_patch " << patchFileVersion << "\n";
    out << "name " << std::quoted(patch.name) << "\n";
    out << "oscillators " << waveformName(patch.oscillatorA)
        << " " << waveformName(patch.oscillatorB) << "\n";
    out << "params"
        << " " << patch.oscillatorMix
        << " " << patch.detuneCents
        << " " << patch.subOscillator
        << " " << patch.noise
        << " " << patch.cutoff
        << " " << patch.resonance
        << " " << patch.filterEnvelopeAmount
        << " " << patch.lfoRate
        << " " << patch.vibratoCents
        << " " << patch.tremoloDepth
        << " " << patch.drive
        << " " << patch.gain
        << " " << patch.pan
        << "\n";
    out << "amp"
        << " " << patch.ampEnvelope.attack
        << " " << patch.ampEnvelope.decay
        << " " << patch.ampEnvelope.sustain
        << " " << patch.ampEnvelope.release
        << "\n";
    out << "filter"
        << " " << patch.filterEnvelope.attack
        << " " << patch.filterEnvelope.decay
        << " " << patch.filterEnvelope.sustain
        << " " << patch.filterEnvelope.release
        << "\n";
    out << "end_patch\n";
}

SynthPatch loadPatch(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("failed to open patch for reading: " + path);
    }

    expectToken(in, "arachno_patch");
    const int version = readValue<int>(in, "patch version");
    if (version != patchFileVersion) {
        throw std::runtime_error("unsupported patch version: " + std::to_string(version));
    }

    SynthPatch patch;
    expectToken(in, "name");
    patch.name = readQuoted(in, "patch name");

    expectToken(in, "oscillators");
    patch.oscillatorA = waveformFromName(readValue<std::string>(in, "oscillator A"));
    patch.oscillatorB = waveformFromName(readValue<std::string>(in, "oscillator B"));

    expectToken(in, "params");
    patch.oscillatorMix = readValue<double>(in, "oscillator mix");
    patch.detuneCents = readValue<double>(in, "detune");
    patch.subOscillator = readValue<double>(in, "sub oscillator");
    patch.noise = readValue<double>(in, "noise");
    patch.cutoff = readValue<double>(in, "cutoff");
    patch.resonance = readValue<double>(in, "resonance");
    patch.filterEnvelopeAmount = readValue<double>(in, "filter envelope amount");
    patch.lfoRate = readValue<double>(in, "lfo rate");
    patch.vibratoCents = readValue<double>(in, "vibrato");
    patch.tremoloDepth = readValue<double>(in, "tremolo");
    patch.drive = readValue<double>(in, "drive");
    patch.gain = readValue<double>(in, "gain");
    patch.pan = readValue<double>(in, "pan");

    expectToken(in, "amp");
    patch.ampEnvelope.attack = readValue<double>(in, "amp attack");
    patch.ampEnvelope.decay = readValue<double>(in, "amp decay");
    patch.ampEnvelope.sustain = readValue<double>(in, "amp sustain");
    patch.ampEnvelope.release = readValue<double>(in, "amp release");

    expectToken(in, "filter");
    patch.filterEnvelope.attack = readValue<double>(in, "filter attack");
    patch.filterEnvelope.decay = readValue<double>(in, "filter decay");
    patch.filterEnvelope.sustain = readValue<double>(in, "filter sustain");
    patch.filterEnvelope.release = readValue<double>(in, "filter release");

    expectToken(in, "end_patch");
    return patch;
}

} // namespace arachno
