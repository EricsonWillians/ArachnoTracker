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
    // TPDF dither RNG: returns uniform [-0.5, 0.5) (Phase 6)
    double ditherRand() const {
        ditherState_ ^= ditherState_ << 13;
        ditherState_ ^= ditherState_ >> 7;
        ditherState_ ^= ditherState_ << 17;
        return static_cast<double>(ditherState_ & 0xFFFFFFFFull) / 4294967296.0 - 0.5;
    }

    static constexpr int maxUnisonVoices = 8;
    static constexpr int chorusBufferSize = 4096;
    static constexpr int fxDelayBufferSize = 16384;
    static constexpr int maxActiveVoices = 128;
    // TPDF dither constants (Phase 6): scale for 24-bit output depth
    static constexpr double ditherScale = 1.0 / (1ull << 24); // ~5.96e-8
    // Simple xorshift-based dither RNG (stateless per-call, seeded in constructor)
    mutable uint64_t ditherState_ = 0x9E3779B97F4A7C15ull;

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
        std::array<double, maxUnisonVoices> unisonRatioCached {};
        double subPhase = 0.0;
        // TPT SVF filter states (replaced old cascaded 1-pole states)
        double svfZ1 = 0.0;
        double svfZ2 = 0.0;
        double svfG = 0.0;
        double svfK = 0.0;
        double highPassState = 0.0;
        double toneTiltState = 0.0;
        double chorusToneStateL = 0.0;
        double chorusToneStateR = 0.0;
        double delayDiffuseStateL = 0.0;
        double delayDiffuseStateR = 0.0;
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
        double delayToneStateL = 0.0;
        double delayToneStateR = 0.0;
        double reverbStateL1 = 0.0;
        double reverbStateL2 = 0.0;
        double reverbStateR1 = 0.0;
        double reverbStateR2 = 0.0;
        // All-pass diffuser cascade states (Phase 5)
        double apfL1 = 0.0, apfL2 = 0.0, apfL3 = 0.0, apfL4 = 0.0;
        double apfR1 = 0.0, apfR2 = 0.0, apfR3 = 0.0, apfR4 = 0.0;
        double fmFeedbackState = 0.0;
        double driftA = 0.0;
        double driftB = 0.0;
        double driftC = 0.0;
        double driftD = 0.0;
        double driftRatioA = 1.0;
        double driftRatioB = 1.0;
        double driftRatioC = 1.0;
        double driftRatioD = 1.0;
        double detuneRatioA = 1.0;
        double detuneRatioB = 1.0;
        double detuneRatioC = 1.0;
        double detuneRatioD = 1.0;
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
        double lfoValue = 0.0;
        double lfoStep = 0.0;
        double frequencyValue = 440.0;
        double frequencyStep = 0.0;
        double tremoloGainValue = 1.0;
        double tremoloGainStep = 0.0;
        double panLeftGainValue = 0.7071067811865476;
        double panLeftGainStep = 0.0;
        double panRightGainValue = 0.7071067811865476;
        double panRightGainStep = 0.0;
        int unisonCountCached = 1;
        bool oscAEnabledCached = true;
        bool oscBEnabledCached = false;
        bool oscCEnabledCached = false;
        bool oscDEnabledCached = false;
        double oscALevelCached = 1.0;
        double oscBLevelCached = 0.0;
        double oscCLevelCached = 0.0;
        double oscDLevelCached = 0.0;
        double oscMixCached = 0.0;
        double oscCMixCached = 0.0;
        double oscDMixCached = 0.0;
        double driveACached = 0.0;
        double driveBCached = 0.0;
        double driveCCached = 0.0;
        double driveDCached = 0.0;
        double waveCompACached = 1.0;
        double waveCompBCached = 1.0;
        double waveCompCCached = 1.0;
        double waveCompDCached = 1.0;
        double subAmountCached = 0.0;
        double noiseAmountCached = 0.0;
        double toneTiltCached = 0.0;
        double tapeColorCached = 0.0;
        double airBoostCached = 0.0;
        double lowPunchCached = 0.0;
        double analogWarmthCached = 0.36;
        double voiceSlopCached = 0.28;
        double phaseScatterCached = 0.22;
        double unisonWarpCached = 0.18;
        double unisonHumanizeCached = 0.32;
        double fmColorCached = 0.5;
        double fmSpreadCached = 0.0;
        double chorusToneCached = 0.58;
        double chorusJitterCached = 0.26;
        double chorusSaturationCached = 0.24;
        double delayDiffusionCached = 0.24;
        double delayWowCached = 0.22;
        double delayCrossfeedCached = 0.36;
        double reverbDecayCached = 0.62;
        double reverbEarlyMixCached = 0.28;
        double reverbToneCached = 0.52;
        double reverbChorusCached = 0.2;
        double reverbBloomCached = 0.24;
        double consoleCrosstalkCached = 0.06;
        double stereoDepthCached = 0.22;
        double hifiExciterCached = 0.24;
        double outputTransformerCached = 0.2;
        double outputSoftClipCached = 0.28;
        double outputGlueCached = 0.24;
        double tremoloDepthCached = 0.0;
        double gainCached = 1.0;
        double filterDriveCached = 1.0;
        double ringAmountCached = 0.0;
        double resonanceCached = 0.0;
        int filterModeCached = 0;
        double chorusEnsembleCached = 0.0;
        double delayStereoCached = 0.0;
        double delayModDepthCached = 0.333;
        double delayDriveCached = 0.0;
        double delayDuckingCached = 0.0;
        double reverbDiffusionCached = 0.5;
        double reverbWidthCached = 0.0;
        double reverbShimmerCached = 0.0;
        double reverbModDepthCached = 0.0;
        double dividerAlphaCached = 0.02;
        double aliasAlphaCached = 0.2;
        double aliasBlendCached = 0.0;
        // Cached effect mix values for fast-path skipping
        double cachedChorusMix = 0.0;
        double cachedDelayMix = 0.0;
        double cachedReverbMix = 0.0;
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
        std::array<float, fxDelayBufferSize> fxDelayLeft {};
        std::array<float, fxDelayBufferSize> fxDelayRight {};
        int chorusIndex = 0;
        int fxDelayIndex = 0;
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
    double envelopeFor(const Envelope& envelope, double age, double gateSeconds, int curveType) const;

    double sampleRate_ = 48000.0;
    double outputDcInputLeft_ = 0.0;
    double outputDcOutputLeft_ = 0.0;
    double outputDcInputRight_ = 0.0;
    double outputDcOutputRight_ = 0.0;
    double outputLimiterGain_ = 1.0;
    double truePeakLimiterGain_ = 1.0;
    double outputPeakFollower_ = 0.0;
    double outputRmsFollower_ = 0.0;
    double smoothedDspLoadPercent_ = 0.0;
    // True peak limiting state (Phase 6)
    double truePeakPrevLeft_ = 0.0;
    double truePeakPrevRight_ = 0.0;
    SynthRenderTelemetry telemetry_ {};
    std::vector<Voice> voices_;
};

} // namespace arachno
