#pragma once

#include <array>
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
    void reset();
    bool active() const;

private:
    static constexpr int maxUnisonVoices = 8;
    static constexpr int chorusBufferSize = 4096;
    static constexpr int maxActiveVoices = 192;

    struct Voice {
        Note note;
        SynthPatch patch;
        double pan = 0.0;
        double phaseA = 0.0;
        double phaseB = 0.0;
        double phaseC = 0.0;
        double phaseD = 0.0;
        std::array<double, maxUnisonVoices> unisonPhaseA {};
        std::array<double, maxUnisonVoices> unisonPhaseB {};
        std::array<double, maxUnisonVoices> unisonPhaseC {};
        std::array<double, maxUnisonVoices> unisonPhaseD {};
        double subPhase = 0.0;
        double filterState = 0.0;
        double filterState2 = 0.0;
        double highPassState = 0.0;
        double noiseColorState = 0.0;
        double transientColorState = 0.0;
        double combState = 0.0;
        double fmFeedbackState = 0.0;
        double driftA = 0.0;
        double driftB = 0.0;
        double driftC = 0.0;
        double driftD = 0.0;
        double age = 0.0;
        double lfoPhase = 0.0;
        double chorusPhase = 0.0;
        double gateSeconds = 0.25;
        std::array<float, chorusBufferSize> chorusLeft {};
        std::array<float, chorusBufferSize> chorusRight {};
        int chorusIndex = 0;
        double crushHoldLeft = 0.0;
        double crushHoldRight = 0.0;
        int crushCounter = 0;
        std::uint32_t noiseState = 0x12345678;
    };

    double sampleOscillator(
        Waveform waveform,
        double phase,
        double phaseIncrement,
        std::uint32_t& noiseState,
        double pulseWidth = 0.5) const;
    double envelopeFor(const Envelope& envelope, double age, double gateSeconds) const;

    double sampleRate_ = 48000.0;
    double outputDcInputLeft_ = 0.0;
    double outputDcOutputLeft_ = 0.0;
    double outputDcInputRight_ = 0.0;
    double outputDcOutputRight_ = 0.0;
    std::vector<Voice> voices_;
};

} // namespace arachno
