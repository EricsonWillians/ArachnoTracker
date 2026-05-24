#pragma once

#include <string>

namespace arachno {

enum class Waveform {
    Sine,
    Square,
    Saw,
    Triangle,
    Noise
};

struct Envelope {
    double attack = 0.005;
    double decay = 0.08;
    double sustain = 0.72;
    double release = 0.18;
};

struct SynthPatch {
    std::string name = "Init";
    Waveform oscillatorA = Waveform::Saw;
    Waveform oscillatorB = Waveform::Square;
    double oscillatorMix = 0.35;
    double detuneCents = 7.0;
    double subOscillator = 0.18;
    double noise = 0.02;
    double cutoff = 0.72;
    double resonance = 0.12;
    double filterEnvelopeAmount = 0.18;
    double lfoRate = 5.5;
    double vibratoCents = 0.0;
    double tremoloDepth = 0.0;
    double drive = 0.08;
    double gain = 0.55;
    double pan = 0.0;
    Envelope ampEnvelope;
    Envelope filterEnvelope;
};

struct Instrument {
    int id = 0;
    SynthPatch patch;
};

const char* waveformName(Waveform waveform);
Waveform waveformFromName(const std::string& name);
bool setSynthPatchParameter(SynthPatch& patch, const std::string& parameter, double value);

} // namespace arachno
