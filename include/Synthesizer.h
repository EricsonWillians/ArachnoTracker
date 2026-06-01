#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "Instrument.h"
#include "Note.h"

namespace arachno {

enum class SynthQualityTier {
    Ultra,
    High,
    Balanced,
    Eco
};

struct SynthRenderTelemetry {
    int activeVoices = 0;
    int sampleCount = 0;
    SynthQualityTier qualityTier = SynthQualityTier::Ultra;
    double dspLoadPercent = 0.0;
    double headroomDb = 0.0;
    double preLimiterHeadroomDb = 0.0;
    double postLimiterHeadroomDb = 0.0;
    double limiterReductionDb = 0.0;
    double outputPeak = 0.0;
    double outputRms = 0.0;
    double underrunRisk = 0.0;
    bool simdReadyPath = false;
    bool deterministicParallelPath = false;
};

class Synthesizer {
public:
    explicit Synthesizer(double sampleRate = 48000.0);

    void setSampleRate(double sampleRate);
    void noteOn(const Note& note, const SynthPatch& patch, double pan, double gateSeconds, int instrumentIndex = -1);
    void applyInstrumentWaveformToActiveVoices(int instrumentIndex, const std::string& oscillator, Waveform waveform);
    void applyInstrumentParameterToActiveVoices(int instrumentIndex, const std::string& parameter, double value);
    void render(float* left, float* right, int sampleCount);
    void reset();
    bool active() const;
    const SynthRenderTelemetry& telemetry() const { return telemetry_; }

private:
    static constexpr int maxUnisonVoices = 8;
    static constexpr int chorusBufferSize = 4096;
    static constexpr int maxActiveVoices = 128;

    struct Voice {
        Note note;
        SynthPatch patch;
        double pan = 0.0;
        double baseFrequency = 440.0;
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
        double filterState3 = 0.0;
        double filterState4 = 0.0;
        double highPassState = 0.0;
        double toneTiltState = 0.0;
        double noiseColorState = 0.0;
        double transientColorState = 0.0;
        double transientBodyPhase = 0.0;
        double nonlinearSmoothState = 0.0;
        double nonlinearPrevInput = 0.0;
        double lowBodyState = 0.0;
        double airExciterState = 0.0;
        double antiAliasState = 0.0;
        double antiAliasState2 = 0.0;
        double dividerState = 1.0;
        double dividerSmoother = 1.0;
        double pinkB0 = 0.0;
        double pinkB1 = 0.0;
        double pinkB2 = 0.0;
        double pinkB3 = 0.0;
        double pinkB4 = 0.0;
        double pinkB5 = 0.0;
        double pinkB6 = 0.0;
        double combState = 0.0;
        double fmFeedbackState = 0.0;
        double driftA = 0.0;
        double driftB = 0.0;
        double driftC = 0.0;
        double driftD = 0.0;
        bool isStolen = false;
        double stolenGain = 1.0;
        double stolenDecayCoefficient = 1.0;
        double pitchEnvelopeState = 0.0;
        double pitchEnvelopeDecayCoefficient = 1.0;
        double transientPitchEnvelopeState = 0.0;
        double transientPitchEnvelopeDecayCoefficient = 1.0;
        double transientEnvelopeState = 1.0;
        double transientEnvelopeDecayCoefficient = 1.0;
        int controlSamplesRemaining = 0;
        int controlBlockSize = 32;
        int filterPassesCached = 1;
        double ampEnvelopeValue = 0.0;
        double ampEnvelopeStep = 0.0;
        double filterAlphaValue = 0.0;
        double filterAlphaStep = 0.0;
        double filterIterAlphaValue = 0.0;
        double filterIterAlphaStep = 0.0;
        double filterFeedbackValue = 0.0;
        double filterFeedbackStep = 0.0;
        double filterResonanceCompValue = 1.0;
        double filterResonanceCompStep = 0.0;
        int driftStepCounter = 0;
        int denormalSanitizeCounter = 0;
        double age = 0.0;
        double lfoPhase = 0.0;
        double chorusPhase = 0.0;
        double gateSeconds = 0.25;
        int instrumentIndex = -1;
        int onsetSamplesRemaining = 0;
        int onsetSamplesTotal = 0;
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
        double pulseWidth = 0.5,
        int triangleHarmonicCap = 31,
        int supersawVoiceCap = 7) const;
    double envelopeFor(const Envelope& envelope, double age, double gateSeconds) const;

    double sampleRate_ = 48000.0;
    double outputDcInputLeft_ = 0.0;
    double outputDcOutputLeft_ = 0.0;
    double outputDcInputRight_ = 0.0;
    double outputDcOutputRight_ = 0.0;
    double outputLimiterGain_ = 1.0;
    double outputPeakFollower_ = 0.0;
    double outputRmsFollower_ = 0.0;
    double smoothedDspLoadPercent_ = 0.0;
    SynthRenderTelemetry telemetry_ {};
    std::vector<Voice> voices_;
};

} // namespace arachno
