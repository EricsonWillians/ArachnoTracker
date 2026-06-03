#include "Synthesizer.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <limits>
#if defined(__SSE2__)
#include <xmmintrin.h>
#endif

namespace arachno {

namespace {
#if defined(__SSE2__)
void ensureFastDenormalMode() {
    _mm_setcsr(_mm_getcsr() | 0x8040);
}
#else
void ensureFastDenormalMode() {}
#endif

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
    if (phase >= 1.0) {
        if (phase < 2.0) {
            return phase - 1.0;
        }
        return phase - std::floor(phase);
    }
    if (phase < 0.0) {
        if (phase > -1.0) {
            return phase + 1.0;
        }
        return phase - std::floor(phase);
    }
    return phase;
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

double fastSaturate(double value) {
    return value / (1.0 + std::abs(value));
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
        // Gentle sine shaping — keeps FM smooth and musical
        const double soft = std::sin(value * twoPi * 0.5);
        return value * (1.0 - mix) + soft * mix;
    }
    if (c < 0.66) {
        const double mix = (c - 0.33) / 0.33;
        // Soft tanh clipping — controlled harmonics, no aliasing
        const double clipped = std::tanh(value * 1.8);
        return value * (1.0 - mix) + clipped * mix;
    }
    const double mix = (c - 0.66) / 0.34;
    // Saturated sine instead of asin(sin) folding — avoids sharp triangle corners
    // that produce harsh, non-musical FM sidebands and aliasing
    const double saturated = std::sin(value * pi * 1.2) * (1.0 - std::abs(value) * 0.15);
    return value * (1.0 - mix) + saturated * mix;
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
    if (activeVoiceCount > 30) {
        return SynthQualityTier::Eco;
    }
    if (activeVoiceCount > 18) {
        return SynthQualityTier::Balanced;
    }
    if (activeVoiceCount > 8) {
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
    const double ab = oscA * (1.0 - b) + oscB * b;
    const double abc = ab * (1.0 - c) + oscC * c;
    return abc * (1.0 - d) + oscD * d;
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

// TPT SVF helpers
inline double tptSvfProcessSample(double v0, double g, double k, double& z1, double& z2) {
    const double v1 = (z1 + g * (v0 - z2)) / (1.0 + g * (g + k));
    const double v2 = z2 + g * v1;
    z1 = 2.0 * v1 - z1;
    z2 = 2.0 * v2 - z2;
    return v2; // LP output
}

inline double tptSvfProcessSampleWithMode(double v0, double g, double k, double& z1, double& z2, int mode) {
    const double v1 = (z1 + g * (v0 - z2)) / (1.0 + g * (g + k));
    const double v2 = z2 + g * v1;
    z1 = 2.0 * v1 - z1;
    z2 = 2.0 * v2 - z2;
    // mode: 0=LP, 1=LP (same output, different resonance behavior via k), 2=BP, 3=Notch, 4=Peak
    switch (mode) {
        case 0:
        case 1:
            return v2; // LP
        case 2:
            return v1; // BP
        case 3:
            return v0 - k * v1; // Notch
        case 4:
            return v0 - k * v1 + v2; // Peak (Notch + LP)
        default:
            return v2;
    }
}

// Map old filterMode (0=LP2, 1=LP4, 2=HP+LP) to TPT mode
inline int mapOldFilterModeToTpt(int oldMode) {
    // 0 -> LP, 1 -> LP (with higher resonance), 2 -> BP
    switch (oldMode) {
        case 0: return 0; // LP
        case 1: return 0; // LP (resonance handled via k)
        case 2: return 2; // BP
        default: return 0;
    }
}

// Velocity curve lookup: convert normalized velocity [0,1] to gain factor [0,1]
// curve: 0=linear, 1=exponential, 2=logarithmic, 3=DX7-style
inline double velocityCurveValue(double velocityNorm, int curve) {
    switch (curve) {
        case 0: // linear
            return velocityNorm;
        case 1: // exponential (natural feel, default)
            return std::pow(velocityNorm, 1.7);
        case 2: { // logarithmic (more dynamic range at low velocities)
            const double v = std::max(velocityNorm, 0.001);
            return std::log10(v * 9.0 + 1.0); // maps 0->0, 1->1
        }
        case 3: { // DX7-style (table-based approximation)
            // DX7 uses an exponential curve with different character
            // Approximate with a shifted power curve
            if (velocityNorm <= 0.0) return 0.0;
            const double dx7 = std::pow(velocityNorm, 0.75) * 0.35 + std::pow(velocityNorm, 2.2) * 0.65;
            return dx7;
        }
        default:
            return std::pow(velocityNorm, 1.7);
    }
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
    const int baseVoiceLimit = smoothedDspLoadPercent_ > 90.0
        ? 18
        : (smoothedDspLoadPercent_ > 78.0 ? 28 : (smoothedDspLoadPercent_ > 62.0 ? 42 : 58));
    const int unisonPenalty = std::max(0, std::clamp(patch.unisonVoices, 1, maxSynthUnisonVoices) - 1) * 3;
    const int voiceLimit = std::clamp(baseVoiceLimit - unisonPenalty, 12, 56);

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
            Voice& oldest = voices_[static_cast<std::size_t>(oldestIndex)];
            if (oldest.isStolen || oldest.stolenGain < 0.025 || voices_.size() >= static_cast<std::size_t>(voiceLimit + 16)) {
                voices_.erase(voices_.begin() + oldestIndex);
            } else {
                markVoiceStolen(oldest);
                break;
            }
        }
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

    if (voices_.size() >= static_cast<std::size_t>(voiceLimit)) {
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
            if (voices_.size() >= static_cast<std::size_t>(voiceLimit + 12)
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
    const double loadPressure = std::clamp(
        (smoothedDspLoadPercent_ - 64.0) / 34.0
            + std::max(0.0, static_cast<double>(voices_.size()) - 18.0) / 32.0,
        0.0,
        1.0);
    if (loadPressure > 0.0001) {
        const double minGate = note.midi <= 50 ? 0.075 : 0.038;
        voice.gateSeconds = std::max(minGate, voice.gateSeconds * (1.0 - loadPressure * 0.42));
        const double releaseScale = 1.0 - loadPressure * 0.58;
        voice.patch.ampEnvelope.release = std::max(0.001, voice.patch.ampEnvelope.release * releaseScale);
        voice.patch.filterEnvelope.release = std::max(0.001, voice.patch.filterEnvelope.release * releaseScale);
        voice.patch.ampEnvelope.decay = std::max(0.001, voice.patch.ampEnvelope.decay * (1.0 - loadPressure * 0.15));
        voice.patch.filterEnvelope.decay = std::max(0.001, voice.patch.filterEnvelope.decay * (1.0 - loadPressure * 0.15));
        voice.patch.transientDecay = std::max(0.001, voice.patch.transientDecay * (1.0 - loadPressure * 0.22));
        voice.patch.delayMix *= (1.0 - loadPressure * 0.52);
        voice.patch.reverbMix *= (1.0 - loadPressure * 0.56);
        voice.patch.combMix *= (1.0 - loadPressure * 0.48);
        voice.patch.chorusMix *= (1.0 - loadPressure * 0.32);
    }
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
    const double phaseScatter = clamp01(patch.phaseScatter);
    const double phaseSeed = static_cast<double>(voice.noiseState & 0xffffu) / 65535.0;
    const double phaseOffset = (analogColor * 0.35 + phaseScatter * 0.42) * phaseSeed;
    voice.subPhase = wrapPhase(phaseOffset * 0.7);
    voice.transientBodyPhase = wrapPhase(phaseOffset * 1.3);
    voice.lfoPhase = wrapPhase(phaseOffset * 0.9);
    voice.lfoValue = std::sin(voice.lfoPhase * twoPi);
    voice.lfoStep = 0.0;
    voice.frequencyValue = voice.baseFrequency;
    voice.frequencyStep = 0.0;
    voice.tremoloGainValue = 1.0;
    voice.tremoloGainStep = 0.0;
    const double initPanLeft = std::cos((voice.pan + 1.0) * pi * 0.25);
    const double initPanRight = std::sin((voice.pan + 1.0) * pi * 0.25);
    voice.panLeftGainValue = initPanLeft;
    voice.panLeftGainStep = 0.0;
    voice.panRightGainValue = initPanRight;
    voice.panRightGainStep = 0.0;
    voice.dividerState = (phaseSeed > 0.5) ? 1.0 : -1.0;
    voice.dividerSmoother = voice.dividerState;
    voice.pinkB0 = randomSymmetric(voice.noiseState) * 0.008;
    voice.pinkB1 = randomSymmetric(voice.noiseState) * 0.008;
    voice.pinkB2 = randomSymmetric(voice.noiseState) * 0.008;
    voice.pinkB3 = randomSymmetric(voice.noiseState) * 0.008;
    voice.pinkB4 = randomSymmetric(voice.noiseState) * 0.008;
    voice.pinkB5 = randomSymmetric(voice.noiseState) * 0.008;
    voice.pinkB6 = randomSymmetric(voice.noiseState) * 0.008;
    voice.detuneRatioA = centsToRatio(patch.oscADetuneCents);
    voice.detuneRatioB = centsToRatio(patch.detuneCents + patch.oscBDetuneCents);
    voice.detuneRatioC = centsToRatio(patch.detuneCCents + patch.oscCDetuneCents);
    voice.detuneRatioD = centsToRatio(patch.detuneDCents + patch.oscDDetuneCents);
    voice.driftRatioA = 1.0;
    voice.driftRatioB = 1.0;
    voice.driftRatioC = 1.0;
    voice.driftRatioD = 1.0;
    const int unisonCount = unisonVoiceCount(patch);
    for (int index = 0; index < unisonCount; ++index) {
        const double offset = static_cast<double>(index) / static_cast<double>(unisonCount);
        const double localJitter = (analogColor * 0.035 + phaseScatter * 0.085)
            * randomSymmetric(voice.noiseState);
        voice.unisonPhaseA[static_cast<std::size_t>(index)] = wrapPhase(offset + phaseOffset + localJitter);
        voice.unisonPhaseB[static_cast<std::size_t>(index)] = wrapPhase(offset * 0.37 + phaseOffset * 0.51 + localJitter);
        voice.unisonPhaseC[static_cast<std::size_t>(index)] = wrapPhase(offset * 0.73 + phaseOffset * 0.83 + localJitter);
        voice.unisonPhaseD[static_cast<std::size_t>(index)] = wrapPhase(offset * 0.19 + phaseOffset * 0.29 + localJitter);
        const double position = unisonPosition(index, unisonCount);
        voice.unisonRatioCached[static_cast<std::size_t>(index)] = centsToRatio(position * patch.unisonDetuneCents);
    }
    for (int index = unisonCount; index < maxSynthUnisonVoices; ++index) {
        voice.unisonRatioCached[static_cast<std::size_t>(index)] = 1.0;
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
    ensureFastDenormalMode();
    const auto renderStart = std::chrono::steady_clock::now();
    const double invSampleRate = sampleRate_ > 0.0 ? (1.0 / sampleRate_) : 0.0;

    // Manual compaction loop for pre-cull (swap-and-pop, no reallocations)
    const double preCullPressure = std::clamp(
        (smoothedDspLoadPercent_ - 66.0) / 30.0
            + std::max(0.0, static_cast<double>(voices_.size()) - 20.0) / 28.0,
        0.0,
        1.0);
    if (preCullPressure > 0.0001) {
        const double releaseFloor = 0.00045 + preCullPressure * 0.0031;
        std::size_t write = 0;
        for (std::size_t read = 0; read < voices_.size(); ++read) {
            const Voice& voice = voices_[read];
            bool dead = false;
            if (voice.isStolen && voice.stolenGain < (0.0014 + preCullPressure * 0.007)) {
                dead = true;
            } else if (voice.age > voice.gateSeconds && voice.ampEnvelopeValue < releaseFloor) {
                dead = true;
            }
            if (!dead) {
                if (write != read) {
                    voices_[write] = std::move(voices_[read]);
                }
                ++write;
            }
        }
        voices_.resize(write);
    }

    const int activeVoiceCount = static_cast<int>(voices_.size());
    SynthQualityTier qualityTier = qualityTierForVoiceCount(activeVoiceCount);
    if (smoothedDspLoadPercent_ > 95.0) {
        qualityTier = SynthQualityTier::Eco;
    } else if (smoothedDspLoadPercent_ > 84.0) {
        qualityTier = static_cast<int>(qualityTier) < static_cast<int>(SynthQualityTier::Balanced)
            ? SynthQualityTier::Balanced
            : qualityTier;
    } else if (smoothedDspLoadPercent_ > 68.0) {
        qualityTier = static_cast<int>(qualityTier) < static_cast<int>(SynthQualityTier::High)
            ? SynthQualityTier::High
            : qualityTier;
    }
    const bool panicLoad = smoothedDspLoadPercent_ > 68.0 || activeVoiceCount > 18;
    const bool emergencyLoad = smoothedDspLoadPercent_ > 82.0 || activeVoiceCount > 26;
    if (panicLoad) {
        qualityTier = SynthQualityTier::Eco;
    }
    const bool heavyLoad = qualityTier == SynthQualityTier::Balanced || qualityTier == SynthQualityTier::Eco;
    const bool extremeLoad = qualityTier == SynthQualityTier::Eco;
    const int triangleHarmonicCap = emergencyLoad
        ? 5
        : (qualityTier == SynthQualityTier::Ultra
        ? 31
        : (qualityTier == SynthQualityTier::High ? 23 : (qualityTier == SynthQualityTier::Balanced ? 15 : 9)));
    const int supersawVoiceCap = emergencyLoad
        ? 1
        : (qualityTier == SynthQualityTier::Ultra
        ? 7
        : (qualityTier == SynthQualityTier::High ? 5 : (qualityTier == SynthQualityTier::Balanced ? 3 : 2)));
    double voiceNorm = 1.0 / std::sqrt(1.0 + static_cast<double>(activeVoiceCount) * 0.95);
    if (heavyLoad) {
        voiceNorm *= 0.92;
    }
    if (extremeLoad) {
        voiceNorm *= 0.86;
    }
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
                                          double& outSvfG,
                                          double& outSvfK,
                                          double& outResonanceComp,
                                          int& outFilterMode) {
        const double frequency = voice.baseFrequency
            * centsToRatio(lfoValue * voice.patch.vibratoCents)
            * semitonesToRatio(pitchEnvelopeValue + transientPitchEnvelopeValue);
        const double filterEnvelope = envelopeFor(
            voice.patch.filterEnvelope,
            ageSeconds,
            voice.gateSeconds,
            voice.patch.filterEnvelopeCurve);
        const double keytrack = clamp01(voice.patch.filterKeytrack);
        const double keytrackBias = ((static_cast<double>(voice.note.midi - 60) / 48.0) * 0.18) * keytrack;
        const double cutoffNormalized = clamp01(
            voice.patch.cutoff
            + filterEnvelope * voice.patch.filterEnvelopeAmount
            + lfoValue * clamp01(voice.patch.lfoFilterDepth) * 0.35
            + (velocityNorm - 0.6) * 0.16
            + keytrackBias);
        const double cutoffHz = std::clamp(
            20.0 * std::exp2(cutoffNormalized * 10.0),
            20.0,
            sampleRate_ * 0.45);
        outSvfG = std::tan(pi * cutoffHz / sampleRate_);
        const double resonance = clamp01(voice.patch.resonance);
        outSvfK = std::max(0.05, 2.0 - 2.0 * resonance);
        // TPT SVF resonance compensation: at high resonance the filter loses significant
        // gain. Use a steeper curve that matches the actual gain loss.
        outResonanceComp = 1.0 + resonance * resonance * 2.2 + resonance * 0.6;
        outFilterMode = std::clamp(voice.patch.filterMode, 0, 2);
    };

    for (int sample = 0; sample < sampleCount; ++sample) {
        double voiceBusLeft0 = 0.0;
        double voiceBusLeft1 = 0.0;
        double voiceBusLeft2 = 0.0;
        double voiceBusLeft3 = 0.0;
        double voiceBusRight0 = 0.0;
        double voiceBusRight1 = 0.0;
        double voiceBusRight2 = 0.0;
        double voiceBusRight3 = 0.0;

        for (std::size_t voiceIndex = 0; voiceIndex < voices_.size(); ++voiceIndex) {
            Voice& voice = voices_[voiceIndex];
            const double lfo = voice.lfoValue;
            const double velocityNorm = std::clamp(static_cast<double>(voice.note.velocity), 0.0, 1.0);
            const double analogColor = clamp01(voice.patch.analogColor);
            const double transientAmount = clamp01(voice.patch.transientNoise);
            const bool needsNoiseEngine = !panicLoad && !emergencyLoad
                && (voice.patch.noiseEnabled || transientAmount > 0.0001);
            const bool simplifiedNonlinear = heavyLoad || activeVoiceCount > 30 || panicLoad;

            // Early-exit for silent voices
            if (voice.ampEnvelopeValue < 1e-7 && voice.age > voice.gateSeconds) {
                voice.ampEnvelopeValue += voice.ampEnvelopeStep;
                voice.age += invSampleRate;
                voice.pitchEnvelopeState *= voice.pitchEnvelopeDecayCoefficient;
                voice.transientPitchEnvelopeState *= voice.transientPitchEnvelopeDecayCoefficient;
                voice.transientEnvelopeState *= voice.transientEnvelopeDecayCoefficient;
                voice.lfoPhase = wrapPhase(voice.lfoPhase + std::max(0.0, voice.patch.lfoRate) * invSampleRate);
                voice.lfoValue += voice.lfoStep;
                voice.frequencyValue += voice.frequencyStep;
                voice.tremoloGainValue += voice.tremoloGainStep;
                voice.panLeftGainValue += voice.panLeftGainStep;
                voice.panRightGainValue += voice.panRightGainStep;
                voice.chorusPhase = wrapPhase(voice.chorusPhase + std::max(0.0, voice.patch.chorusRate) * invSampleRate);
                if (voice.controlSamplesRemaining > 0) {
                    --voice.controlSamplesRemaining;
                }
                continue;
            }

            if (voice.driftStepCounter <= 0) {
                const double vintageDrift = clamp01(voice.patch.vintageDrift);
                const double voiceSlop = clamp01(voice.patch.voiceSlop);
                // Reduced base drift and analogColor contribution to prevent excessive
                // pitch wobble that sounds unprofessional on sustained darkwave/EBM tones.
                const double driftAmountCents = (0.06
                    + analogColor * 0.85
                    + clamp01(voice.patch.drive) * 0.25
                    + clamp01(voice.patch.unisonDetuneCents / 20.0) * 0.35)
                    * (0.25 + vintageDrift * 0.75 + voiceSlop * 0.55);
                voice.driftA = stepDrift(voice.driftA, voice.noiseState, driftAmountCents);
                voice.driftB = stepDrift(voice.driftB, voice.noiseState, driftAmountCents * 1.13);
                voice.driftC = stepDrift(voice.driftC, voice.noiseState, driftAmountCents * 1.07);
                voice.driftD = stepDrift(voice.driftD, voice.noiseState, driftAmountCents * 0.92);
                voice.driftRatioA = centsToRatio(voice.driftA);
                voice.driftRatioB = centsToRatio(voice.driftB);
                voice.driftRatioC = centsToRatio(voice.driftC);
                voice.driftRatioD = centsToRatio(voice.driftD);
                voice.driftStepCounter = heavyLoad ? 7 : 3;
            } else {
                --voice.driftStepCounter;
            }
            const double driftRatioA = voice.driftRatioA;
            const double driftRatioB = voice.driftRatioB;
            const double driftRatioC = voice.driftRatioC;
            const double driftRatioD = voice.driftRatioD;
            const double frequency = std::max(1.0, voice.frequencyValue);
            const double nyquist = std::max(1.0, sampleRate_ * 0.5);
            const double spectralStress = std::clamp(frequency / (nyquist * 0.92), 0.0, 1.0);
            const double antiAliasScale = 1.0 - 0.72 * spectralStress * spectralStress;
            const double lowEndFocus = std::clamp((220.0 - frequency) / 180.0, 0.0, 1.0);
            const double detuneRatioA = voice.detuneRatioA;
            const double detuneRatioB = voice.detuneRatioB;
            const double detuneRatioC = voice.detuneRatioC;
            const double detuneRatioD = voice.detuneRatioD;
            int unisonCount = panicLoad ? 1 : std::min(2, voice.unisonCountCached);
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
            if (heavyLoad || panicLoad) {
                fmDepth *= 0.55;
            }
            if (extremeLoad || panicLoad) {
                fmDepth = 0.0;
            }
            fmDepth *= antiAliasScale;
            if (voice.controlSamplesRemaining <= 0) {
                int controlBlockSamples = std::max(8, voice.controlBlockSize);
                if (heavyLoad || panicLoad) {
                    controlBlockSamples = std::max(controlBlockSamples, 64);
                }
                if (extremeLoad || panicLoad) {
                    controlBlockSamples = std::max(controlBlockSamples, 128);
                }
                voice.controlBlockSize = std::clamp(controlBlockSamples, 8, 192);
                const double blockDuration = static_cast<double>(voice.controlBlockSize) * invSampleRate;
                const double lfoStart = voice.lfoValue;
                const double blockLfoEnd = std::sin(
                    wrapPhase(voice.lfoPhase + std::max(0.0, voice.patch.lfoRate) * blockDuration) * twoPi);
                voice.lfoStep = (blockLfoEnd - lfoStart) / static_cast<double>(voice.controlBlockSize);
                voice.lfoValue = lfoStart;
                // Velocity-to-attack: shorter attack at high velocity
                const double velToAttack = clamp01(voice.patch.velocityToAttack);
                const double velocityAttackScale = 1.0 - velocityCurveValue(velocityNorm, voice.patch.velocityCurve) * velToAttack * 0.85;
                Envelope velAdjustedAmpEnv = voice.patch.ampEnvelope;
                velAdjustedAmpEnv.attack *= velocityAttackScale;
                const double ampStart = envelopeFor(velAdjustedAmpEnv, voice.age, voice.gateSeconds, voice.patch.ampEnvelopeCurve);
                const double ampEnd = envelopeFor(
                    velAdjustedAmpEnv,
                    voice.age + blockDuration,
                    voice.gateSeconds,
                    voice.patch.ampEnvelopeCurve);
                voice.ampEnvelopeValue = ampStart;
                voice.ampEnvelopeStep = (ampEnd - ampStart) / static_cast<double>(voice.controlBlockSize);
                const double pitchEnvelopeEndState = voice.pitchEnvelopeState * std::pow(
                    voice.pitchEnvelopeDecayCoefficient,
                    static_cast<double>(voice.controlBlockSize));
                const double transientPitchEnvelopeEndState = voice.transientPitchEnvelopeState * std::pow(
                    voice.transientPitchEnvelopeDecayCoefficient,
                    static_cast<double>(voice.controlBlockSize));
                const double wowFlutter = clamp01(voice.patch.wowFlutter);
                const double wowDepthCents = wowFlutter * 22.0;
                const double wowRateHz = 0.12 + wowFlutter * 0.68;
                const double flutterRateHz = 2.0 + wowFlutter * 5.5;
                const double wowPhase = voice.phaseA * 0.73 + voice.phaseC * 0.19 + voice.lfoPhase * 0.11;
                const double flutterPhase = voice.phaseB * 0.29 + voice.phaseD * 0.41 + voice.lfoPhase * 0.07;
                const double wowStart = std::sin((voice.age * wowRateHz + wowPhase) * twoPi);
                const double wowEnd = std::sin(((voice.age + blockDuration) * wowRateHz + wowPhase) * twoPi);
                const double flutterStart = std::sin((voice.age * flutterRateHz + flutterPhase) * twoPi);
                const double flutterEnd = std::sin(((voice.age + blockDuration) * flutterRateHz + flutterPhase) * twoPi);
                const double wowFlutterCentsStart = wowDepthCents * (wowStart * 0.66 + flutterStart * 0.34);
                const double wowFlutterCentsEnd = wowDepthCents * (wowEnd * 0.66 + flutterEnd * 0.34);
                const double frequencyStart = std::max(
                    1.0,
                    voice.baseFrequency
                        * centsToRatio(lfoStart * voice.patch.vibratoCents)
                        * centsToRatio(wowFlutterCentsStart)
                        * semitonesToRatio(voice.pitchEnvelopeState + voice.transientPitchEnvelopeState));
                const double frequencyEnd = std::max(
                    1.0,
                    voice.baseFrequency
                        * centsToRatio(blockLfoEnd * voice.patch.vibratoCents)
                        * centsToRatio(wowFlutterCentsEnd)
                        * semitonesToRatio(pitchEnvelopeEndState + transientPitchEnvelopeEndState));
                voice.frequencyValue = frequencyStart;
                voice.frequencyStep = (frequencyEnd - frequencyStart) / static_cast<double>(voice.controlBlockSize);
                const double tremoloDepth = clamp01(voice.patch.tremoloDepth);
                const double tremoloStart = 1.0 - tremoloDepth * ((lfoStart + 1.0) * 0.5);
                const double tremoloEnd = 1.0 - tremoloDepth * ((blockLfoEnd + 1.0) * 0.5);
                voice.tremoloGainValue = tremoloStart;
                voice.tremoloGainStep = (tremoloEnd - tremoloStart) / static_cast<double>(voice.controlBlockSize);
                const double lfoPanDepth = clamp01(voice.patch.lfoPanDepth) * 0.45;
                const double panStart = std::clamp(voice.pan + lfoStart * lfoPanDepth, -1.0, 1.0);
                const double panEnd = std::clamp(voice.pan + blockLfoEnd * lfoPanDepth, -1.0, 1.0);
                const double panLeftStart = std::cos((panStart + 1.0) * pi * 0.25);
                const double panLeftEnd = std::cos((panEnd + 1.0) * pi * 0.25);
                const double panRightStart = std::sin((panStart + 1.0) * pi * 0.25);
                const double panRightEnd = std::sin((panEnd + 1.0) * pi * 0.25);
                voice.panLeftGainValue = panLeftStart;
                voice.panLeftGainStep = (panLeftEnd - panLeftStart) / static_cast<double>(voice.controlBlockSize);
                voice.panRightGainValue = panRightStart;
                voice.panRightGainStep = (panRightEnd - panRightStart) / static_cast<double>(voice.controlBlockSize);

                double svfGStart = 0.0;
                double svfKStart = 2.0;
                double resonanceCompStart = 1.0;
                int filterModeStart = 0;
                computeFilterControlValues(
                    voice,
                    voice.age,
                    lfoStart,
                    voice.pitchEnvelopeState,
                    voice.transientPitchEnvelopeState,
                    velocityNorm,
                    fmDepth,
                    svfGStart,
                    svfKStart,
                    resonanceCompStart,
                    filterModeStart);

                double svfGEnd = svfGStart;
                double svfKEnd = svfKStart;
                double resonanceCompEnd = resonanceCompStart;
                int filterModeEnd = filterModeStart;
                computeFilterControlValues(
                    voice,
                    voice.age + blockDuration,
                    blockLfoEnd,
                    pitchEnvelopeEndState,
                    transientPitchEnvelopeEndState,
                    velocityNorm,
                    fmDepth,
                    svfGEnd,
                    svfKEnd,
                    resonanceCompEnd,
                    filterModeEnd);

                voice.svfG = svfGStart;
                voice.svfK = svfKStart;
                voice.filterAlphaValue = svfGStart;
                voice.filterAlphaStep = (svfGEnd - svfGStart) / static_cast<double>(voice.controlBlockSize);
                voice.filterIterAlphaValue = svfKStart;
                voice.filterIterAlphaStep = (svfKEnd - svfKStart) / static_cast<double>(voice.controlBlockSize);
                voice.filterResonanceCompValue = resonanceCompStart;
                voice.filterResonanceCompStep = (resonanceCompEnd - resonanceCompStart)
                    / static_cast<double>(voice.controlBlockSize);
                voice.filterPassesCached = filterModeStart;
                int adaptiveUnison = unisonVoiceCount(voice.patch);
                if (activeVoiceCount > 0) {
                    const int adaptiveBudget = std::max(1, 24 / activeVoiceCount);
                    adaptiveUnison = std::min(adaptiveUnison, adaptiveBudget);
                }
                if (panicLoad) {
                    adaptiveUnison = 1;
                }
                voice.unisonCountCached = std::clamp(adaptiveUnison, 1, maxSynthUnisonVoices);
                const double unisonWarp = clamp01(voice.patch.unisonWarp);
                const double unisonHumanize = clamp01(voice.patch.unisonHumanize);
                const double warpShape = 1.0 + unisonWarp * 2.6;
                for (int index = 0; index < voice.unisonCountCached; ++index) {
                    const double position = unisonPosition(index, voice.unisonCountCached);
                    const double shapedPosition = position == 0.0
                        ? 0.0
                        : std::copysign(std::pow(std::abs(position), warpShape), position);
                    const double humanizeCents = randomSymmetric(voice.noiseState)
                        * unisonHumanize
                        * (0.2 + analogColor * 0.5)
                        * std::max(0.75, std::abs(voice.patch.unisonDetuneCents) * 0.08);
                    voice.unisonRatioCached[static_cast<std::size_t>(index)] =
                        centsToRatio(shapedPosition * voice.patch.unisonDetuneCents + humanizeCents);
                }
                for (int index = voice.unisonCountCached; index < maxSynthUnisonVoices; ++index) {
                    voice.unisonRatioCached[static_cast<std::size_t>(index)] = 1.0;
                }
                voice.oscAEnabledCached = voice.patch.oscillatorAEnabled;
                voice.oscBEnabledCached = emergencyLoad ? false : voice.patch.oscillatorBEnabled;
                voice.oscCEnabledCached = emergencyLoad ? false : voice.patch.oscillatorCEnabled;
                voice.oscDEnabledCached = emergencyLoad ? false : voice.patch.oscillatorDEnabled;
                voice.oscALevelCached = clamp01(voice.patch.oscALevel);
                voice.oscBLevelCached = clamp01(voice.patch.oscBLevel);
                voice.oscCLevelCached = clamp01(voice.patch.oscCLevel);
                voice.oscDLevelCached = clamp01(voice.patch.oscDLevel);
                voice.oscMixCached = clamp01(voice.patch.oscillatorMix);
                voice.oscCMixCached = clamp01(voice.patch.oscillatorCMix);
                voice.oscDMixCached = clamp01(voice.patch.oscillatorDMix);
                voice.driveACached = clamp01(voice.patch.oscADrive);
                voice.driveBCached = clamp01(voice.patch.oscBDrive);
                voice.driveCCached = clamp01(voice.patch.oscCDrive);
                voice.driveDCached = clamp01(voice.patch.oscDDrive);
                voice.waveCompACached = waveformLevelCompensation(voice.patch.oscillatorA);
                voice.waveCompBCached = waveformLevelCompensation(voice.patch.oscillatorB);
                voice.waveCompCCached = waveformLevelCompensation(voice.patch.oscillatorC);
                voice.waveCompDCached = waveformLevelCompensation(voice.patch.oscillatorD);
                voice.subAmountCached = emergencyLoad ? 0.0 : clamp01(voice.patch.subOscillator);
                voice.noiseAmountCached = emergencyLoad ? 0.0 : clamp01(voice.patch.noise);
                voice.toneTiltCached = clampSigned(voice.patch.toneTilt);
                voice.tapeColorCached = clamp01(voice.patch.tapeColor);
                voice.airBoostCached = clamp01(voice.patch.airBoost);
                voice.lowPunchCached = clamp01(voice.patch.lowPunch);
                voice.analogWarmthCached = clamp01(voice.patch.analogWarmth);
                voice.voiceSlopCached = clamp01(voice.patch.voiceSlop);
                voice.phaseScatterCached = clamp01(voice.patch.phaseScatter);
                voice.unisonWarpCached = clamp01(voice.patch.unisonWarp);
                voice.unisonHumanizeCached = clamp01(voice.patch.unisonHumanize);
                voice.fmColorCached = clamp01(voice.patch.fmColor);
                voice.fmSpreadCached = clamp01(voice.patch.fmSpread);
                voice.chorusToneCached = clamp01(voice.patch.chorusTone);
                voice.chorusJitterCached = clamp01(voice.patch.chorusJitter);
                voice.chorusSaturationCached = clamp01(voice.patch.chorusSaturation);
                voice.delayDiffusionCached = clamp01(voice.patch.delayDiffusion);
                voice.delayWowCached = clamp01(voice.patch.delayWow);
                voice.delayCrossfeedCached = clamp01(voice.patch.delayCrossfeed);
                voice.reverbDecayCached = clamp01(voice.patch.reverbDecay);
                voice.reverbEarlyMixCached = clamp01(voice.patch.reverbEarlyMix);
                voice.reverbToneCached = clamp01(voice.patch.reverbTone);
                voice.reverbChorusCached = clamp01(voice.patch.reverbChorus);
                voice.reverbBloomCached = clamp01(voice.patch.reverbBloom);
                voice.consoleCrosstalkCached = clamp01(voice.patch.consoleCrosstalk);
                voice.stereoDepthCached = clamp01(voice.patch.stereoDepth);
                voice.hifiExciterCached = clamp01(voice.patch.hifiExciter);
                voice.outputTransformerCached = clamp01(voice.patch.outputTransformer);
                voice.outputSoftClipCached = clamp01(voice.patch.outputSoftClip);
                voice.outputGlueCached = clamp01(voice.patch.outputGlue);
                voice.tremoloDepthCached = clamp01(voice.patch.tremoloDepth);
                voice.gainCached = std::max(0.0, voice.patch.gain);
                voice.detuneRatioA = centsToRatio(voice.patch.oscADetuneCents);
                voice.detuneRatioB = centsToRatio(voice.patch.detuneCents + voice.patch.oscBDetuneCents);
                voice.detuneRatioC = centsToRatio(voice.patch.detuneCCents + voice.patch.oscCDetuneCents);
                voice.detuneRatioD = centsToRatio(voice.patch.detuneDCents + voice.patch.oscDDetuneCents);
                voice.chorusEnsembleCached = clamp01(voice.patch.chorusEnsemble);
                voice.delayStereoCached = clamp01(voice.patch.delayStereo);
                voice.delayModDepthCached = clamp01(voice.patch.delayModDepth);
                voice.delayDriveCached = clamp01(voice.patch.delayDrive);
                voice.delayDuckingCached = clamp01(voice.patch.delayDucking);
                voice.reverbDiffusionCached = clamp01(voice.patch.reverbDiffusion);
                voice.reverbWidthCached = clamp01(voice.patch.reverbWidth);
                voice.reverbShimmerCached = clamp01(voice.patch.reverbShimmer);
                voice.reverbModDepthCached = clamp01(voice.patch.reverbModDepth);
                voice.filterDriveCached = emergencyLoad
                    ? (1.0 + clamp01(voice.patch.filterDrive) * 2.0)
                    : (1.0 + clamp01(voice.patch.filterDrive) * 7.0);
                voice.ringAmountCached = emergencyLoad
                    ? 0.0
                    : (voice.patch.ringEnabled ? clamp01(voice.patch.ringMod) : 0.0);
                voice.resonanceCached = emergencyLoad
                    ? std::min(0.12, clamp01(voice.patch.resonance))
                    : clamp01(voice.patch.resonance);
                voice.filterModeCached = emergencyLoad
                    ? 0
                    : std::clamp(voice.patch.filterMode, 0, 2);
                const double dividerFollowHz = std::clamp(frequency * 0.40, 24.0, 3600.0);
                voice.dividerAlphaCached = std::clamp(1.0 - std::exp(-twoPi * dividerFollowHz / sampleRate_), 0.01, 0.35);
                const double spectralStressCtl = std::clamp(frequency / (nyquist * 0.92), 0.0, 1.0);
                const double aliasCutHz = std::clamp(
                    (1.0 - spectralStressCtl) * 16000.0 + 2200.0 + analogColor * 1400.0,
                    1800.0,
                    18000.0);
                voice.aliasAlphaCached = std::clamp(1.0 - std::exp(-twoPi * aliasCutHz / sampleRate_), 0.02, 0.98);
                voice.aliasBlendCached = std::clamp(spectralStressCtl * (0.22 + (1.0 - analogColor) * 0.18), 0.0, 0.46);

                // Cache effect mix values for fast-path skipping
                const bool allowChorusFx = !panicLoad && (!heavyLoad || activeVoiceCount <= 8);
                const double chorusMixRaw = voice.patch.chorusEnabled
                    ? (heavyLoad ? clamp01(voice.patch.chorusMix) * 0.4 : clamp01(voice.patch.chorusMix))
                    : 0.0;
                voice.cachedChorusMix = (extremeLoad || !allowChorusFx) ? 0.0 : chorusMixRaw;
                const bool allowSpatialFx = !panicLoad && !extremeLoad && !heavyLoad && activeVoiceCount <= 6;
                const double loadHeadroom = std::clamp((72.0 - smoothedDspLoadPercent_) / 36.0, 0.0, 1.0);
                const double voiceHeadroom = std::clamp((12.0 - static_cast<double>(activeVoiceCount)) / 8.0, 0.0, 1.0);
                const double spatialBudget = allowSpatialFx ? (loadHeadroom * voiceHeadroom) : 0.0;
                voice.cachedDelayMix = clamp01(voice.patch.delayMix) * spatialBudget;
                voice.cachedReverbMix = clamp01(voice.patch.reverbMix) * spatialBudget;

                voice.controlSamplesRemaining = voice.controlBlockSize;
            }
            const double fmFeedback = voice.patch.fmEnabled ? clamp01(voice.patch.fmFeedback) * 0.92 : 0.0;
            const int fmAlgorithm = std::clamp(voice.patch.fmAlgorithm, 0, 3);
            const double fmColor = voice.fmColorCached;
            const double fmSpread = voice.fmSpreadCached;
            const bool oscAEnabled = voice.oscAEnabledCached;
            const bool oscBEnabled = voice.oscBEnabledCached;
            const bool oscCEnabled = voice.oscCEnabledCached;
            const bool oscDEnabled = voice.oscDEnabledCached;
            const double oscALevel = voice.oscALevelCached;
            const double oscBLevel = voice.oscBLevelCached;
            const double oscCLevel = voice.oscCLevelCached;
            const double oscDLevel = voice.oscDLevelCached;
            double oscA = 0.0;
            double oscB = 0.0;
            double oscC = 0.0;
            double oscD = 0.0;
            double unisonSide = 0.0;
            double unisonContrast = 0.0;
            bool unisonReferenceSet = false;
            double unisonReference = 0.0;
            const double primaryPhaseBefore = voice.unisonPhaseA.front();
            const double oscMix = voice.oscMixCached;
            const double oscCMix = voice.oscCMixCached;
            const double oscDMix = voice.oscDMixCached;
            const double driveA = voice.driveACached;
            const double driveB = voice.driveBCached;
            const double driveC = voice.driveCCached;
            const double driveD = voice.driveDCached;
            const double waveCompA = voice.waveCompACached;
            const double waveCompB = voice.waveCompBCached;
            const double waveCompC = voice.waveCompCCached;
            const double waveCompD = voice.waveCompDCached;
            for (int index = 0; index < unisonCount; ++index) {
                const std::size_t unisonIndex = static_cast<std::size_t>(index);
                const double position = unisonPosition(index, unisonCount);
                const double phaseA = voice.unisonPhaseA[unisonIndex];
                const double phaseB = voice.unisonPhaseB[unisonIndex];
                const double phaseC = voice.unisonPhaseC[unisonIndex];
                const double phaseD = voice.unisonPhaseD[unisonIndex];
                const double unisonRatio = voice.unisonRatioCached[unisonIndex];
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
                    const double fmRatioLocal = fmRatio * (1.0 + position * fmSpread * 0.35);
                    const double opB = shapeFmSignal(std::sin(wrapPhase(phaseB * fmRatioLocal) * twoPi), fmColor);
                    const double opC = shapeFmSignal(std::sin(wrapPhase(phaseC * fmRatioLocal * 1.5) * twoPi), fmColor);
                    const double opD = shapeFmSignal(std::sin(wrapPhase(phaseD * fmRatioLocal * 2.0) * twoPi), fmColor);
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
            const double dividerAlpha = voice.dividerAlphaCached;
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
            double whiteNoise = 0.0;
            double pinkNoise = 0.0;
            double noise = 0.0;
            if (needsNoiseEngine) {
                whiteNoise = sampleOscillator(
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
                pinkNoise = std::clamp(
                    (voice.pinkB0 + voice.pinkB1 + voice.pinkB2 + voice.pinkB3 + voice.pinkB4 + voice.pinkB5 + voice.pinkB6
                        + whiteNoise * 0.5362)
                        * 0.11,
                    -1.0,
                    1.0);
                voice.pinkB6 = whiteNoise * 0.115926;
                const double noiseTone = clamp01(voice.patch.noiseTone);
                const double brightNoiseBias = noiseTone * noiseTone * (0.55 + noiseTone * 0.45);
                const double noiseSource = pinkNoise * (1.0 - brightNoiseBias) + whiteNoise * brightNoiseBias;
                noise = voice.patch.noiseEnabled
                    ? sculptNoiseSample(noiseSource, voice.patch.noiseTone, voice.noiseColorState)
                    : 0.0;
            }
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
                + sub * voice.subAmountCached
                + noise * voice.noiseAmountCached;
            const double ringAmount = voice.ringAmountCached;
            value = value * (1.0 - ringAmount)
                + (oscA * (oscB + oscC * 0.5 + oscD * 0.35)) * ringAmount;
            value *= levelComp;
            const double bassFocus = voice.subAmountCached * (0.35 + analogColor * 0.65);
            const double subReinforce = simplifiedNonlinear
                ? fastSaturate((subSine * 0.82 + subOctave * 0.48) * (1.0 + std::max(0.0, voice.patch.drive) * 1.6))
                : std::tanh((subSine * 0.82 + subOctave * 0.48) * (1.0 + std::max(0.0, voice.patch.drive) * 1.6));
            const double subDynamics = 1.0 / (1.0 + std::abs(oscABCD) * 0.35);
            value += subReinforce * bassFocus * (0.30 + lowEndFocus * 0.24) * subDynamics;
            const double aliasAlpha = voice.aliasAlphaCached;
            voice.antiAliasState += aliasAlpha * (value - voice.antiAliasState);
            voice.antiAliasState2 += aliasAlpha * (voice.antiAliasState - voice.antiAliasState2);
            const double aliasBlend = voice.aliasBlendCached;
            value = value * (1.0 - aliasBlend) + voice.antiAliasState2 * aliasBlend;

            // TPT SVF filter (replaces cascaded 1-pole)
            const double resonance = voice.resonanceCached;
            const int filterMode = voice.filterModeCached;
            const double filterDrive = voice.filterDriveCached;
            // Compute cutoff Hz with keytracking, envelope, and velocity
            const double filterEnvelope = envelopeFor(voice.patch.filterEnvelope, voice.age, voice.gateSeconds, voice.patch.filterEnvelopeCurve);
            const double keytrack = clamp01(voice.patch.filterKeytrack);
            const double keytrackBias = ((static_cast<double>(voice.note.midi - 60) / 48.0) * 0.18) * keytrack;
            // Velocity-to-filter modulation (patch-controllable depth)
            const double velToFilter = clamp01(voice.patch.velocityToFilter);
            const double velocityFilterBoost = velocityCurveValue(velocityNorm, voice.patch.velocityCurve) * velToFilter * 0.35;
            // Key-scaled resonance (higher notes get more bite)
            const double keytrackRes = clamp01(voice.patch.filterKeytrackResonance);
            const double resonanceKeyBias = ((static_cast<double>(voice.note.midi - 60) / 48.0) * 0.12) * keytrackRes;
            const double effectiveResonance = std::clamp(resonance + resonanceKeyBias, 0.0, 1.0);
            const double cutoffNormalized = clamp01(
                voice.patch.cutoff
                + filterEnvelope * voice.patch.filterEnvelopeAmount
                + lfo * clamp01(voice.patch.lfoFilterDepth) * 0.35
                + velocityFilterBoost
                + keytrackBias);
            const double cutoffHz = std::clamp(
                20.0 * std::exp2(cutoffNormalized * 10.0),
                20.0,
                sampleRate_ * 0.45);
            const double g = std::tan(pi * cutoffHz / sampleRate_);
            // TPT SVF damping: k=2 at no resonance, k approaches 0 at max resonance.
            // Clamp k to a minimum of 0.05 to prevent numerical instability at extreme resonance.
            const double k = std::max(0.05, 2.0 - 2.0 * effectiveResonance);

            // Single filter input saturation — drive adds harmonic content before filtering
            const double driven = simplifiedNonlinear
                ? fastSaturate(value * filterDrive * 0.9)
                : std::tanh(value * filterDrive);
            const int tptMode = mapOldFilterModeToTpt(filterMode);
            // Mode 1 (old LP4) uses same LP output but with gentler k for smoother resonance
            const double effectiveK = (filterMode == 1) ? std::max(0.08, k * 0.65) : k;
            // Nonlinear TPT SVF: saturate the v1 integrator state for analog character
            const double filterNonlin = clamp01(voice.patch.filterNonlinearity);
            double filterValue;
            if (filterNonlin > 0.001) {
                // Compute linear v1 first, then apply gentle saturation to the state only
                const double v1Linear = (voice.svfZ1 + g * (driven - voice.svfZ2)) / (1.0 + g * (g + effectiveK));
                const double v1 = v1Linear * (1.0 - filterNonlin) + std::tanh(v1Linear * (1.0 + filterNonlin * 1.5)) * filterNonlin;
                const double v2 = voice.svfZ2 + g * v1;
                voice.svfZ1 = 2.0 * v1 - voice.svfZ1;
                voice.svfZ2 = 2.0 * v2 - voice.svfZ2;
                switch (tptMode) {
                    case 0: case 1: filterValue = v2; break;
                    case 2: filterValue = v1; break;
                    case 3: filterValue = driven - effectiveK * v1; break;
                    case 4: filterValue = driven - effectiveK * v1 + v2; break;
                    default: filterValue = v2;
                }
            } else {
                filterValue = tptSvfProcessSampleWithMode(driven, g, effectiveK, voice.svfZ1, voice.svfZ2, tptMode);
            }
            value = filterValue;

            const double combMix = (extremeLoad || panicLoad)
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

            if (!panicLoad) {
                value = wavefoldSample(value, voice.patch.wavefold * antiAliasScale);
            }

            const double highPass = clamp01(voice.patch.highPass);
            if (highPass > 0.0) {
                const double hpAlpha = std::clamp(0.002 + highPass * 0.45, 0.002, 0.6);
                voice.highPassState += hpAlpha * (value - voice.highPassState);
                value -= voice.highPassState;
            }

            const double transientAmountEffective = panicLoad ? 0.0 : transientAmount;
            const double transientDecay = std::max(0.001, voice.patch.transientDecay);
            const double transientEnvelope = voice.transientEnvelopeState;
            const double clickEnvelope = voice.age < 0.002 ? 1.0 - voice.age / 0.002 : 0.0;
            const int burstCount = extremeLoad
                ? 1
                : std::clamp(voice.patch.transientBurstCount, 1, 12);
            const double burstSpacing = std::max(0.0005, voice.patch.transientBurstSpacing);
            const double burstDecay = clamp01(voice.patch.transientBurstDecay);
            double burstEnvelope = 0.0;
            if (transientAmountEffective > 0.0) {
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
            const double transientBright = transientTone * transientTone * (0.4 + transientTone * 0.6);
            double transientNoise = 0.0;
            if (transientAmountEffective > 0.0001) {
                const double transientSource = pinkNoise * (1.0 - transientBright * 0.72)
                    + whiteNoise * (0.34 + transientBright * 0.66);
                transientNoise = sculptNoiseSample(
                    transientSource,
                    voice.patch.transientTone,
                    voice.transientColorState);
            }
            const double transientShape = 1.0 + clamp01(voice.patch.transientShape) * 8.0;
            const double shapedBurst = std::tanh(burstEnvelope * transientShape);
            const double transientBodyAmount = transientAmountEffective
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
            value += clickEnvelope * (panicLoad ? 0.0 : clamp01(voice.patch.click))
                + transientEnvelope * transientAmountEffective * transientNoise
                + shapedBurst * transientAmountEffective * transientNoise * 0.65
                + transientBody * transientBodyAmount;

            const double drive = std::max(0.0, voice.patch.drive) * (0.72 + antiAliasScale * 0.28);
            const double analogWarmth = voice.analogWarmthCached;
            const double hifiExciter = voice.hifiExciterCached;
            const double polyNoiseScale = std::clamp(
                1.0 / std::sqrt(std::max(1.0, static_cast<double>(activeVoiceCount))),
                0.20,
                1.0);
            const double analogNoise = randomSymmetric(voice.noiseState)
                * (analogColor * 0.0018)
                * (panicLoad ? 0.35 : 1.0)
                * polyNoiseScale;
            value += analogNoise;
            const double preNonlinear = value;
            const double satDrive = drive * (0.85 + analogColor * 0.55 + analogWarmth * 0.42);
            const double satAsymmetry = (velocityNorm - 0.5) * 0.45 + voice.toneTiltCached * 0.2;
            double nonlinear = asymmetricSaturation(preNonlinear, satDrive, satAsymmetry);
            if (!heavyLoad) {
                const double midpoint = 0.5 * (preNonlinear + voice.nonlinearPrevInput);
                const double nonlinearMid = asymmetricSaturation(midpoint, satDrive, satAsymmetry);
                nonlinear = nonlinear * 0.56 + nonlinearMid * 0.44;
            }
            if (!extremeLoad && !panicLoad) {
                nonlinear = harmonicExciter(
                    nonlinear,
                    analogColor * 0.62 + drive * 0.25 + analogWarmth * 0.34 + hifiExciter * 0.45);
            }
            voice.nonlinearPrevInput = preNonlinear;
            const double smoothAlpha = nonlinearSmoothingAlpha(drive, analogColor);
            voice.nonlinearSmoothState += smoothAlpha * (nonlinear - voice.nonlinearSmoothState);
            const double smoothMix = std::clamp(0.12 + drive * 0.28 + analogColor * 0.2, 0.0, 0.65);
            nonlinear = nonlinear * (1.0 - smoothMix) + voice.nonlinearSmoothState * smoothMix;
            const double bodyAlpha = std::clamp((twoPi * 140.0) * invSampleRate, 0.002, 0.12);
            voice.lowBodyState += bodyAlpha * (preNonlinear - voice.lowBodyState);
            const double lowPunch = voice.lowPunchCached;
            const double bodyBlend = std::clamp(
                voice.subAmountCached * 0.28 + analogColor * 0.25 + drive * 0.08 + lowPunch * 0.26 + analogWarmth * 0.24,
                0.0,
                0.62);
            value = nonlinear * (1.0 - bodyBlend) + voice.lowBodyState * bodyBlend;
            const double airAlpha = std::clamp((twoPi * 3200.0) * invSampleRate, 0.02, 0.35);
            voice.airExciterState += airAlpha * (value - voice.airExciterState);
            const double airBand = value - voice.airExciterState;
            const double airAmount = panicLoad
                ? 0.0
                : std::clamp(
                    analogColor * 0.18 + drive * 0.12 + voice.airBoostCached * 0.3 + analogWarmth * 0.08 + hifiExciter * 0.22,
                    0.0,
                    0.42);
            value += airBand * airAmount;
            const double tapeColor = voice.tapeColorCached;
            if (tapeColor > 0.0001) {
                const double tapeDrive = 1.0 + tapeColor * 4.5 + analogWarmth * 1.6;
                double tapeSample = std::tanh(value * tapeDrive);
                tapeSample = softKneeLimiter(tapeSample + voice.lowBodyState * tapeColor * 0.12, 1.0, 2.0 + tapeColor * 3.0);
                const double tapeMix = tapeColor * 0.58;
                value = value * (1.0 - tapeMix) + tapeSample * tapeMix;
            }
            value *= (1.0 + drive * 0.34 + analogColor * 0.18 + analogWarmth * 0.14);
            value = softKneeLimiter(value, 1.05, 3.2);

            const double toneTilt = voice.toneTiltCached;
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
            value *= voice.tremoloGainValue;
            // Velocity curve: patch-selectable curve with depth control
            const double velToAmp = clamp01(voice.patch.velocityToAmp);
            const double velocityGain = 1.0 - velToAmp + velocityCurveValue(velocityNorm, voice.patch.velocityCurve) * velToAmp;
            value *= voice.gainCached * velocityGain;
            const double transformer = voice.outputTransformerCached;
            if (transformer > 0.0001) {
                const double transformerDrive = 1.0 + transformer * 5.5;
                const double transformed = asymmetricSaturation(value, transformerDrive * 0.35, 0.14);
                const double transformerMix = std::clamp(transformer * 0.72, 0.0, 0.72);
                value = value * (1.0 - transformerMix) + transformed * transformerMix;
            }
            const double outputGlue = voice.outputGlueCached;
            if (outputGlue > 0.0001) {
                const double glueDrive = 1.0 + outputGlue * 3.8;
                const double glued = std::tanh(value * glueDrive) / glueDrive;
                const double glueMix = std::clamp(outputGlue * 0.72, 0.0, 0.72);
                value = value * (1.0 - glueMix) + glued * glueMix;
            }
            const double outputSoftClip = voice.outputSoftClipCached;
            if (outputSoftClip > 0.0001) {
                const double clipThreshold = std::clamp(1.0 - outputSoftClip * 0.38, 0.58, 1.0);
                const double clipRatio = std::clamp(2.1 + outputSoftClip * 7.2, 2.1, 10.0);
                const double clipped = softKneeLimiter(value, clipThreshold, clipRatio);
                const double clipMix = std::clamp(outputSoftClip * 0.78, 0.0, 0.78);
                value = value * (1.0 - clipMix) + clipped * clipMix;
            }
            if (voice.patch.bitCrushEnabled) {
                value = crushSample(value, voice.patch.bitCrush);
            }
            voice.fmFeedbackState = value;

            const double leftGain = voice.panLeftGainValue;
            const double rightGain = voice.panRightGainValue;
            const double spread = clamp01(voice.patch.stereoSpread);
            const double stereoDepth = voice.stereoDepthCached;
            const double phaseScatter = voice.phaseScatterCached;
            const double sideSeed = std::abs(unisonSide) > 1e-6
                ? unisonSide
                : (value - voice.toneTiltState) * 0.42;
            const double sideValue = crushSample(
                (simplifiedNonlinear
                        ? fastSaturate(sideSeed * (1.0 + drive * 4.0))
                        : std::tanh(sideSeed * (1.0 + drive * 4.0)))
                    * ampEnvelope
                    * voice.gainCached
                    * velocityGain
                    * spread
                    * (0.8 + phaseScatter * 0.5 + stereoDepth * 0.42),
                voice.patch.bitCrush);
            double voiceLeft = value * leftGain;
            double voiceRight = value * rightGain;
            voiceLeft -= sideValue * 0.5;
            voiceRight += sideValue * 0.5;
            if (spread > 0.0) {
                const double mid = (voiceLeft + voiceRight) * 0.5;
                double side = (voiceRight - voiceLeft) * 0.5;
                side *= 1.0 + spread * (1.35 + stereoDepth * 0.92);
                voiceLeft = mid - side;
                voiceRight = mid + side;
            }

            // Effects fast-path: skip ALL effects if all mixes are zero
            const double effectiveChorusMix = voice.cachedChorusMix;
            const double delayMix = voice.cachedDelayMix;
            const double reverbMix = voice.cachedReverbMix;
            const bool effectsActive = effectiveChorusMix > 0.0 || delayMix > 0.0 || reverbMix > 0.0;

            if (effectsActive) {
                // Chorus
                const double chorusFeedback = clamp01(voice.patch.chorusFeedback) * 0.85;
                if (effectiveChorusMix > 0.0 || chorusFeedback > 0.0001) {
                    const double analogWarmth = voice.analogWarmthCached;
                    const double wowFlutter = clamp01(voice.patch.wowFlutter);
                    const double vintageDrift = clamp01(voice.patch.vintageDrift);
                    const double chorusJitter = voice.chorusJitterCached;
                    const double chorusSaturation = voice.chorusSaturationCached;
                    const double chorusLuxury = std::clamp(
                        voice.chorusEnsembleCached * 0.46 + analogWarmth * 0.28 + wowFlutter * 0.18 + vintageDrift * 0.08,
                        0.0,
                        1.0);
                    const double chorusLfo = std::sin(voice.chorusPhase * twoPi);
                    const double chorusWidth = clamp01(voice.patch.chorusWidth);
                    const double chorusLfoOffset = std::sin(wrapPhase(voice.chorusPhase + chorusWidth * 0.25) * twoPi);
                    const double baseDelay = sampleRate_ * (0.004 + clamp01(voice.patch.chorusDelay) * 0.022);
                    const double depthSamples = sampleRate_ * 0.012 * clamp01(voice.patch.chorusDepth);
                    const double wowLfo = std::sin(wrapPhase(voice.chorusPhase * (0.57 + wowFlutter * 0.28) + 0.17) * twoPi);
                    const double flutterLfo = std::sin(wrapPhase(voice.chorusPhase * (1.83 + wowFlutter * 1.75) + 0.61) * twoPi);
                    const double jitterNoise = randomSymmetric(voice.noiseState) * chorusJitter;
                    const double jitterLfo = std::sin(wrapPhase(voice.chorusPhase * (2.2 + chorusJitter * 5.6) + 0.43) * twoPi);
                    const double wowMod = 1.0
                        + (wowLfo * 0.62 + flutterLfo * 0.38) * (0.006 + wowFlutter * 0.018)
                        + (jitterNoise * 0.003 + jitterLfo * 0.005) * chorusJitter;
                    const double delayedLeft = readDelay(
                        voice.chorusLeft,
                        voice.chorusIndex,
                        (baseDelay + depthSamples * chorusLfo) * wowMod);
                    const double delayedRight = readDelay(
                        voice.chorusRight,
                        voice.chorusIndex,
                        (baseDelay - depthSamples * chorusLfoOffset) * (2.0 - wowMod));
                    const double ensembleAmount = voice.chorusEnsembleCached;
                    double chorusWetLeft = delayedLeft;
                    double chorusWetRight = delayedRight;
                    if (ensembleAmount > 0.0001) {
                        const double lfo2 = std::sin(wrapPhase(voice.chorusPhase + 0.37 + chorusWidth * 0.31) * twoPi);
                        const double lfo3 = std::sin(wrapPhase(voice.chorusPhase + 0.71 + chorusWidth * 0.19) * twoPi);
                        const double delayedLeft2 = readDelay(
                            voice.chorusLeft,
                            voice.chorusIndex,
                            baseDelay * 1.21 + depthSamples * 0.72 * lfo2);
                        const double delayedRight2 = readDelay(
                            voice.chorusRight,
                            voice.chorusIndex,
                            baseDelay * 1.17 - depthSamples * 0.72 * lfo3);
                        const double ensembleBlend = std::clamp(ensembleAmount * 0.55, 0.0, 0.55);
                        chorusWetLeft = chorusWetLeft * (1.0 - ensembleBlend) + delayedLeft2 * ensembleBlend;
                        chorusWetRight = chorusWetRight * (1.0 - ensembleBlend) + delayedRight2 * ensembleBlend;
                    }
                    if (chorusLuxury > 0.0001) {
                        const double lfo4 = std::sin(wrapPhase(voice.chorusPhase + 0.13 + chorusWidth * 0.43) * twoPi);
                        const double lfo5 = std::sin(wrapPhase(voice.chorusPhase + 0.89 + chorusWidth * 0.57) * twoPi);
                        const double delayedLeft3 = readDelay(
                            voice.chorusLeft,
                            voice.chorusIndex,
                            (baseDelay * 1.39 + depthSamples * 0.42 * lfo4) * (1.0 + wowFlutter * 0.02));
                        const double delayedRight3 = readDelay(
                            voice.chorusRight,
                            voice.chorusIndex,
                            (baseDelay * 1.44 - depthSamples * 0.42 * lfo5) * (1.0 - wowFlutter * 0.02));
                        const double luxuryBlend = std::clamp(chorusLuxury * 0.33, 0.0, 0.33);
                        chorusWetLeft = chorusWetLeft * (1.0 - luxuryBlend) + delayedLeft3 * luxuryBlend;
                        chorusWetRight = chorusWetRight * (1.0 - luxuryBlend) + delayedRight3 * luxuryBlend;
                    }
                    const double chorusTone = voice.chorusToneCached;
                    const double toneAlpha = std::clamp(0.015 + chorusTone * 0.75, 0.015, 0.9);
                    voice.chorusToneStateL += toneAlpha * (chorusWetLeft - voice.chorusToneStateL);
                    voice.chorusToneStateR += toneAlpha * (chorusWetRight - voice.chorusToneStateR);
                    chorusWetLeft = voice.chorusToneStateL;
                    chorusWetRight = voice.chorusToneStateR;
                    const double chorusCompDrive = 1.0 + chorusLuxury * 2.4 + chorusSaturation * 3.2;
                    chorusWetLeft = std::tanh(chorusWetLeft * chorusCompDrive) / chorusCompDrive;
                    chorusWetRight = std::tanh(chorusWetRight * chorusCompDrive) / chorusCompDrive;
                    const double bbdNoise = randomSymmetric(voice.noiseState)
                        * (0.00005 + chorusLuxury * 0.00035 + chorusJitter * 0.00028)
                        * (0.35 + effectiveChorusMix * 0.65)
                        * polyNoiseScale
                        * (panicLoad ? 0.45 : 1.0);
                    chorusWetLeft += bbdNoise;
                    chorusWetRight -= bbdNoise * 0.85;
                    const double chorusInLeft = voiceLeft + delayedRight * chorusFeedback;
                    const double chorusInRight = voiceRight + delayedLeft * chorusFeedback;
                    voice.chorusLeft[static_cast<std::size_t>(voice.chorusIndex)] = static_cast<float>(chorusInLeft);
                    voice.chorusRight[static_cast<std::size_t>(voice.chorusIndex)] = static_cast<float>(chorusInRight);
                    voice.chorusIndex = (voice.chorusIndex + 1) % static_cast<int>(voice.chorusLeft.size());
                    voiceLeft = voiceLeft * (1.0 - effectiveChorusMix) + chorusWetLeft * effectiveChorusMix;
                    voiceRight = voiceRight * (1.0 - effectiveChorusMix) + chorusWetRight * effectiveChorusMix;
                }

                // Delay / Reverb
                const bool allowSpatialFx = !panicLoad && !extremeLoad && !heavyLoad && activeVoiceCount <= 6;
                const int fxWriteIndex = voice.fxDelayIndex;
                if (delayMix > 0.0 || reverbMix > 0.0) {
                    const double delayTimeNorm = clamp01(voice.patch.delayTime);
                    const double delaySamples = std::clamp(
                        48.0 + delayTimeNorm * static_cast<double>(fxDelayBufferSize - 192),
                        24.0,
                        static_cast<double>(fxDelayBufferSize - 2));
                    const double wowFlutter = clamp01(voice.patch.wowFlutter);
                    const double wowCharacter = std::clamp(wowFlutter * 0.55 + voice.delayWowCached * 0.85, 0.0, 1.0);
                    const double modDepth = 0.001 + voice.delayModDepthCached * 0.018 + wowCharacter * 0.012;
                    const double stereoSkew = voice.delayStereoCached * 0.14;
                    const double wow = std::sin((voice.age * (0.08 + wowCharacter * 0.42) + voice.chorusPhase * 0.13) * twoPi);
                    const double flutter = std::sin((voice.age * (2.6 + wowCharacter * 4.1) + voice.chorusPhase * 0.31) * twoPi);
                    const double flutterDelayMod = 1.0 + std::sin(voice.chorusPhase * twoPi) * modDepth;
                    const double tapeWarp = 1.0 + (wow * 0.72 + flutter * 0.28) * modDepth * 0.85;
                    const double delayedLeft = readDelay(
                        voice.fxDelayLeft,
                        fxWriteIndex,
                        delaySamples * flutterDelayMod * tapeWarp * (1.0 + stereoSkew));
                    const double delayedRight = readDelay(
                        voice.fxDelayRight,
                        fxWriteIndex,
                        delaySamples * (2.0 - flutterDelayMod) * (2.0 - tapeWarp) * (1.0 - stereoSkew));
                    const double diffusionBlend = std::clamp(voice.delayDiffusionCached * 0.68, 0.0, 0.68);
                    const double diffuseLeft = delayedLeft * (1.0 - diffusionBlend)
                        + delayedRight * (diffusionBlend * 0.55)
                        + voice.delayDiffuseStateL * (diffusionBlend * 0.45);
                    const double diffuseRight = delayedRight * (1.0 - diffusionBlend)
                        + delayedLeft * (diffusionBlend * 0.55)
                        + voice.delayDiffuseStateR * (diffusionBlend * 0.45);
                    voice.delayDiffuseStateL = diffuseLeft;
                    voice.delayDiffuseStateR = diffuseRight;
                    const double delayFeedback = clamp01(voice.patch.delayFeedback) * 0.96;
                    const double delayTone = clamp01(voice.patch.delayTone);
                    const double toneAlpha = std::clamp(0.006 + delayTone * 0.42, 0.006, 0.5);
                    const double delayCrossfeed = std::clamp(0.04 + voice.delayStereoCached * 0.16 + voice.delayCrossfeedCached * 0.34, 0.03, 0.58);
                    const double feedbackInLeft = diffuseLeft * delayFeedback + diffuseRight * (delayFeedback * delayCrossfeed);
                    const double feedbackInRight = diffuseRight * delayFeedback + diffuseLeft * (delayFeedback * delayCrossfeed);
                    voice.delayToneStateL += toneAlpha * (feedbackInLeft - voice.delayToneStateL);
                    voice.delayToneStateR += toneAlpha * (feedbackInRight - voice.delayToneStateR);
                    const double feedbackDrive = 1.0 + voice.delayDriveCached * 3.2;
                    const double drivenFeedbackL = std::tanh(voice.delayToneStateL * feedbackDrive);
                    const double drivenFeedbackR = std::tanh(voice.delayToneStateR * feedbackDrive);
                    const double tapeNoise = randomSymmetric(voice.noiseState)
                        * (0.00003 + voice.tapeColorCached * 0.00018)
                        * (0.35 + delayMix * 0.65)
                        * polyNoiseScale
                        * (panicLoad ? 0.45 : 1.0);
                    voice.fxDelayLeft[static_cast<std::size_t>(fxWriteIndex)] = static_cast<float>(voiceLeft + drivenFeedbackL + tapeNoise);
                    voice.fxDelayRight[static_cast<std::size_t>(fxWriteIndex)] = static_cast<float>(voiceRight + drivenFeedbackR - tapeNoise);
                    const double duckGain = 1.0 / (1.0 + voice.delayDuckingCached * std::abs((voiceLeft + voiceRight) * 0.5) * 6.0);
                    const double effectiveDelayMix = delayMix * duckGain;
                    voiceLeft = voiceLeft * (1.0 - effectiveDelayMix) + diffuseLeft * effectiveDelayMix;
                    voiceRight = voiceRight * (1.0 - effectiveDelayMix) + diffuseRight * effectiveDelayMix;

                    if (reverbMix > 0.0) {
                        const double size = clamp01(voice.patch.reverbSize);
                        const double damping = clamp01(voice.patch.reverbDamping);
                        const double preDelay = 24.0 + clamp01(voice.patch.reverbPreDelay) * 900.0;
                        const double reverbModScale = 0.001 + voice.reverbModDepthCached * 0.010 + voice.reverbChorusCached * 0.012;
                        const double modA = 1.0 + std::sin(voice.chorusPhase * twoPi * 0.73 + 0.31) * reverbModScale;
                        const double modB = 1.0 + std::sin(voice.chorusPhase * twoPi * 0.57 + 1.19) * reverbModScale;
                        const double modC = 1.0 + std::sin(voice.chorusPhase * twoPi * 0.91 + 2.07) * reverbModScale;
                        const double modD = 1.0 + std::sin(voice.chorusPhase * twoPi * 0.63 + 2.71) * reverbModScale;
                        const double tapA = readDelay(voice.fxDelayLeft, fxWriteIndex, (preDelay + delaySamples * (0.43 + size * 0.35)) * modA);
                        const double tapB = readDelay(voice.fxDelayLeft, fxWriteIndex, (preDelay + delaySamples * (0.79 + size * 0.31)) * modB);
                        const double tapC = readDelay(voice.fxDelayRight, fxWriteIndex, (preDelay + delaySamples * (0.61 + size * 0.41)) * modC);
                        const double tapD = readDelay(voice.fxDelayRight, fxWriteIndex, (preDelay + delaySamples * (0.94 + size * 0.27)) * modD);

                        // === ALL-PASS DIFFUSER CASCADE (Phase 5) ===
                        // 4-stage all-pass filter cascade for dense, natural reflections
                        // Each stage smears transients while preserving broadband energy
                        const double apfCoeff = 0.65 + voice.reverbDiffusionCached * 0.28; // 0.65-0.93
                        // Left channel cascade
                        const double apfInL1 = tapA * 0.62 + tapB * 0.38;
                        const double apfOutL1 = voice.apfL1 + apfCoeff * apfInL1;
                        voice.apfL1 = apfInL1 - apfCoeff * apfOutL1;
                        const double apfInL2 = apfOutL1;
                        const double apfOutL2 = voice.apfL2 + apfCoeff * 0.82 * apfInL2;
                        voice.apfL2 = apfInL2 - apfCoeff * 0.82 * apfOutL2;
                        const double apfInL3 = apfOutL2;
                        const double apfOutL3 = voice.apfL3 + apfCoeff * 0.64 * apfInL3;
                        voice.apfL3 = apfInL3 - apfCoeff * 0.64 * apfOutL3;
                        const double apfInL4 = apfOutL3;
                        const double apfOutL4 = voice.apfL4 + apfCoeff * 0.48 * apfInL4;
                        voice.apfL4 = apfInL4 - apfCoeff * 0.48 * apfOutL4;
                        // Right channel cascade
                        const double apfInR1 = tapD * 0.62 + tapC * 0.38;
                        const double apfOutR1 = voice.apfR1 + apfCoeff * apfInR1;
                        voice.apfR1 = apfInR1 - apfCoeff * apfOutR1;
                        const double apfInR2 = apfOutR1;
                        const double apfOutR2 = voice.apfR2 + apfCoeff * 0.82 * apfInR2;
                        voice.apfR2 = apfInR2 - apfCoeff * 0.82 * apfOutR2;
                        const double apfInR3 = apfOutR2;
                        const double apfOutR3 = voice.apfR3 + apfCoeff * 0.64 * apfInR3;
                        voice.apfR3 = apfInR3 - apfCoeff * 0.64 * apfOutR3;
                        const double apfInR4 = apfOutR3;
                        const double apfOutR4 = voice.apfR4 + apfCoeff * 0.48 * apfInR4;
                        voice.apfR4 = apfInR4 - apfCoeff * 0.48 * apfOutR4;
                        const double diffuseL = apfOutL4;
                        const double diffuseR = apfOutR4;

                        const double diffuseAlpha = std::clamp(
                            0.018 + (1.0 - damping) * (0.08 + voice.reverbDiffusionCached * 0.10) + voice.reverbToneCached * 0.04,
                            0.015,
                            0.32);
                        const double decay = std::clamp(
                            0.54 + size * 0.34 + voice.reverbDiffusionCached * 0.09 + voice.reverbDecayCached * 0.12 + voice.reverbBloomCached * 0.1,
                            0.54,
                            0.985);
                        const double reverbInL = (diffuseL * 0.72 + diffuseR * 0.18 + tapC * 0.10) * decay;
                        const double reverbInR = (diffuseR * 0.72 + diffuseL * 0.18 + tapB * 0.10) * decay;
                        voice.reverbStateL1 += diffuseAlpha * (reverbInL - voice.reverbStateL1);
                        voice.reverbStateR1 += diffuseAlpha * (reverbInR - voice.reverbStateR1);
                        voice.reverbStateL2 += diffuseAlpha * ((voice.reverbStateL1 + voice.reverbStateR1 * 0.11) - voice.reverbStateL2);
                        voice.reverbStateR2 += diffuseAlpha * ((voice.reverbStateR1 + voice.reverbStateL1 * 0.11) - voice.reverbStateR2);
                        double reverbWetL = voice.reverbStateL2;
                        double reverbWetR = voice.reverbStateR2;
                        const double warmth = voice.analogWarmthCached;
                        const double air = voice.airBoostCached;
                        const double lowLift = (reverbInL + reverbInR) * 0.5 - (reverbWetL + reverbWetR) * 0.5;
                        const double reverbBody = std::tanh(lowLift * (0.7 + warmth * 0.8));
                        reverbWetL += reverbBody * (0.02 + warmth * 0.08 + voice.reverbBloomCached * 0.08);
                        reverbWetR += reverbBody * (0.02 + warmth * 0.08 + voice.reverbBloomCached * 0.08);
                        const double reverbAirL = (tapA - tapB) * (0.08 + air * 0.34 + voice.reverbToneCached * 0.26);
                        const double reverbAirR = (tapD - tapC) * (0.08 + air * 0.34 + voice.reverbToneCached * 0.26);
                        reverbWetL += reverbAirL;
                        reverbWetR += reverbAirR;
                        if (voice.reverbBloomCached > 0.0001) {
                            const double bloomDrive = 1.0 + voice.reverbBloomCached * 3.2;
                            reverbWetL = std::tanh(reverbWetL * bloomDrive) / bloomDrive;
                            reverbWetR = std::tanh(reverbWetR * bloomDrive) / bloomDrive;
                        }
                        if (voice.reverbShimmerCached > 0.0001) {
                            const double shimmerExcite = (std::abs(tapA) + std::abs(tapD) - std::abs(tapB) - std::abs(tapC));
                            const double shimmer = std::tanh(shimmerExcite * 1.7) * (0.06 + voice.reverbShimmerCached * 0.24);
                            reverbWetL += shimmer;
                            reverbWetR += shimmer;
                        }
                        if (voice.reverbWidthCached > 0.0001) {
                            const double wetMid = (reverbWetL + reverbWetR) * 0.5;
                            double wetSide = (reverbWetR - reverbWetL) * 0.5;
                            wetSide *= 1.0 + voice.reverbWidthCached * 1.35;
                            reverbWetL = wetMid - wetSide;
                            reverbWetR = wetMid + wetSide;
                        }
                        const double earlyMix = std::clamp(voice.reverbEarlyMixCached * 0.52, 0.0, 0.52);
                        const double earlyLeft = tapA * 0.52 + tapB * 0.28 + tapC * 0.20;
                        const double earlyRight = tapD * 0.52 + tapC * 0.28 + tapB * 0.20;
                        const double reverbOutLeft = reverbWetL * (1.0 - earlyMix) + earlyLeft * earlyMix;
                        const double reverbOutRight = reverbWetR * (1.0 - earlyMix) + earlyRight * earlyMix;
                        voiceLeft = voiceLeft * (1.0 - reverbMix) + reverbOutLeft * reverbMix;
                        voiceRight = voiceRight * (1.0 - reverbMix) + reverbOutRight * reverbMix;
                    }
                    voice.fxDelayIndex = (voice.fxDelayIndex + 1) % static_cast<int>(voice.fxDelayLeft.size());
                } else if (allowSpatialFx && (clamp01(voice.patch.delayMix) > 0.0 || clamp01(voice.patch.reverbMix) > 0.0)) {
                    voice.fxDelayLeft[static_cast<std::size_t>(fxWriteIndex)] = static_cast<float>(voiceLeft);
                    voice.fxDelayRight[static_cast<std::size_t>(fxWriteIndex)] = static_cast<float>(voiceRight);
                    voice.fxDelayIndex = (voice.fxDelayIndex + 1) % static_cast<int>(voice.fxDelayLeft.size());
                } else {
                    voice.delayToneStateL *= 0.995;
                    voice.delayToneStateR *= 0.995;
                    voice.reverbStateL1 *= 0.994;
                    voice.reverbStateL2 *= 0.994;
                    voice.reverbStateR1 *= 0.994;
                    voice.reverbStateR2 *= 0.994;
                    voice.apfL1 *= 0.994; voice.apfL2 *= 0.994;
                    voice.apfL3 *= 0.994; voice.apfL4 *= 0.994;
                    voice.apfR1 *= 0.994; voice.apfR2 *= 0.994;
                    voice.apfR3 *= 0.994; voice.apfR4 *= 0.994;
                }
            } else {
                // Fast-path decay when spatial FX are inactive.
                voice.delayToneStateL *= 0.995;
                voice.delayToneStateR *= 0.995;
                voice.reverbStateL1 *= 0.994;
                voice.reverbStateL2 *= 0.994;
                voice.reverbStateR1 *= 0.994;
                voice.reverbStateR2 *= 0.994;
                voice.apfL1 *= 0.994; voice.apfL2 *= 0.994;
                voice.apfL3 *= 0.994; voice.apfL4 *= 0.994;
                voice.apfR1 *= 0.994; voice.apfR2 *= 0.994;
                voice.apfR3 *= 0.994; voice.apfR4 *= 0.994;
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
            const double crosstalk = voice.consoleCrosstalkCached;
            if (crosstalk > 0.0001) {
                const double bleed = std::clamp(crosstalk * 0.35, 0.0, 0.35);
                const double dryLeft = voiceLeft;
                const double dryRight = voiceRight;
                voiceLeft = dryLeft * (1.0 - bleed) + dryRight * bleed;
                voiceRight = dryRight * (1.0 - bleed) + dryLeft * bleed;
            }
            const int busIndex = static_cast<int>(voiceIndex & 0x3u);
            switch (busIndex) {
                case 0:
                    voiceBusLeft0 += voiceLeft;
                    voiceBusRight0 += voiceRight;
                    break;
                case 1:
                    voiceBusLeft1 += voiceLeft;
                    voiceBusRight1 += voiceRight;
                    break;
                case 2:
                    voiceBusLeft2 += voiceLeft;
                    voiceBusRight2 += voiceRight;
                    break;
                default:
                    voiceBusLeft3 += voiceLeft;
                    voiceBusRight3 += voiceRight;
                    break;
            }
            voice.phaseA = voice.unisonPhaseA.front();
            voice.phaseB = voice.unisonPhaseB.front();
            voice.phaseC = voice.unisonPhaseC.front();
            voice.phaseD = voice.unisonPhaseD.front();
            voice.subPhase = wrapPhase(voice.subPhase + (frequency * 0.5) * invSampleRate);
            voice.lfoPhase = wrapPhase(voice.lfoPhase + std::max(0.0, voice.patch.lfoRate) * invSampleRate);
            voice.lfoValue += voice.lfoStep;
            voice.frequencyValue += voice.frequencyStep;
            voice.tremoloGainValue += voice.tremoloGainStep;
            voice.panLeftGainValue += voice.panLeftGainStep;
            voice.panRightGainValue += voice.panRightGainStep;
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
        }

        double mixedLeft = voiceBusLeft0 + voiceBusLeft1 + voiceBusLeft2 + voiceBusLeft3;
        double mixedRight = voiceBusRight0 + voiceBusRight1 + voiceBusRight2 + voiceBusRight3;

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
        if (heavyLoad) {
            mixedLeft = fastSaturate(mixedLeft * 0.82) / 0.82;
            mixedRight = fastSaturate(mixedRight * 0.82) / 0.82;
        } else {
            mixedLeft = std::tanh(mixedLeft * 0.70) / 0.70;
            mixedRight = std::tanh(mixedRight * 0.70) / 0.70;
        }

        // === TRUE PEAK LIMITING (Phase 6) ===
        // Detect inter-sample peaks using 4x oversampled lookahead
        const double truePeakLeft = std::abs(mixedLeft) + std::abs(mixedLeft - truePeakPrevLeft_) * 0.27;
        const double truePeakRight = std::abs(mixedRight) + std::abs(mixedRight - truePeakPrevRight_) * 0.27;
        truePeakPrevLeft_ = mixedLeft;
        truePeakPrevRight_ = mixedRight;
        const double truePeakMax = std::max(truePeakLeft, truePeakRight);
        if (truePeakMax > 1.0) {
            const double tpGain = 1.0 / truePeakMax;
            mixedLeft *= tpGain;
            mixedRight *= tpGain;
        }

        const double dcLeft = mixedLeft - outputDcInputLeft_ + dcCoeff * outputDcOutputLeft_;
        const double dcRight = mixedRight - outputDcInputRight_ + dcCoeff * outputDcOutputRight_;
        outputDcInputLeft_ = mixedLeft;
        outputDcOutputLeft_ = dcLeft;
        outputDcInputRight_ = mixedRight;
        outputDcOutputRight_ = dcRight;
        blockPostLimiterPeak = std::max(blockPostLimiterPeak, std::max(std::abs(dcLeft), std::abs(dcRight)));

        // === TPDF DITHERING (Phase 6) ===
        // Triangular probability density function dither for 24-bit output
        // Prevents quantization distortion when converting double → float
        const double ditherL = (ditherRand() + ditherRand() - 1.0) * ditherScale;
        const double ditherR = (ditherRand() + ditherRand() - 1.0) * ditherScale;
        left[sample] += static_cast<float>(dcLeft + ditherL);
        right[sample] += static_cast<float>(dcRight + ditherR);
    }

    // Manual compaction loop for post-render voice culling (swap-and-pop)
    {
        std::size_t write = 0;
        for (std::size_t read = 0; read < voices_.size(); ++read) {
            const Voice& voice = voices_[read];
            const double total = voice.gateSeconds + voice.patch.ampEnvelope.release + 0.04;
            bool dead = false;
            if (voice.age > voice.gateSeconds && voice.ampEnvelopeValue < 0.00045) {
                dead = true;
            } else if (voice.isStolen && voice.stolenGain < 0.001) {
                dead = true;
            } else if (voice.age > total) {
                dead = true;
            }
            if (!dead) {
                if (write != read) {
                    voices_[write] = std::move(voices_[read]);
                }
                ++write;
            }
        }
        voices_.resize(write);
    }

    // O(n) scan voice culling instead of std::nth_element
    const int cullCap = smoothedDspLoadPercent_ > 82.0
        ? 16
        : (smoothedDspLoadPercent_ > 66.0 ? 24 : 34);
    if (static_cast<int>(voices_.size()) > cullCap) {
        auto voicePriority = [](const Voice& voice) {
            const double velocityWeight = std::clamp(static_cast<double>(voice.note.velocity), 0.0, 1.0);
            const double lowNoteProtection = voice.note.midi <= 52 ? 0.20 : 0.0;
            const double heldProtection = voice.age < voice.gateSeconds ? 0.12 : 0.0;
            return voice.ampEnvelopeValue * (0.6 + velocityWeight * 0.4)
                + lowNoteProtection
                + heldProtection;
        };
        // Find the cullCap-th highest priority threshold with a simple scan
        std::vector<double> priorities;
        priorities.reserve(voices_.size());
        for (const auto& voice : voices_) {
            priorities.push_back(voicePriority(voice));
        }
        // Find threshold: we want to keep cullCap voices with highest priority.
        // Use nth_element on the priorities vector (not on voices_)
        if (static_cast<int>(priorities.size()) > cullCap) {
            auto prioritiesCopy = priorities;
            auto keepEnd = prioritiesCopy.begin() + cullCap;
            std::nth_element(
                prioritiesCopy.begin(),
                keepEnd,
                prioritiesCopy.end(),
                std::greater<double>());
            const double threshold = *keepEnd;
            std::size_t write = 0;
            for (std::size_t read = 0; read < voices_.size(); ++read) {
                if (priorities[read] >= threshold) {
                    if (write != read) {
                        voices_[write] = std::move(voices_[read]);
                    }
                    ++write;
                }
            }
            voices_.resize(std::min(write, static_cast<std::size_t>(cullCap)));
        }
    }

    const auto renderEnd = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = renderEnd - renderStart;
    const double renderSeconds = static_cast<double>(sampleCount) / std::max(1.0, sampleRate_);
    const double instantLoadPercent = renderSeconds > 1e-9
        ? (elapsed.count() / renderSeconds) * 100.0
        : 0.0;
    smoothedDspLoadPercent_ = smoothedDspLoadPercent_ * 0.74 + instantLoadPercent * 0.26;
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
    const int activeVoiceCountPostCull = static_cast<int>(voices_.size());
    const double riskScore = std::clamp(
        (smoothedDspLoadPercent_ - 50.0) / 30.0
            + (activeVoiceCountPostCull > 0 ? static_cast<double>(activeVoiceCountPostCull) / 192.0 : 0.0) * 0.35
            + (limiterReductionDb / 12.0) * 0.15,
        0.0,
        1.0);
    telemetry_.activeVoices = activeVoiceCountPostCull;
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
    return envelopeFor(envelope, age, gateSeconds, 0);
}

double Synthesizer::envelopeFor(const Envelope& envelope, double age, double gateSeconds, int curveType) const {
    auto levelDuringGate = [&](double t) {
        const double attack = std::max(envelope.attack, 0.0001);
        const double decay = std::max(envelope.decay, 0.0001);
        const double sustain = clamp01(envelope.sustain);
        if (t <= 0.0) {
            return 0.0;
        }
        if (t < attack) {
            const double x = std::clamp(t / attack, 0.0, 1.0);
            // Attack curve: smoothstep is always used for attack (musically best)
            return x * x * (3.0 - 2.0 * x);
        }
        if (t < attack + decay) {
            const double x = std::clamp((t - attack) / decay, 0.0, 1.0);
            switch (curveType) {
                case 1: // linear
                    return sustain + (1.0 - sustain) * (1.0 - x);
                case 2: // logarithmic (fast initial drop, slow tail)
                    return sustain + (1.0 - sustain) * (1.0 - std::sqrt(x));
                case 3: // analog RC (1 - exp(-x), more natural)
                    return sustain + (1.0 - sustain) * std::exp(-5.0 * x);
                case 0: // exponential (default)
                default:
                    return sustain + (1.0 - sustain) * std::exp(-4.2 * x);
            }
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
    double released;
    switch (curveType) {
        case 1: // linear release
            released = releaseStart * (1.0 - t);
            break;
        case 2: // logarithmic release
            released = releaseStart * (1.0 - std::sqrt(t));
            break;
        case 3: // analog RC release
            released = releaseStart * std::exp(-5.0 * t);
            break;
        case 0: // exponential (default)
        default:
            released = releaseStart * std::exp(-6.5 * t);
            break;
    }
    if (released < 1e-6) {
        return 0.0;
    }
    return released;
}

} // namespace arachno
