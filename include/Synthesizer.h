#pragma once

#include <cstdint>
#include <vector>

#include "Instrument.h"
#include "Note.h"

namespace arachno {

class Synthesizer {
public:
    explicit Synthesizer(double sampleRate = 48000.0);

    void setSampleRate(double sampleRate);
    void noteOn(const Note& note, const SynthPatch& patch, double pan, double gateSeconds);
    void render(float* left, float* right, int sampleCount);
    bool active() const;

private:
    struct Voice {
        Note note;
        SynthPatch patch;
        double pan = 0.0;
        double phaseA = 0.0;
        double phaseB = 0.0;
        double subPhase = 0.0;
        double filterState = 0.0;
        double age = 0.0;
        double lfoPhase = 0.0;
        double gateSeconds = 0.25;
        std::uint32_t noiseState = 0x12345678;
    };

    double sampleOscillator(Waveform waveform, double phase, std::uint32_t& noiseState) const;
    double envelopeFor(const Envelope& envelope, double age, double gateSeconds) const;

    double sampleRate_ = 48000.0;
    std::vector<Voice> voices_;
};

} // namespace arachno
