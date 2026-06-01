#include "Synthesizer.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <limits>
#if defined(__SSE2__) || defined(__AVX__) || defined(__AVX2__)
#include <immintrin.h>
#endif

namespace arachno {

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double twoPi = 2.0 * pi;
constexpr int maxSynthUnisonVoices = 8;

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double clampSigned(double value) {
    return std::clamp(value, -1.0, 1.0);
}

double centsToRatio(double cents) {
    return std::pow(2.0, cents / 1200.0);
}

double semitonesToRatio(double semitones) {
    return std::pow(2.0, semitones / 12.0);
}

double wrapPhase(double phase) {
    phase = std::fmod(phase, 1.0);
    return phase < 0.0 ? phase + 1.0 : phase;
}

double sanitizeState(double value) {
    return std::abs(value) < 1e-18 ? 0.0 : value;
}

double polyBlep(double phase, double phaseIncrement) {
    if (phaseIncrement <= 0.0 || phaseIncrement >= 1.0) {
        return 0.0;
    }
    if (phase < phaseIncrement) {
        const double t = phase / phaseIncrement;
        return (t + t) - (t * t) - 1.0;
    }
    if (phase > 1.0 - phaseIncrement) {
        const double t = (phase - 1.0) / phaseIncrement;
        return (t * t) + (t + t) + 1.0;
    }
    return 0.0;
}

double randomSymmetric(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return (static_cast<double>((state >> 8) & 0x00ffffff) / 8388607.5) - 1.0;
}

double stepDrift(double current, std::uint32_t& state, double amountCents) {
    const double target = randomSymmetric(state) * amountCents;
    return current + (target - current) * 0.00035;
}

double crushSample(double value, double amount) {
    const double clamped = clamp01(amount);
    if (clamped <= 0.0) {
        return value;
    }
    const double levels = std::max(2.0, std::pow(2.0, 16.0 - clamped * 13.0));
    return std::round(value * levels) / levels;
}

double wavefoldSample(double value, double amount) {
    const double drive = 1.0 + clamp01(amount) * 12.0;
    double folded = value * drive;
    if (folded > 1.0 || folded < -1.0) {
        folded = std::asin(std::sin(folded * pi * 0.5)) * (2.0 / pi);
    }
    return std::clamp(folded, -1.0, 1.0);
}

double asymmetricSaturation(double value, double drive, double asymmetry) {
    const double clampedDrive = std::max(0.0, drive);
    const double shapedAsymmetry = clampSigned(asymmetry);
    const double gain = 1.0 + clampedDrive * 10.0;
    const double biased = value * gain + shapedAsymmetry * 0.22;
    const double positive = std::tanh(std::max(0.0, biased) * (1.0 + shapedAsymmetry * 0.35));
    const double negative = std::tanh(std::min(0.0, biased) * (1.0 - shapedAsymmetry * 0.2));
    return (positive + negative) - shapedAsymmetry * 0.08;
}

double harmonicExciter(double value, double amount) {
    const double clamped = clamp01(amount);
    if (clamped <= 0.0) {
        return value;
    }
    const double even = std::tanh(value * 1.9);
    const double odd = std::tanh(value * 3.7);
    const double colored = even * 0.58 + odd * 0.42;
    const double mix = clamped * 0.42;
    return value * (1.0 - mix) + colored * mix;
}

double nonlinearSmoothingAlpha(double drive, double analogColor) {
    const double harshness = std::clamp(drive * 0.8 + analogColor * 0.7, 0.0, 1.6);
    return std::clamp(0.035 + harshness * 0.09, 0.03, 0.18);
}

double softKneeLimiter(double value, double threshold, double ratio) {
    const double t = std::max(0.01, threshold);
    const double r = std::max(1.0, ratio);
    const double sign = value < 0.0 ? -1.0 : 1.0;
    const double absValue = std::abs(value);
    if (absValue <= t) {
        return value;
    }
    const double compressed = t + (absValue - t) / r;
    return sign * compressed;
}

double waveformLevelCompensation(Waveform waveform) {
    switch (waveform) {
        case Waveform::Sine:
            return 1.00;
        case Waveform::Square:
            return 0.82;
        case Waveform::Saw:
            return 0.94;
        case Waveform::Triangle:
            return 1.08;
        case Waveform::Noise:
            return 0.72;
        case Waveform::SuperSaw:
            return 0.84;
    }
    return 1.0;
}

double bandLimitedTriangle(double phase, double phaseIncrement, int harmonicCap) {
    if (phaseIncrement <= 1e-9) {
        return 0.0;
    }
    const int nyquistLimit = static_cast<int>(std::floor(0.5 / std::max(phaseIncrement, 1e-9)));
    const int maxOddHarmonic = std::clamp(nyquistLimit | 1, 1, std::clamp(harmonicCap | 1, 1, 31));
    double sum = 0.0;
    for (int harmonic = 1; harmonic <= maxOddHarmonic; harmonic += 2) {
        const double sign = ((harmonic / 2) % 2 == 0) ? 1.0 : -1.0;
        sum += sign * (std::sin(twoPi * phase * static_cast<double>(harmonic))
            / (static_cast<double>(harmonic) * static_cast<double>(harmonic)));
    }
    return std::clamp(sum * (8.0 / (pi * pi)), -1.0, 1.0);
}

double shapeFmSignal(double value, double color) {
    const double c = clamp01(color);
    if (c < 0.33) {
        const double mix = c / 0.33;
        const double soft = std::sin(value * twoPi);
        return value * (1.0 - mix) + soft * mix;
    }
    if (c < 0.66) {
        const double mix = (c - 0.33) / 0.33;
        const double clipped = std::tanh(value * 2.8);
        return value * (1.0 - mix) + clipped * mix;
    }
    const double mix = (c - 0.66) / 0.34;
    const double folded = std::asin(std::sin(value * pi * 1.7)) * (2.0 / pi);
    return value * (1.0 - mix) + folded * mix;
}

double sculptNoiseSample(double white, double tone, double& lowState) {
    const double shapedTone = clamp01(tone);
    const double lowAlpha = std::clamp(0.002 + (1.0 - shapedTone) * 0.45, 0.002, 0.8);
    lowState += lowAlpha * (white - lowState);
    const double high = white - lowState;
    const double darkMix = std::pow(1.0 - shapedTone, 0.75);
    const double brightMix = std::pow(shapedTone, 0.75);
    return lowState * darkMix + high * brightMix;
}

int unisonVoiceCount(const SynthPatch& patch) {
    return std::clamp(patch.unisonVoices, 1, maxSynthUnisonVoices);
}

SynthQualityTier qualityTierForVoiceCount(int activeVoiceCount) {
    if (activeVoiceCount > 56) {
        return SynthQualityTier::Eco;
    }
    if (activeVoiceCount > 32) {
        return SynthQualityTier::Balanced;
    }
    if (activeVoiceCount > 16) {
        return SynthQualityTier::High;
    }
    return SynthQualityTier::Ultra;
}

double unisonPosition(int index, int count) {
    if (count <= 1) {
        return 0.0;
    }
    return -1.0 + 2.0 * static_cast<double>(index) / static_cast<double>(count - 1);
}

double unisonOscNorm(int count) {
    static constexpr std::array<double, 9> table {
        1.0, 1.0, 0.6070974422, 0.4532845598, 0.3685673043, 0.3141154420, 0.2750041399, 0.2462992277, 0.2242002190
    };
    const int clamped = std::clamp(count, 1, 8);
    return table[static_cast<std::size_t>(clamped)];
}

double unisonSideNorm(int count) {
    static constexpr std::array<double, 9> table {
        1.0, 1.0, 0.5507609878, 0.3887732629, 0.3038631172, 0.2508388698, 0.2143272656, 0.1876843810, 0.1671841900
    };
    const int clamped = std::clamp(count, 1, 8);
    return table[static_cast<std::size_t>(clamped)];
}

double unisonContrastNorm(int count) {
    static constexpr std::array<double, 9> table {
        1.0, 1.0, 0.5743491775, 0.4152436465, 0.3298769777, 0.2759459323, 0.2384948469, 0.2108247374, 0.1894645708
    };
    const int clamped = std::clamp(count, 1, 8);
    return table[static_cast<std::size_t>(clamped)];
}

double mixOscillatorBlock(double oscA, double oscB, double oscC, double oscD, double mixB, double mixC, double mixD) {
    const double b = clamp01(mixB);
    const double c = clamp01(mixC);
    const double d = clamp01(mixD);
#if defined(__SSE2__) || defined(__AVX__) || defined(__AVX2__)
    const float wA = static_cast<float>((1.0 - d) * (1.0 - c) * (1.0 - b));
    const float wB = static_cast<float>((1.0 - d) * (1.0 - c) * b);
    const float wC = static_cast<float>((1.0 - d) * c);
    const float wD = static_cast<float>(d);
    const __m128 osc = _mm_set_ps(static_cast<float>(oscD), static_cast<float>(oscC), static_cast<float>(oscB), static_cast<float>(oscA));
    const __m128 w = _mm_set_ps(wD, wC, wB, wA);
    const __m128 m = _mm_mul_ps(osc, w);
    alignas(16) float packed[4];
    _mm_store_ps(packed, m);
    return static_cast<double>(packed[0] + packed[1] + packed[2] + packed[3]);
#else
    const double ab = oscA * (1.0 - b) + oscB * b;
    const double abc = ab * (1.0 - c) + oscC * c;
    return abc * (1.0 - d) + oscD * d;
#endif
}

double shapeFilterOutput(
    double filterState4,
    double hpAccum,
    double nonlinearInput,
    double resonance,
    double resonanceComp,
    int filterMode) {
#if defined(__SSE2__) || defined(__AVX__) || defined(__AVX2__)
    const double core = filterMode == 2
        ? std::tanh((filterState4 * 0.78 + hpAccum * 0.22) * 1.5)
        : filterState4;
    const __m128 vec = _mm_set_ps(
        0.0f,
        static_cast<float>(nonlinearInput),
        static_cast<float>(core),
        static_cast<float>(resonanceComp));
    alignas(16) float packed[4];
    _mm_store_ps(packed, vec);
    const double base = static_cast<double>(packed[2]) * static_cast<double>(packed[0]);
    const double feedback = static_cast<double>(packed[1]) * resonance * 0.06;
    return base + feedback;
#else
    const double core = filterMode == 2
        ? std::tanh((filterState4 * 0.78 + hpAccum * 0.22) * 1.5)
        : filterState4;
    return core * resonanceComp + nonlinearInput * resonance * 0.06;
#endif
}

template <typename T, std::size_t Size>
double readDelay(const std::array<T, Size>& buffer, int writeIndex, double delaySamples) {
    double readPosition = static_cast<double>(writeIndex) - delaySamples;
    const double sizeAsDouble = static_cast<double>(Size);
    if (readPosition < 0.0) {
        readPosition += sizeAsDouble;
        if (readPosition < 0.0) {
            readPosition = std::fmod(readPosition, sizeAsDouble) + sizeAsDouble;
        }
    } else if (readPosition >= sizeAsDouble) {
        readPosition -= sizeAsDouble;
    }
    const int indexA = static_cast<int>(readPosition);
    const int indexB = (indexA + 1) % static_cast<int>(Size);
    const double fraction = readPosition - static_cast<double>(indexA);
    return static_cast<double>(buffer[static_cast<std::size_t>(indexA)]) * (1.0 - fraction)
        + static_cast<double>(buffer[static_cast<std::size_t>(indexB)]) * fraction;
}
} // namespace

Synthesizer::Synthesizer(double sampleRate) : sampleRate_(sampleRate) {
    voices_.reserve(maxActiveVoices);
}

void Synthesizer::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
}

void Synthesizer::noteOn(const Note& note, const SynthPatch& patch, double pan, double gateSeconds, int instrumentIndex) {
    const bool anyPrimaryOscEnabled = patch.oscillatorAEnabled
        || patch.oscillatorBEnabled
        || patch.oscillatorCEnabled
        || patch.oscillatorDEnabled;
    if (!anyPrimaryOscEnabled) {
        return;
    }

    auto removeOldestMatching = [&](auto&& predicate, int keepLimit) {
        keepLimit = std::max(1, keepLimit);
        while (true) {
            int count = 0;
            int oldestIndex = -1;
            double oldestAge = -1.0;
            for (int index = 0; index < static_cast<int>(voices_.size()); ++index) {
                if (!predicate(voices_[static_cast<std::size_t>(index)])) {
                    continue;
                }
                ++count;
                if (voices_[static_cast<std::size_t>(index)].age > oldestAge) {
                    oldestAge = voices_[static_cast<std::size_t>(index)].age;
                    oldestIndex = index;
                }
            }
            if (count < keepLimit || oldestIndex < 0) {
                break;
            }
            voices_.erase(voices_.begin() + oldestIndex);
        }
    };
    auto markVoiceStolen = [&](Voice& voice) {
        if (voice.isStolen) {
            return;
        }
        voice.isStolen = true;
        voice.stolenGain = 1.0;
        voice.stolenDecayCoefficient = std::exp(-1.0 / (std::max(1.0, sampleRate_) * 0.007));
        voice.gateSeconds = std::min(voice.gateSeconds, voice.age);
        voice.onsetSamplesRemaining = 0;
    };

    const bool auditionVoice = instrumentIndex < 0;
    const int sameMidiLimit = auditionVoice
        ? 1
        : std::max(3, 11 - std::min(8, std::max(1, patch.unisonVoices)));
    removeOldestMatching(
        [&](const Voice& voice) {
            return voice.note.midi == note.midi;
        },
        sameMidiLimit);
    if (auditionVoice) {
        removeOldestMatching(
            [&](const Voice& voice) {
                return voice.instrumentIndex < 0;
            },
            8);
    }
    if (!patch.name.empty()) {
        removeOldestMatching(
            [&](const Voice& voice) {
                return voice.patch.name == patch.name;
            },
            auditionVoice ? 8 : 24);
    }

    if (voices_.size() >= static_cast<std::size_t>(maxActiveVoices)) {
        auto stealCandidate = voices_.begin();
        double lowestPriority = std::numeric_limits<double>::infinity();
        for (auto it = voices_.begin(); it != voices_.end(); ++it) {
            const double env = envelopeFor(it->patch.ampEnvelope, it->age, it->gateSeconds);
            const bool inRelease = it->age >= it->gateSeconds;
            const double bassProtection = it->note.midi <= 48 ? 0.14 : 0.0;
            const double heldProtection = inRelease ? 0.0 : 0.12;
            const double velocityWeight = std::clamp(static_cast<double>(it->note.velocity), 0.0, 1.0);
            const double priority = env * (0.65 + velocityWeight * 0.35) + bassProtection + heldProtection;
            if (priority < lowestPriority) {
                lowestPriority = priority;
                stealCandidate = it;
            }
        }
        if (stealCandidate != voices_.end()) {
            if (voices_.size() >= static_cast<std::size_t>(maxActiveVoices + 12)
                || stealCandidate->isStolen
                || stealCandidate->stolenGain < 0.04) {
                voices_.erase(stealCandidate);
            } else {
                markVoiceStolen(*stealCandidate);
            }
        }
    }

    Voice voice;
    voice.note = note;
    voice.patch = patch;
    voice.baseFrequency = std::max(1.0, note.frequency());
    voice.pan = std::clamp(pan + patch.pan, -1.0, 1.0);
    voice.gateSeconds = std::max(0.01, gateSeconds);
    voice.instrumentIndex = instrumentIndex;
    voice.onsetSamplesTotal = std::clamp(static_cast<int>(sampleRate_ * 0.0025), 24, 192);
    voice.onsetSamplesRemaining = voice.onsetSamplesTotal;
    voice.pitchEnvelopeState = patch.pitchEnvelopeSemitones;
    voice.pitchEnvelopeDecayCoefficient = std::exp(-1.0 / (std::max(1.0, sampleRate_) * std::max(0.001, patch.pitchEnvelopeDecay)));
    voice.transientPitchEnvelopeState = patch.transientPitchSemitones;
    voice.transientPitchEnvelopeDecayCoefficient = std::exp(-1.0 / (std::max(1.0, sampleRate_) * std::max(0.001, patch.transientPitchDecay)));
    voice.transientEnvelopeState = 1.0;
    voice.transientEnvelopeDecayCoefficient = std::exp(-1.0 / (std::max(1.0, sampleRate_) * std::max(0.001, patch.transientDecay)));
    voice.noiseState = static_cast<std::uint32_t>((note.midi + 1) * 2654435761u);
    const double analogColor = clamp01(patch.analogColor);
    const double phaseSeed = static_cast<double>(voice.noiseState & 0xffffu) / 65535.0;
    const double phaseOffset = analogColor * 0.35 * phaseSeed;
    voice.subPhase = wrapPhase(phaseOffset * 0.7);
    voice.transientBodyPhase = wrapPhase(phaseOffset * 1.3);
    voice.dividerState = (phaseSeed > 0.5) ? 1.0 : -1.0;
    voice.dividerSmoother = voice.dividerState;
    voice.pinkB0 = randomSymmetric(voice.noiseState) * 0.008;
    voice.pinkB1 = randomSymmetric(voice.noiseState) * 0.008;
    voice.pinkB2 = randomSymmetric(voice.noiseState) * 0.008;
    voice.pinkB3 = randomSymmetric(voice.noiseState) * 0.008;
    voice.pinkB4 = randomSymmetric(voice.noiseState) * 0.008;
    voice.pinkB5 = randomSymmetric(voice.noiseState) * 0.008;
    voice.pinkB6 = randomSymmetric(voice.noiseState) * 0.008;
    const int unisonCount = unisonVoiceCount(patch);
    for (int index = 0; index < unisonCount; ++index) {
        const double offset = static_cast<double>(index) / static_cast<double>(unisonCount);
        const double localJitter = analogColor * randomSymmetric(voice.noiseState) * 0.035;
        voice.unisonPhaseA[static_cast<std::size_t>(index)] = wrapPhase(offset + phaseOffset + localJitter);
        voice.unisonPhaseB[static_cast<std::size_t>(index)] = wrapPhase(offset * 0.37 + phaseOffset * 0.51 + localJitter);
        voice.unisonPhaseC[static_cast<std::size_t>(index)] = wrapPhase(offset * 0.73 + phaseOffset * 0.83 + localJitter);
        voice.unisonPhaseD[static_cast<std::size_t>(index)] = wrapPhase(offset * 0.19 + phaseOffset * 0.29 + localJitter);
    }
    voices_.push_back(voice);
}

void Synthesizer::applyInstrumentWaveformToActiveVoices(int instrumentIndex, const std::string& oscillator, Waveform waveform) {
    if (instrumentIndex < 0) {
        return;
    }
    std::string normalized = oscillator;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    for (Voice& voice : voices_) {
        if (voice.instrumentIndex != instrumentIndex) {
            continue;
        }
        if (normalized == "a" || normalized == "osc_a" || normalized == "oscillator_a") {
            voice.patch.oscillatorA = waveform;
        } else if (normalized == "b" || normalized == "osc_b" || normalized == "oscillator_b") {
            voice.patch.oscillatorB = waveform;
        } else if (normalized == "c" || normalized == "osc_c" || normalized == "oscillator_c") {
            voice.patch.oscillatorC = waveform;
        } else if (normalized == "d" || normalized == "osc_d" || normalized == "oscillator_d") {
            voice.patch.oscillatorD = waveform;
        }
    }
}

void Synthesizer::applyInstrumentParameterToActiveVoices(int instrumentIndex, const std::string& parameter, double value) {
    if (instrumentIndex < 0) {
        return;
    }
    std::string normalized = parameter;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return ch == '-' ? '_' : static_cast<char>(std::tolower(ch));
    });
    for (Voice& voice : voices_) {
        if (voice.instrumentIndex != instrumentIndex) {
            continue;
        }
        (void)setSynthPatchParameter(voice.patch, parameter, value);
        if (normalized == "pitch_envelope_semitones" || normalized == "pitch_env" || normalized == "pitch_envelope") {
            voice.pitchEnvelopeState = voice.patch.pitchEnvelopeSemitones;
        } else if (normalized == "pitch_envelope_decay" || normalized == "pitch_decay") {
            voice.pitchEnvelopeDecayCoefficient = std::exp(
                -1.0 / (std::max(1.0, sampleRate_) * std::max(0.001, voice.patch.pitchEnvelopeDecay)));
        } else if (normalized == "transient_pitch_semitones" || normalized == "transient_pitch") {
            voice.transientPitchEnvelopeState = voice.patch.transientPitchSemitones;
        } else if (normalized == "transient_pitch_decay") {
            voice.transientPitchEnvelopeDecayCoefficient = std::exp(
                -1.0 / (std::max(1.0, sampleRate_) * std::max(0.001, voice.patch.transientPitchDecay)));
        } else if (normalized == "transient_decay") {
            voice.transientEnvelopeDecayCoefficient = std::exp(
                -1.0 / (std::max(1.0, sampleRate_) * std::max(0.001, voice.patch.transientDecay)));
            voice.transientEnvelopeState = 1.0;
        }
    }
}

bool Synthesizer::active() const {
    return !voices_.empty();
}

void Synthesizer::reset() {
    voices_.clear();
    outputDcInputLeft_ = 0.0;
    outputDcOutputLeft_ = 0.0;
    outputDcInputRight_ = 0.0;
    outputDcOutputRight_ = 0.0;
    outputLimiterGain_ = 1.0;
    outputPeakFollower_ = 0.0;
    outputRmsFollower_ = 0.0;
    smoothedDspLoadPercent_ = 0.0;
    telemetry_ = SynthRenderTelemetry {};
}

void Synthesizer::render(float* left, float* right, int sampleCount) {
    if (sampleCount <= 0) {
        return;
    }
    const auto renderStart = std::chrono::steady_clock::now();
    const double invSampleRate = sampleRate_ > 0.0 ? (1.0 / sampleRate_) : 0.0;
    const int activeVoiceCount = static_cast<int>(voices_.size());
    const SynthQualityTier qualityTier = qualityTierForVoiceCount(activeVoiceCount);
    const bool heavyLoad = qualityTier == SynthQualityTier::Balanced || qualityTier == SynthQualityTier::Eco;
    const bool extremeLoad = qualityTier == SynthQualityTier::Eco;
    const int triangleHarmonicCap = qualityTier == SynthQualityTier::Ultra
        ? 31
        : (qualityTier == SynthQualityTier::High ? 25 : (qualityTier == SynthQualityTier::Balanced ? 17 : 11));
    const int supersawVoiceCap = qualityTier == SynthQualityTier::Ultra
        ? 7
        : (qualityTier == SynthQualityTier::High ? 5 : (qualityTier == SynthQualityTier::Balanced ? 4 : 3));
    const double voiceNorm = 1.0 / std::sqrt(1.0 + static_cast<double>(activeVoiceCount) * 0.95);
    const double peakAttackCoeff = std::exp(-1.0 / (sampleRate_ * 0.00085));
    const double peakReleaseCoeff = std::exp(-1.0 / (sampleRate_ * 0.075));
    const double rmsCoeff = std::exp(-1.0 / (sampleRate_ * 0.02));
    constexpr double limiterThreshold = 0.82;
    const double limiterAttackCoeff = std::exp(-1.0 / (sampleRate_ * 0.00045));
    const double limiterReleaseCoeff = std::exp(-1.0 / (sampleRate_ * 0.09));
    constexpr double dcCoeff = 0.995;
    double blockPreLimiterPeak = 0.0;
    double blockPostLimiterPeak = 0.0;
    auto computeFilterControlValues = [&](const Voice& voice,
                                          double ageSeconds,
                                          double lfoValue,
                                          double pitchEnvelopeValue,
                                          double transientPitchEnvelopeValue,
                                          double velocityNorm,
                                          double fmDepthHint,
                                          double& outAlpha,
                                          double& outIterAlpha,
                                          double& outFeedback,
                                          double& outResonanceComp,
                                          int& outFilterPasses) {
        const double frequency = voice.baseFrequency
            * centsToRatio(lfoValue * voice.patch.vibratoCents)
            * semitonesToRatio(pitchEnvelopeValue + transientPitchEnvelopeValue);
        const double filterEnvelope = envelopeFor(
            voice.patch.filterEnvelope,
            ageSeconds,
            voice.gateSeconds);
        const double keytrack = clamp01(voice.patch.filterKeytrack);
        const double keytrackBias = ((static_cast<double>(voice.note.midi - 60) / 48.0) * 0.18) * keytrack;
        const double cutoffNormalized = clamp01(
            voice.patch.cutoff
            + filterEnvelope * voice.patch.filterEnvelopeAmount
            + lfoValue * clamp01(voice.patch.lfoFilterDepth) * 0.35
            + (velocityNorm - 0.6) * 0.16
            + keytrackBias);
        const double cutoff = cutoffNormalized * cutoffNormalized;
        const double cutoffHz = std::clamp(
            25.0 * std::exp2(cutoff * 10.0),
            20.0,
            sampleRate_ * 0.45);
        outAlpha = std::clamp(1.0 - std::exp(-twoPi * cutoffHz / sampleRate_), 0.001, 0.999);
        const double resonance = clamp01(voice.patch.resonance);
        const int filterMode = std::clamp(voice.patch.filterMode, 0, 2);
        const bool hqFilterPath = !heavyLoad
            && (cutoffHz > 3600.0 || resonance > 0.58 || fmDepthHint > 0.20 || frequency > 1400.0);
        outFilterPasses = hqFilterPath ? 2 : 1;
        outIterAlpha = outFilterPasses > 1
            ? std::clamp(1.0 - std::exp(-twoPi * cutoffHz / (sampleRate_ * static_cast<double>(outFilterPasses))), 0.001, 0.999)
            : outAlpha;
        outFeedback = resonance * (filterMode == 2 ? 1.55 : (1.0 - cutoff * 0.72));
        outResonanceComp = 1.0 + resonance * (0.25 + (1.0 - cutoff) * 0.65);
    };

    for (int sample = 0; sample < sampleCount; ++sample) {
        const double denormalBias = (sample & 1) == 0 ? 1e-18 : -1e-18;
        std::array<double, 4> voiceBusLeft {0.0, 0.0, 0.0, 0.0};
        std::array<double, 4> voiceBusRight {0.0, 0.0, 0.0, 0.0};

        for (std::size_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
            Voice& voice = voices_[voiceIndex];
            const double lfo = std::sin(voice.lfoPhase * twoPi);
            const double velocityNorm = std::clamp(static_cast<double>(voice.note.velocity), 0.0, 1.0);
            const double analogColor = clamp01(voice.patch.analogColor);
            if (voice.driftStepCounter <= 0) {
                const double driftAmountCents = 0.14
                    + analogColor * 2.25
                    + clamp01(voice.patch.drive) * 0.65
                    + clamp01(voice.patch.unisonDetuneCents / 20.0) * 0.75;
                voice.driftA = stepDrift(voice.driftA, voice.noiseState, driftAmountCents);
                voice.driftB = stepDrift(voice.driftB, voice.noiseState, driftAmountCents * 1.13);
                voice.driftC = stepDrift(voice.driftC, voice.noiseState, driftAmountCents * 1.07);
                voice.driftD = stepDrift(voice.driftD, voice.noiseState, driftAmountCents * 0.92);
                voice.driftStepCounter = heavyLoad ? 7 : 3;
            } else {
                --voice.driftStepCounter;
            }
            const double driftRatioA = centsToRatio(voice.driftA);
            const double driftRatioB = centsToRatio(voice.driftB);
            const double driftRatioC = centsToRatio(voice.driftC);
            const double driftRatioD = centsToRatio(voice.driftD);
            const double pitchEnvelope = voice.pitchEnvelopeState;
            const double transientPitchEnvelope = voice.transientPitchEnvelopeState;
            const double frequency = voice.baseFrequency
                * centsToRatio(lfo * voice.patch.vibratoCents)
                * semitonesToRatio(pitchEnvelope + transientPitchEnvelope);
            const double nyquist = std::max(1.0, sampleRate_ * 0.5);
            const double spectralStress = std::clamp(frequency / (nyquist * 0.92), 0.0, 1.0);
            const double antiAliasScale = 1.0 - 0.72 * spectralStress * spectralStress;
            const double lowEndFocus = std::clamp((220.0 - frequency) / 180.0, 0.0, 1.0);
            const double detuneRatioA = centsToRatio(voice.patch.oscADetuneCents);
            const double detuneRatioB = centsToRatio(voice.patch.detuneCents + voice.patch.oscBDetuneCents);
            const double detuneRatioC = centsToRatio(voice.patch.detuneCCents + voice.patch.oscCDetuneCents);
            const double detuneRatioD = centsToRatio(voice.patch.detuneDCents + voice.patch.oscDDetuneCents);
            int unisonCount = unisonVoiceCount(voice.patch);
            if (activeVoiceCount > 0) {
                const int adaptiveBudget = std::max(1, 64 / activeVoiceCount);
                unisonCount = std::min(unisonCount, adaptiveBudget);
            }
            const double syncAmount = voice.patch.hardSyncEnabled
                ? clamp01(voice.patch.hardSync) * antiAliasScale
                : 0.0;
            const double pulseWidthA = std::clamp(
                voice.patch.pulseWidth
                    + (voice.patch.oscAPulseWidth - 0.5)
                    + lfo * clamp01(voice.patch.pwmDepth + voice.patch.oscAPwmDepth) * 0.45,
                0.03,
                0.97);
            const double pulseWidthB = std::clamp(
                voice.patch.pulseWidth
                    + (voice.patch.oscBPulseWidth - 0.5)
                    + lfo * clamp01(voice.patch.pwmDepth + voice.patch.oscBPwmDepth) * 0.45,
                0.03,
                0.97);
            const double pulseWidthC = std::clamp(
                voice.patch.pulseWidth
                    + (voice.patch.oscCPulseWidth - 0.5)
                    + lfo * clamp01(voice.patch.pwmDepth + voice.patch.oscCPwmDepth) * 0.45,
                0.03,
                0.97);
            const double pulseWidthD = std::clamp(
                voice.patch.pulseWidth
                    + (voice.patch.oscDPulseWidth - 0.5)
                    + lfo * clamp01(voice.patch.pwmDepth + voice.patch.oscDPwmDepth) * 0.45,
                0.03,
                0.97);
            const double fmRatio = std::max(0.125, voice.patch.fmRatio);
            double fmDepth = voice.patch.fmEnabled ? clamp01(voice.patch.fmAmount) * 1.5 : 0.0;
            if (heavyLoad) {
                fmDepth *= 0.55;
            }
            if (extremeLoad) {
                fmDepth = 0.0;
            }
            fmDepth *= antiAliasScale;
            if (voice.controlSamplesRemaining <= 0) {
                int controlBlockSamples = std::max(8, voice.controlBlockSize);
                if (heavyLoad) {
                    controlBlockSamples = std::max(controlBlockSamples, 48);
                }
                if (extremeLoad) {
                    controlBlockSamples = std::max(controlBlockSamples, 64);
                }
                voice.controlBlockSize = std::clamp(controlBlockSamples, 8, 128);
                const double blockDuration = static_cast<double>(voice.controlBlockSize) * invSampleRate;
                const double blockLfoEnd = std::sin(
                    wrapPhase(voice.lfoPhase + std::max(0.0, voice.patch.lfoRate) * blockDuration) * twoPi);
                const double ampStart = envelopeFor(voice.patch.ampEnvelope, voice.age, voice.gateSeconds);
                const double ampEnd = envelopeFor(
                    voice.patch.ampEnvelope,
                    voice.age + blockDuration,
                    voice.gateSeconds);
                voice.ampEnvelopeValue = ampStart;
                voice.ampEnvelopeStep = (ampEnd - ampStart) / static_cast<double>(voice.controlBlockSize);

                double alphaStart = 0.001;
                double iterAlphaStart = 0.001;
                double feedbackStart = 0.0;
                double resonanceCompStart = 1.0;
                int filterPassesStart = 1;
                computeFilterControlValues(
                    voice,
                    voice.age,
                    lfo,
                    voice.pitchEnvelopeState,
                    voice.transientPitchEnvelopeState,
                    velocityNorm,
                    fmDepth,
                    alphaStart,
                    iterAlphaStart,
                    feedbackStart,
                    resonanceCompStart,
                    filterPassesStart);

                double alphaEnd = alphaStart;
                double iterAlphaEnd = iterAlphaStart;
                double feedbackEnd = feedbackStart;
                double resonanceCompEnd = resonanceCompStart;
                int filterPassesEnd = filterPassesStart;
                computeFilterControlValues(
                    voice,
                    voice.age + blockDuration,
                    blockLfoEnd,
                    voice.pitchEnvelopeState * std::pow(
                        voice.pitchEnvelopeDecayCoefficient,
                        static_cast<double>(voice.controlBlockSize)),
                    voice.transientPitchEnvelopeState * std::pow(
                        voice.transientPitchEnvelopeDecayCoefficient,
                        static_cast<double>(voice.controlBlockSize)),
                    velocityNorm,
                    fmDepth,
                    alphaEnd,
                    iterAlphaEnd,
                    feedbackEnd,
                    resonanceCompEnd,
                    filterPassesEnd);

                voice.filterAlphaValue = alphaStart;
                voice.filterAlphaStep = (alphaEnd - alphaStart) / static_cast<double>(voice.controlBlockSize);
                voice.filterIterAlphaValue = iterAlphaStart;
                voice.filterIterAlphaStep = (iterAlphaEnd - iterAlphaStart) / static_cast<double>(voice.controlBlockSize);
                voice.filterFeedbackValue = feedbackStart;
                voice.filterFeedbackStep = (feedbackEnd - feedbackStart) / static_cast<double>(voice.controlBlockSize);
                voice.filterResonanceCompValue = resonanceCompStart;
                voice.filterResonanceCompStep = (resonanceCompEnd - resonanceCompStart)
                    / static_cast<double>(voice.controlBlockSize);
                voice.filterPassesCached = std::max(filterPassesStart, filterPassesEnd);
                voice.controlSamplesRemaining = voice.controlBlockSize;
            }
            const double fmFeedback = voice.patch.fmEnabled ? clamp01(voice.patch.fmFeedback) * 0.92 : 0.0;
            const int fmAlgorithm = std::clamp(voice.patch.fmAlgorithm, 0, 3);
            const bool oscAEnabled = voice.patch.oscillatorAEnabled;
            const bool oscBEnabled = voice.patch.oscillatorBEnabled;
            const bool oscCEnabled = voice.patch.oscillatorCEnabled;
            const bool oscDEnabled = voice.patch.oscillatorDEnabled;
            const double oscALevel = clamp01(voice.patch.oscALevel);
            const double oscBLevel = clamp01(voice.patch.oscBLevel);
            const double oscCLevel = clamp01(voice.patch.oscCLevel);
            const double oscDLevel = clamp01(voice.patch.oscDLevel);
            double oscA = 0.0;
            double oscB = 0.0;
            double oscC = 0.0;
            double oscD = 0.0;
            double unisonSide = 0.0;
            double unisonContrast = 0.0;
            bool unisonReferenceSet = false;
            double unisonReference = 0.0;
            const double primaryPhaseBefore = voice.unisonPhaseA.front();
            const double oscMix = clamp01(voice.patch.oscillatorMix);
            const double oscCMix = clamp01(voice.patch.oscillatorCMix);
            const double oscDMix = clamp01(voice.patch.oscillatorDMix);
            const double driveA = clamp01(voice.patch.oscADrive);
            const double driveB = clamp01(voice.patch.oscBDrive);
            const double driveC = clamp01(voice.patch.oscCDrive);
            const double driveD = clamp01(voice.patch.oscDDrive);
            const double waveCompA = waveformLevelCompensation(voice.patch.oscillatorA);
            const double waveCompB = waveformLevelCompensation(voice.patch.oscillatorB);
            const double waveCompC = waveformLevelCompensation(voice.patch.oscillatorC);
            const double waveCompD = waveformLevelCompensation(voice.patch.oscillatorD);
            for (int index = 0; index < unisonCount; ++index) {
                const std::size_t unisonIndex = static_cast<std::size_t>(index);
                const double position = unisonPosition(index, unisonCount);
                const double phaseA = voice.unisonPhaseA[unisonIndex];
                const double phaseB = voice.unisonPhaseB[unisonIndex];
                const double phaseC = voice.unisonPhaseC[unisonIndex];
                const double phaseD = voice.unisonPhaseD[unisonIndex];
                const double unisonRatio = centsToRatio(position * voice.patch.unisonDetuneCents);
                const double incrementA = std::clamp((frequency * unisonRatio * detuneRatioA * driftRatioA) * invSampleRate, 0.0, 0.49);
                const double incrementB = std::clamp(
                    (frequency * unisonRatio * detuneRatioB * driftRatioB) * invSampleRate,
                    0.0,
                    0.49);
                const double incrementC = std::clamp(
                    (frequency * unisonRatio * detuneRatioC * driftRatioC) * invSampleRate,
                    0.0,
                    0.49);
                const double incrementD = std::clamp(
                    (frequency * unisonRatio * detuneRatioD * driftRatioD) * invSampleRate,
                    0.0,
                    0.49);
                const double syncedPhaseB = syncAmount <= 0.0
                    ? phaseB
                    : wrapPhase(phaseA * (1.0 + syncAmount * 12.0) + phaseB * (1.0 - syncAmount));
                double pmA = 0.0;
                double pmB = 0.0;
                double pmC = 0.0;
                double pmD = 0.0;
                if (fmDepth > 0.000001) {
                    const double opB = shapeFmSignal(std::sin(wrapPhase(phaseB * fmRatio) * twoPi), voice.patch.analogColor);
                    const double opC = shapeFmSignal(std::sin(wrapPhase(phaseC * fmRatio * 1.5) * twoPi), voice.patch.analogColor);
                    const double opD = shapeFmSignal(std::sin(wrapPhase(phaseD * fmRatio * 2.0) * twoPi), voice.patch.analogColor);
                    const double fb = clampSigned(voice.fmFeedbackState) * fmFeedback;
                    switch (fmAlgorithm) {
                        case 0: // 4-op cascade
                            pmC = (opD + fb * 0.4) * fmDepth * 0.35;
                            pmB = (opC + pmC) * fmDepth * 0.55;
                            pmA = (opB + pmB + fb) * fmDepth;
                            break;
                        case 1: // split cascade
                            pmB = (opD + fb * 0.5) * fmDepth * 0.65;
                            pmA = (opB * 0.7 + opC * 0.55 + pmB + fb) * fmDepth;
                            break;
                        case 2: // parallel stack
                            pmA = (opB * 0.7 + opC * 0.5 + opD * 0.35 + fb) * fmDepth;
                            pmB = (opC * 0.45 + fb * 0.3) * fmDepth;
                            break;
                        default: // metallic feedback network
                            pmA = (opB + opC + opD) * fmDepth * 0.55 + fb;
                            pmB = (opB + opD * 0.35) * fmDepth * 0.4;
                            pmC = (opB + fb * 0.25) * fmDepth * 0.25;
                            pmD = (opC + fb * 0.15) * fmDepth * 0.2;
                            break;
                    }
                }
                const double sampleA = oscAEnabled
                    ? sampleOscillator(
                        voice.patch.oscillatorA,
                        wrapPhase(phaseA + pmA),
                        incrementA,
                        voice.noiseState,
                        pulseWidthA,
                        triangleHarmonicCap,
                        supersawVoiceCap)
                    : 0.0;
                const double sampleB = oscBEnabled
                    ? sampleOscillator(
                        voice.patch.oscillatorB,
                        wrapPhase(syncedPhaseB + pmB),
                        incrementB,
                        voice.noiseState,
                        pulseWidthB,
                        triangleHarmonicCap,
                        supersawVoiceCap)
                    : 0.0;
                const double sampleC = oscCEnabled
                    ? sampleOscillator(
                        voice.patch.oscillatorC,
                        wrapPhase(phaseC + pmC),
                        incrementC,
                        voice.noiseState,
                        pulseWidthC,
                        triangleHarmonicCap,
                        supersawVoiceCap)
                    : 0.0;
                const double sampleD = oscDEnabled
                    ? sampleOscillator(
                        voice.patch.oscillatorD,
                        wrapPhase(phaseD + pmD),
                        incrementD,
                        voice.noiseState,
                        pulseWidthD,
                        triangleHarmonicCap,
                        supersawVoiceCap)
                    : 0.0;
                const double shapedA = asymmetricSaturation(
                    sampleA * waveCompA,
                    driveA * 0.9,
                    0.0)
                    * oscALevel;
                const double shapedB = asymmetricSaturation(
                    sampleB * waveCompB,
                    driveB * 0.9,
                    0.0)
                    * oscBLevel;
                const double shapedC = asymmetricSaturation(
                    sampleC * waveCompC,
                    driveC * 0.9,
                    0.0)
                    * oscCLevel;
                const double shapedD = asymmetricSaturation(
                    sampleD * waveCompD,
                    driveD * 0.9,
                    0.0)
                    * oscDLevel;
                oscA += shapedA;
                oscB += shapedB;
                oscC += shapedC;
                oscD += shapedD;
                const double combined = mixOscillatorBlock(shapedA, shapedB, shapedC, shapedD, oscMix, oscCMix, oscDMix);
                unisonSide += combined * position;
                if (!unisonReferenceSet) {
                    unisonReference = combined;
                    unisonReferenceSet = true;
                } else {
                    unisonContrast += (combined - unisonReference) * position;
                }

                voice.unisonPhaseA[unisonIndex] = wrapPhase(phaseA + incrementA);
                voice.unisonPhaseB[unisonIndex] = wrapPhase(phaseB + incrementB);
                voice.unisonPhaseC[unisonIndex] = wrapPhase(phaseC + incrementC);
                voice.unisonPhaseD[unisonIndex] = wrapPhase(phaseD + incrementD);
            }
            const bool primaryWrapped = voice.unisonPhaseA.front() < primaryPhaseBefore;
            if (primaryWrapped) {
                voice.dividerState = -voice.dividerState;
            }
            const double dividerFollowHz = std::clamp(frequency * 0.40, 24.0, 3600.0);
            const double dividerAlpha = std::clamp(1.0 - std::exp(-twoPi * dividerFollowHz / sampleRate_), 0.01, 0.35);
            voice.dividerSmoother += dividerAlpha * (voice.dividerState - voice.dividerSmoother);
            const double dividerSub = voice.patch.subEnabled ? voice.dividerSmoother : 0.0;
            const double unisonNorm = unisonOscNorm(unisonCount);
            oscA *= unisonNorm;
            oscB *= unisonNorm;
            oscC *= unisonNorm;
            oscD *= unisonNorm;
            unisonSide *= unisonSideNorm(unisonCount);
            unisonSide = unisonSide * 0.82 + unisonContrast * (0.18 * unisonContrastNorm(unisonCount));
            const double subIncrement = std::clamp((frequency * 0.5) * invSampleRate, 0.0, 0.49);
            const double subSquare = voice.patch.subEnabled
                ? sampleOscillator(
                    Waveform::Square,
                    voice.subPhase,
                    subIncrement,
                    voice.noiseState,
                    0.5,
                    triangleHarmonicCap,
                    supersawVoiceCap)
                : 0.0;
            const double subSine = voice.patch.subEnabled
                ? std::sin(voice.subPhase * twoPi)
                : 0.0;
            const double subOctave = voice.patch.subEnabled
                ? std::sin(wrapPhase(voice.subPhase * 0.5) * twoPi)
                : 0.0;
            const double sub = subSquare * 0.18 + subSine * 0.54 + subOctave * 0.12 + dividerSub * 0.16;
            const double whiteNoise = sampleOscillator(
                Waveform::Noise,
                0.0,
                0.0,
                voice.noiseState,
                0.5,
                triangleHarmonicCap,
                supersawVoiceCap);
            voice.pinkB0 = 0.99886 * voice.pinkB0 + whiteNoise * 0.0555179;
            voice.pinkB1 = 0.99332 * voice.pinkB1 + whiteNoise * 0.0750759;
            voice.pinkB2 = 0.96900 * voice.pinkB2 + whiteNoise * 0.1538520;
            voice.pinkB3 = 0.86650 * voice.pinkB3 + whiteNoise * 0.3104856;
            voice.pinkB4 = 0.55000 * voice.pinkB4 + whiteNoise * 0.5329522;
            voice.pinkB5 = -0.7616 * voice.pinkB5 - whiteNoise * 0.0168980;
            const double pinkNoise = std::clamp(
                (voice.pinkB0 + voice.pinkB1 + voice.pinkB2 + voice.pinkB3 + voice.pinkB4 + voice.pinkB5 + voice.pinkB6
                    + whiteNoise * 0.5362)
                    * 0.11,
                -1.0,
                1.0);
            voice.pinkB6 = whiteNoise * 0.115926;
            const double noiseTone = clamp01(voice.patch.noiseTone);
            const double brightNoiseBias = std::pow(noiseTone, 1.45);
            const double noiseSource = pinkNoise * (1.0 - brightNoiseBias) + whiteNoise * brightNoiseBias;
            const double noise = voice.patch.noiseEnabled
                ? sculptNoiseSample(noiseSource, voice.patch.noiseTone, voice.noiseColorState)
                : 0.0;
            const double oscABCD = mixOscillatorBlock(oscA, oscB, oscC, oscD, oscMix, oscCMix, oscDMix);
            int enabledOscillators = 0;
            enabledOscillators += oscAEnabled ? 1 : 0;
            enabledOscillators += oscBEnabled ? 1 : 0;
            enabledOscillators += oscCEnabled ? 1 : 0;
            enabledOscillators += oscDEnabled ? 1 : 0;
            const double sourceWeight = static_cast<double>(enabledOscillators)
                + (voice.patch.subEnabled ? 0.55 : 0.0)
                + (voice.patch.noiseEnabled ? 0.65 : 0.0);
            const double levelComp = 1.0 / std::sqrt(std::max(1.0, sourceWeight));
            double value = oscABCD
                + sub * voice.patch.subOscillator
                + noise * voice.patch.noise;
            value += denormalBias;
            const double ringAmount = voice.patch.ringEnabled ? clamp01(voice.patch.ringMod) : 0.0;
            value = value * (1.0 - ringAmount)
                + (oscA * (oscB + oscC * 0.5 + oscD * 0.35)) * ringAmount;
            value *= levelComp;
            const double bassFocus = clamp01(voice.patch.subOscillator) * (0.35 + analogColor * 0.65);
            const double subReinforce = std::tanh(
                (subSine * 0.82 + subOctave * 0.48) * (1.0 + std::max(0.0, voice.patch.drive) * 1.6));
            const double subDynamics = 1.0 / (1.0 + std::abs(oscABCD) * 0.35);
            value += subReinforce * bassFocus * (0.30 + lowEndFocus * 0.24) * subDynamics;
            const double aliasCutHz = std::clamp(
                (1.0 - spectralStress) * 16000.0 + 2200.0 + analogColor * 1400.0,
                1800.0,
                18000.0);
            const double aliasAlpha = std::clamp(1.0 - std::exp(-twoPi * aliasCutHz / sampleRate_), 0.02, 0.98);
            voice.antiAliasState += aliasAlpha * (value - voice.antiAliasState);
            voice.antiAliasState2 += aliasAlpha * (voice.antiAliasState - voice.antiAliasState2);
            const double aliasBlend = std::clamp(spectralStress * (0.22 + (1.0 - analogColor) * 0.18), 0.0, 0.46);
            value = value * (1.0 - aliasBlend) + voice.antiAliasState2 * aliasBlend;

            const double alpha = std::clamp(voice.filterAlphaValue, 0.001, 0.999);
            const double iterAlpha = std::clamp(
                voice.filterPassesCached > 1 ? voice.filterIterAlphaValue : alpha,
                0.001,
                0.999);
            const double feedback = voice.filterFeedbackValue;
            const double resonanceComp = voice.filterResonanceCompValue;
            const double resonance = clamp01(voice.patch.resonance);
            const int filterMode = std::clamp(voice.patch.filterMode, 0, 2);
            const double filterDrive = 1.0 + clamp01(voice.patch.filterDrive) * 7.0;
            const double nonlinearInput = std::tanh(value * filterDrive);
            const int filterPasses = std::clamp(voice.filterPassesCached, 1, 2);
            double filterValue = nonlinearInput;
            double hpAccum = 0.0;
            for (int pass = 0; pass < filterPasses; ++pass) {
                if (filterMode == 0) {
                    const double filterInput = filterValue - (voice.filterState - voice.filterState2) * feedback;
                    const double safeInput = std::tanh(filterInput * 0.62) * 1.6;
                    voice.filterState += iterAlpha * (safeInput - voice.filterState);
                    voice.filterState2 += iterAlpha * (voice.filterState - voice.filterState2);
                    filterValue = voice.filterState2;
                } else if (filterMode == 1) {
                    const double filterInput = filterValue - (voice.filterState4 * feedback);
                    const double safeInput = std::tanh(filterInput * 0.62) * 1.6;
                    voice.filterState += iterAlpha * (safeInput - voice.filterState);
                    voice.filterState2 += iterAlpha * (voice.filterState - voice.filterState2);
                    voice.filterState3 += iterAlpha * (voice.filterState2 - voice.filterState3);
                    voice.filterState4 += iterAlpha * (voice.filterState3 - voice.filterState4);
                    filterValue = voice.filterState4;
                } else {
                    const double hpInput = filterValue - voice.filterState4 * feedback;
                    const double safeInput = std::tanh(hpInput * 0.62) * 1.6;
                    voice.filterState += iterAlpha * (safeInput - voice.filterState);
                    const double hp = safeInput - voice.filterState;
                    voice.filterState2 += iterAlpha * (hp - voice.filterState2);
                    voice.filterState3 += iterAlpha * (voice.filterState2 - voice.filterState3);
                    voice.filterState4 += iterAlpha * (voice.filterState3 - voice.filterState4);
                    hpAccum = hp;
                    filterValue = voice.filterState4;
                }
            }
            value = shapeFilterOutput(
                filterMode == 2 ? voice.filterState4 : filterValue,
                hpAccum,
                nonlinearInput,
                resonance,
                resonanceComp,
                filterMode);

            const double combMix = extremeLoad
                ? 0.0
                : (heavyLoad
                ? clamp01(voice.patch.combMix) * 0.35
                : clamp01(voice.patch.combMix));
            if (combMix > 0.0) {
                const double fb = clamp01(voice.patch.combFeedback) * 0.985;
                voice.combState = value + voice.combState * fb;
                const double combOut = voice.combState * std::cos(std::max(0.01, voice.patch.combTime) * pi * 2.0);
                value = value * (1.0 - combMix) + combOut * combMix;
            }

            value = wavefoldSample(value, voice.patch.wavefold * antiAliasScale);

            const double highPass = clamp01(voice.patch.highPass);
            if (highPass > 0.0) {
                const double hpAlpha = std::clamp(0.002 + highPass * 0.45, 0.002, 0.6);
                voice.highPassState += hpAlpha * (value - voice.highPassState);
                value -= voice.highPassState;
            }

            const double transientDecay = std::max(0.001, voice.patch.transientDecay);
            const double transientEnvelope = voice.transientEnvelopeState;
            const double clickEnvelope = voice.age < 0.002 ? 1.0 - voice.age / 0.002 : 0.0;
            const int burstCount = extremeLoad
                ? 1
                : std::clamp(voice.patch.transientBurstCount, 1, 12);
            const double burstSpacing = std::max(0.0005, voice.patch.transientBurstSpacing);
            const double burstDecay = clamp01(voice.patch.transientBurstDecay);
            double burstEnvelope = 0.0;
            const double transientAmount = clamp01(voice.patch.transientNoise);
            if (transientAmount > 0.0) {
                const double transientWindow = transientDecay * 6.0
                    + burstSpacing * static_cast<double>(std::max(0, burstCount - 1));
                if (voice.age <= transientWindow) {
                    const double expScale = std::exp(burstSpacing / transientDecay);
                    double expTerm = std::exp(-voice.age / transientDecay);
                    double burstDecayMul = 1.0;
                    for (int burst = 0; burst < burstCount; ++burst) {
                        const double start = static_cast<double>(burst) * burstSpacing;
                        if (voice.age < start) {
                            break;
                        }
                        burstEnvelope += expTerm * burstDecayMul;
                        expTerm *= expScale;
                        burstDecayMul *= burstDecay;
                    }
                }
            }
            const double transientTone = clamp01(voice.patch.transientTone);
            const double transientBright = std::pow(transientTone, 1.25);
            const double transientSource = pinkNoise * (1.0 - transientBright * 0.72)
                + whiteNoise * (0.34 + transientBright * 0.66);
            const double transientNoise = sculptNoiseSample(
                transientSource,
                voice.patch.transientTone,
                voice.transientColorState);
            const double transientShape = 1.0 + clamp01(voice.patch.transientShape) * 8.0;
            const double shapedBurst = std::tanh(burstEnvelope * transientShape);
            const double transientBodyAmount = transientAmount
                * (0.18 + clamp01(voice.patch.transientShape) * 0.42)
                * (0.45 + velocityNorm * 0.55);
            const double transientBodyFreq = std::clamp(
                frequency * (1.0 + clamp01(voice.patch.transientShape) * 1.8),
                20.0,
                sampleRate_ * 0.45);
            const double transientBodyInc = std::clamp(transientBodyFreq * invSampleRate, 0.0, 0.49);
            voice.transientBodyPhase = wrapPhase(voice.transientBodyPhase + transientBodyInc);
            const double transientBody = std::sin(voice.transientBodyPhase * twoPi)
                * std::exp(-voice.age / std::max(0.002, transientDecay * 0.7));
            value += clickEnvelope * clamp01(voice.patch.click)
                + transientEnvelope * transientAmount * transientNoise
                + shapedBurst * transientAmount * transientNoise * 0.65
                + transientBody * transientBodyAmount;

            const double drive = std::max(0.0, voice.patch.drive) * (0.72 + antiAliasScale * 0.28);
            const double analogNoise = randomSymmetric(voice.noiseState) * (analogColor * 0.0018);
            value += analogNoise;
            const double preNonlinear = value;
            const double satDrive = drive * (0.85 + analogColor * 0.55);
            const double satAsymmetry = (velocityNorm - 0.5) * 0.45 + voice.patch.toneTilt * 0.2;
            double nonlinear = asymmetricSaturation(preNonlinear, satDrive, satAsymmetry);
            if (!heavyLoad) {
                const double midpoint = 0.5 * (preNonlinear + voice.nonlinearPrevInput);
                const double nonlinearMid = asymmetricSaturation(midpoint, satDrive, satAsymmetry);
                nonlinear = nonlinear * 0.56 + nonlinearMid * 0.44;
            }
            nonlinear = harmonicExciter(nonlinear, analogColor * 0.62 + drive * 0.25);
            voice.nonlinearPrevInput = preNonlinear;
            const double smoothAlpha = nonlinearSmoothingAlpha(drive, analogColor);
            voice.nonlinearSmoothState += smoothAlpha * (nonlinear - voice.nonlinearSmoothState);
            const double smoothMix = std::clamp(0.12 + drive * 0.28 + analogColor * 0.2, 0.0, 0.65);
            nonlinear = nonlinear * (1.0 - smoothMix) + voice.nonlinearSmoothState * smoothMix;
            const double bodyAlpha = std::clamp((twoPi * 140.0) * invSampleRate, 0.002, 0.12);
            voice.lowBodyState += bodyAlpha * (preNonlinear - voice.lowBodyState);
            const double bodyBlend = std::clamp(
                clamp01(voice.patch.subOscillator) * 0.28 + analogColor * 0.25 + drive * 0.08,
                0.0,
                0.45);
            value = nonlinear * (1.0 - bodyBlend) + voice.lowBodyState * bodyBlend;
            const double airAlpha = std::clamp((twoPi * 3200.0) * invSampleRate, 0.02, 0.35);
            voice.airExciterState += airAlpha * (value - voice.airExciterState);
            const double airBand = value - voice.airExciterState;
            const double airAmount = std::clamp(analogColor * 0.18 + drive * 0.12, 0.0, 0.28);
            value += airBand * airAmount;
            value *= (1.0 + drive * 0.34 + analogColor * 0.18);
            value = softKneeLimiter(value, 1.05, 3.2);

            const double toneTilt = clampSigned(voice.patch.toneTilt);
            if (std::abs(toneTilt) > 0.001) {
                const double lowAlpha = std::clamp(0.012 + (1.0 - std::abs(toneTilt)) * 0.06, 0.008, 0.08);
                voice.toneTiltState += lowAlpha * (value - voice.toneTiltState);
                const double low = voice.toneTiltState;
                const double high = value - low;
                const double lowGain = 1.0 + (toneTilt < 0.0 ? -toneTilt * 0.8 : -toneTilt * 0.35);
                const double highGain = 1.0 + (toneTilt > 0.0 ? toneTilt * 0.8 : toneTilt * 0.35);
                value = low * lowGain + high * highGain;
            }
            const double ampEnvelope = voice.ampEnvelopeValue;
            value *= ampEnvelope;
            if (voice.onsetSamplesRemaining > 0 && voice.onsetSamplesTotal > 0) {
                const double progress = 1.0
                    - static_cast<double>(voice.onsetSamplesRemaining) / static_cast<double>(voice.onsetSamplesTotal);
                const double onsetGain = std::clamp(progress * progress * (3.0 - 2.0 * progress), 0.0, 1.0);
                value *= onsetGain;
                --voice.onsetSamplesRemaining;
            }
            if (voice.isStolen) {
                value *= voice.stolenGain;
                voice.stolenGain *= voice.stolenDecayCoefficient;
            }
            value *= 1.0 - clamp01(voice.patch.tremoloDepth) * ((lfo + 1.0) * 0.5);
            value *= voice.patch.gain * voice.note.velocity;
            if (voice.patch.bitCrushEnabled) {
                value = crushSample(value, voice.patch.bitCrush);
            }
            voice.fmFeedbackState = value;

            const double livePan = std::clamp(
                voice.pan + lfo * clamp01(voice.patch.lfoPanDepth) * 0.45,
                -1.0,
                1.0);
            const double leftGain = std::cos((livePan + 1.0) * pi * 0.25);
            const double rightGain = std::sin((livePan + 1.0) * pi * 0.25);
            const double spread = clamp01(voice.patch.stereoSpread);
            const double sideSeed = std::abs(unisonSide) > 1e-6
                ? unisonSide
                : (value - voice.toneTiltState) * 0.42;
            const double sideValue = crushSample(
                std::tanh(sideSeed * (1.0 + drive * 4.0))
                    * ampEnvelope
                    * voice.patch.gain
                    * voice.note.velocity
                    * spread,
                voice.patch.bitCrush);
            double voiceLeft = value * leftGain;
            double voiceRight = value * rightGain;
            voiceLeft -= sideValue * 0.5;
            voiceRight += sideValue * 0.5;
            if (spread > 0.0) {
                const double mid = (voiceLeft + voiceRight) * 0.5;
                double side = (voiceRight - voiceLeft) * 0.5;
                side *= 1.0 + spread * 1.35;
                voiceLeft = mid - side;
                voiceRight = mid + side;
            }

            const double chorusMix = voice.patch.chorusEnabled
                ? (heavyLoad ? clamp01(voice.patch.chorusMix) * 0.4 : clamp01(voice.patch.chorusMix))
                : 0.0;
            if (chorusMix > 0.0) {
                const double chorusLfo = std::sin(voice.chorusPhase * twoPi);
                const double baseDelay = sampleRate_ * 0.014;
                const double depthSamples = sampleRate_ * 0.012 * clamp01(voice.patch.chorusDepth);
                const double delayedLeft = readDelay(
                    voice.chorusLeft,
                    voice.chorusIndex,
                    baseDelay + depthSamples * chorusLfo);
                const double delayedRight = readDelay(
                    voice.chorusRight,
                    voice.chorusIndex,
                    baseDelay - depthSamples * chorusLfo);
                voice.chorusLeft[static_cast<std::size_t>(voice.chorusIndex)] = static_cast<float>(voiceLeft);
                voice.chorusRight[static_cast<std::size_t>(voice.chorusIndex)] = static_cast<float>(voiceRight);
                voice.chorusIndex = (voice.chorusIndex + 1) % static_cast<int>(voice.chorusLeft.size());
                voiceLeft = voiceLeft * (1.0 - chorusMix) + delayedLeft * chorusMix;
                voiceRight = voiceRight * (1.0 - chorusMix) + delayedRight * chorusMix;
            }

            const int holdSamples = 1 + static_cast<int>(clamp01(voice.patch.sampleRateReduction) * 48.0);
            if (holdSamples > 1) {
                if (voice.crushCounter <= 0) {
                    voice.crushHoldLeft = voiceLeft;
                    voice.crushHoldRight = voiceRight;
                    voice.crushCounter = holdSamples;
                }
                voiceLeft = voice.crushHoldLeft;
                voiceRight = voice.crushHoldRight;
                --voice.crushCounter;
            }
            const int busIndex = static_cast<int>(voiceIndex & 0x3u);
            voiceBusLeft[static_cast<std::size_t>(busIndex)] += voiceLeft;
            voiceBusRight[static_cast<std::size_t>(busIndex)] += voiceRight;
            voice.phaseA = voice.unisonPhaseA.front();
            voice.phaseB = voice.unisonPhaseB.front();
            voice.phaseC = voice.unisonPhaseC.front();
            voice.phaseD = voice.unisonPhaseD.front();
            voice.subPhase = wrapPhase(voice.subPhase + (frequency * 0.5) * invSampleRate);
            voice.lfoPhase = wrapPhase(voice.lfoPhase + std::max(0.0, voice.patch.lfoRate) * invSampleRate);
            voice.chorusPhase = wrapPhase(voice.chorusPhase + std::max(0.0, voice.patch.chorusRate) * invSampleRate);
            voice.age += invSampleRate;
            voice.pitchEnvelopeState *= voice.pitchEnvelopeDecayCoefficient;
            voice.transientPitchEnvelopeState *= voice.transientPitchEnvelopeDecayCoefficient;
            voice.transientEnvelopeState *= voice.transientEnvelopeDecayCoefficient;
            voice.ampEnvelopeValue += voice.ampEnvelopeStep;
            voice.filterAlphaValue += voice.filterAlphaStep;
            voice.filterIterAlphaValue += voice.filterIterAlphaStep;
            voice.filterFeedbackValue += voice.filterFeedbackStep;
            voice.filterResonanceCompValue += voice.filterResonanceCompStep;
            if (voice.controlSamplesRemaining > 0) {
                --voice.controlSamplesRemaining;
            }
            ++voice.denormalSanitizeCounter;
            if (voice.denormalSanitizeCounter >= 16 || voice.controlSamplesRemaining <= 0) {
                voice.filterState = sanitizeState(voice.filterState);
                voice.filterState2 = sanitizeState(voice.filterState2);
                voice.filterState3 = sanitizeState(voice.filterState3);
                voice.filterState4 = sanitizeState(voice.filterState4);
                voice.highPassState = sanitizeState(voice.highPassState);
                voice.toneTiltState = sanitizeState(voice.toneTiltState);
                voice.noiseColorState = sanitizeState(voice.noiseColorState);
                voice.transientColorState = sanitizeState(voice.transientColorState);
                voice.nonlinearSmoothState = sanitizeState(voice.nonlinearSmoothState);
                voice.nonlinearPrevInput = sanitizeState(voice.nonlinearPrevInput);
                voice.lowBodyState = sanitizeState(voice.lowBodyState);
                voice.airExciterState = sanitizeState(voice.airExciterState);
                voice.antiAliasState = sanitizeState(voice.antiAliasState);
                voice.antiAliasState2 = sanitizeState(voice.antiAliasState2);
                voice.dividerSmoother = sanitizeState(voice.dividerSmoother);
                voice.pinkB0 = sanitizeState(voice.pinkB0);
                voice.pinkB1 = sanitizeState(voice.pinkB1);
                voice.pinkB2 = sanitizeState(voice.pinkB2);
                voice.pinkB3 = sanitizeState(voice.pinkB3);
                voice.pinkB4 = sanitizeState(voice.pinkB4);
                voice.pinkB5 = sanitizeState(voice.pinkB5);
                voice.pinkB6 = sanitizeState(voice.pinkB6);
                voice.combState = sanitizeState(voice.combState);
                voice.fmFeedbackState = sanitizeState(voice.fmFeedbackState);
                voice.denormalSanitizeCounter = 0;
            }
        }

        double mixedLeft = 0.0;
        double mixedRight = 0.0;
        for (std::size_t bus = 0; bus < voiceBusLeft.size(); ++bus) {
            mixedLeft += voiceBusLeft[bus];
            mixedRight += voiceBusRight[bus];
        }

        mixedLeft *= voiceNorm;
        mixedRight *= voiceNorm;
        const double peak = std::max(std::abs(mixedLeft), std::abs(mixedRight));
        const double peakCoeff = peak > outputPeakFollower_ ? peakAttackCoeff : peakReleaseCoeff;
        outputPeakFollower_ = peak + (outputPeakFollower_ - peak) * peakCoeff;
        const double squared = 0.5 * (mixedLeft * mixedLeft + mixedRight * mixedRight);
        outputRmsFollower_ = squared + (outputRmsFollower_ - squared) * rmsCoeff;
        const double loudnessProxy = std::max(outputPeakFollower_, std::sqrt(std::max(0.0, outputRmsFollower_)) * 1.25);
        const double autoTrim = 1.0 / (1.0 + loudnessProxy * 0.62);
        mixedLeft *= autoTrim;
        mixedRight *= autoTrim;
        blockPreLimiterPeak = std::max(blockPreLimiterPeak, std::max(std::abs(mixedLeft), std::abs(mixedRight)));
        const double targetLimiter = peak > limiterThreshold
            ? limiterThreshold / std::max(peak, 1e-9)
            : 1.0;
        const double coeff = targetLimiter < outputLimiterGain_ ? limiterAttackCoeff : limiterReleaseCoeff;
        outputLimiterGain_ = targetLimiter + (outputLimiterGain_ - targetLimiter) * coeff;
        mixedLeft *= outputLimiterGain_;
        mixedRight *= outputLimiterGain_;
        mixedLeft = softKneeLimiter(mixedLeft, 0.84, 6.4);
        mixedRight = softKneeLimiter(mixedRight, 0.84, 6.4);
        mixedLeft = std::tanh(mixedLeft * 0.70) / 0.70;
        mixedRight = std::tanh(mixedRight * 0.70) / 0.70;
        const double dcLeft = mixedLeft - outputDcInputLeft_ + dcCoeff * outputDcOutputLeft_;
        const double dcRight = mixedRight - outputDcInputRight_ + dcCoeff * outputDcOutputRight_;
        outputDcInputLeft_ = mixedLeft;
        outputDcOutputLeft_ = dcLeft;
        outputDcInputRight_ = mixedRight;
        outputDcOutputRight_ = dcRight;
        blockPostLimiterPeak = std::max(blockPostLimiterPeak, std::max(std::abs(dcLeft), std::abs(dcRight)));
        left[sample] += static_cast<float>(dcLeft);
        right[sample] += static_cast<float>(dcRight);
    }

    voices_.erase(
        std::remove_if(voices_.begin(), voices_.end(), [](const Voice& voice) {
            const double total = voice.gateSeconds + voice.patch.ampEnvelope.release + 0.04;
            if (voice.isStolen && voice.stolenGain < 0.001) {
                return true;
            }
            return voice.age > total;
        }),
        voices_.end());

    const auto renderEnd = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = renderEnd - renderStart;
    const double renderSeconds = static_cast<double>(sampleCount) / std::max(1.0, sampleRate_);
    const double instantLoadPercent = renderSeconds > 1e-9
        ? (elapsed.count() / renderSeconds) * 100.0
        : 0.0;
    smoothedDspLoadPercent_ = smoothedDspLoadPercent_ * 0.86 + instantLoadPercent * 0.14;
    const double peak = std::max(1e-9, outputPeakFollower_);
    const double headroomDb = peak >= 1.0 ? 0.0 : (-20.0 * std::log10(peak));
    const double limiterReductionDb = outputLimiterGain_ >= 0.999999
        ? 0.0
        : (-20.0 * std::log10(std::max(1e-6, outputLimiterGain_)));
    const double preLimiterHeadroomDb = blockPreLimiterPeak >= 1.0
        ? 0.0
        : (-20.0 * std::log10(std::max(1e-9, blockPreLimiterPeak)));
    const double postLimiterHeadroomDb = blockPostLimiterPeak >= 1.0
        ? 0.0
        : (-20.0 * std::log10(std::max(1e-9, blockPostLimiterPeak)));
    const double rms = std::sqrt(std::max(0.0, outputRmsFollower_));
    const double riskScore = std::clamp(
        (smoothedDspLoadPercent_ - 65.0) / 35.0
            + (activeVoiceCount > 0 ? static_cast<double>(activeVoiceCount) / 192.0 : 0.0) * 0.35
            + (limiterReductionDb / 12.0) * 0.15,
        0.0,
        1.0);
    telemetry_.activeVoices = activeVoiceCount;
    telemetry_.sampleCount = sampleCount;
    telemetry_.qualityTier = qualityTier;
    telemetry_.dspLoadPercent = smoothedDspLoadPercent_;
    telemetry_.headroomDb = headroomDb;
    telemetry_.preLimiterHeadroomDb = preLimiterHeadroomDb;
    telemetry_.postLimiterHeadroomDb = postLimiterHeadroomDb;
    telemetry_.limiterReductionDb = limiterReductionDb;
    telemetry_.outputPeak = peak;
    telemetry_.outputRms = rms;
    telemetry_.underrunRisk = riskScore;
    telemetry_.simdReadyPath =
#if defined(__SSE2__) || defined(__AVX__) || defined(__AVX2__)
        true;
#else
        false;
#endif
    telemetry_.deterministicParallelPath = activeVoiceCount > 1;
}

double Synthesizer::sampleOscillator(
    Waveform waveform,
    double phase,
    double phaseIncrement,
    std::uint32_t& noiseState,
    double pulseWidth,
    int triangleHarmonicCap,
    int supersawVoiceCap) const {
    const double bandLimitedIncrement = std::clamp(phaseIncrement, 0.0, 0.49);
    switch (waveform) {
        case Waveform::Sine:
            return std::sin(phase * twoPi);
        case Waveform::Square: {
            const double width = std::clamp(pulseWidth, 0.03, 0.97);
            double value = phase < width ? 1.0 : -1.0;
            value += polyBlep(phase, bandLimitedIncrement);
            double trailing = phase - width;
            if (trailing < 0.0) {
                trailing += 1.0;
            }
            value -= polyBlep(trailing, bandLimitedIncrement);
            return value;
        }
        case Waveform::Saw:
            return (phase * 2.0 - 1.0) - polyBlep(phase, bandLimitedIncrement);
        case Waveform::Triangle:
            return bandLimitedTriangle(phase, bandLimitedIncrement, triangleHarmonicCap);
        case Waveform::Noise:
            noiseState = noiseState * 1664525u + 1013904223u;
            return (static_cast<double>((noiseState >> 8) & 0x00ffffff) / 8388607.5) - 1.0;
        case Waveform::SuperSaw: {
            constexpr std::array<double, 7> detunes {-0.045, -0.030, -0.016, 0.0, 0.016, 0.030, 0.045};
            constexpr std::array<double, 7> weights {0.60, 0.82, 1.02, 1.25, 1.02, 0.82, 0.60};
            const double detuneScale = std::clamp(1.0 - bandLimitedIncrement * 1.8, 0.35, 1.0);
            double sum = 0.0;
            double weightSum = 0.0;
            const int cappedVoices = std::clamp(supersawVoiceCap, 1, static_cast<int>(detunes.size()));
            for (int index = 0; index < cappedVoices; ++index) {
                const double d = detunes[index] * detuneScale;
                const double localInc = std::clamp(bandLimitedIncrement * (1.0 + d), 0.0, 0.49);
                const double drift = std::sin((phase * twoPi) + static_cast<double>(index) * 0.73) * 0.0025;
                const double localPhase = wrapPhase(phase + (static_cast<double>(index) * 0.137) + d * 0.35 + drift);
                const double saw = (localPhase * 2.0 - 1.0) - polyBlep(localPhase, localInc);
                sum += saw * weights[index];
                weightSum += weights[index];
            }
            const double normalized = weightSum > 0.0 ? (sum / weightSum) : 0.0;
            return std::tanh(normalized * 1.35) * 0.93;
        }
    }
    return 0.0;
}

double Synthesizer::envelopeFor(const Envelope& envelope, double age, double gateSeconds) const {
    auto levelDuringGate = [&](double t) {
        const double attack = std::max(envelope.attack, 0.0001);
        const double decay = std::max(envelope.decay, 0.0001);
        const double sustain = clamp01(envelope.sustain);
        if (t <= 0.0) {
            return 0.0;
        }
        if (t < attack) {
            const double x = std::clamp(t / attack, 0.0, 1.0);
            return x * x * (3.0 - 2.0 * x);
        }
        if (t < attack + decay) {
            const double x = std::clamp((t - attack) / decay, 0.0, 1.0);
            return sustain + (1.0 - sustain) * std::exp(-4.2 * x);
        }
        return sustain;
    };

    const double gatedUntil = std::max(0.0, gateSeconds);
    if (age <= gatedUntil) {
        return levelDuringGate(age);
    }

    const double release = std::max(envelope.release, 0.0001);
    const double releaseStart = levelDuringGate(gatedUntil);
    const double t = (age - gatedUntil) / release;
    if (t >= 1.0) {
        return 0.0;
    }
    const double released = releaseStart * std::exp(-6.5 * t);
    if (released < 1e-6) {
        return 0.0;
    }
    return released;
}

} // namespace arachno
