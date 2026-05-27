#include "Synthesizer.h"

#include <algorithm>
#include <cmath>

namespace arachno {

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double twoPi = 2.0 * pi;
constexpr int maxSynthUnisonVoices = 8;

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
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

double unisonPosition(int index, int count) {
    if (count <= 1) {
        return 0.0;
    }
    return -1.0 + 2.0 * static_cast<double>(index) / static_cast<double>(count - 1);
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

void Synthesizer::noteOn(const Note& note, const SynthPatch& patch, double pan, double gateSeconds) {
    if (voices_.size() >= static_cast<std::size_t>(maxActiveVoices)) {
        auto oldest = std::max_element(voices_.begin(), voices_.end(), [](const Voice& left, const Voice& right) {
            return left.age < right.age;
        });
        if (oldest != voices_.end()) {
            voices_.erase(oldest);
        }
    }

    Voice voice;
    voice.note = note;
    voice.patch = patch;
    voice.pan = std::clamp(pan + patch.pan, -1.0, 1.0);
    voice.gateSeconds = std::max(0.01, gateSeconds);
    voice.noiseState = static_cast<std::uint32_t>((note.midi + 1) * 2654435761u);
    const int unisonCount = unisonVoiceCount(patch);
    for (int index = 0; index < unisonCount; ++index) {
        const double offset = static_cast<double>(index) / static_cast<double>(unisonCount);
        voice.unisonPhaseA[static_cast<std::size_t>(index)] = wrapPhase(offset);
        voice.unisonPhaseB[static_cast<std::size_t>(index)] = wrapPhase(offset * 0.37);
        voice.unisonPhaseC[static_cast<std::size_t>(index)] = wrapPhase(offset * 0.73);
        voice.unisonPhaseD[static_cast<std::size_t>(index)] = wrapPhase(offset * 0.19);
    }
    voices_.push_back(voice);
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
}

void Synthesizer::render(float* left, float* right, int sampleCount) {
    if (sampleCount <= 0) {
        return;
    }
    const double invSampleRate = sampleRate_ > 0.0 ? (1.0 / sampleRate_) : 0.0;
    const int activeVoiceCount = static_cast<int>(voices_.size());
    const bool heavyLoad = activeVoiceCount > 40;
    const bool extremeLoad = activeVoiceCount > 72;

    for (int sample = 0; sample < sampleCount; ++sample) {
        double mixedLeft = 0.0;
        double mixedRight = 0.0;

        for (Voice& voice : voices_) {
            const double lfo = std::sin(voice.lfoPhase * twoPi);
            const double velocityNorm = std::clamp(static_cast<double>(voice.note.velocity), 0.0, 1.0);
            const double driftAmountCents = 0.22
                + clamp01(voice.patch.drive) * 0.95
                + clamp01(voice.patch.unisonDetuneCents / 20.0) * 0.8;
            voice.driftA = stepDrift(voice.driftA, voice.noiseState, driftAmountCents);
            voice.driftB = stepDrift(voice.driftB, voice.noiseState, driftAmountCents * 1.13);
            voice.driftC = stepDrift(voice.driftC, voice.noiseState, driftAmountCents * 1.07);
            voice.driftD = stepDrift(voice.driftD, voice.noiseState, driftAmountCents * 0.92);
            const double driftRatioA = centsToRatio(voice.driftA);
            const double driftRatioB = centsToRatio(voice.driftB);
            const double driftRatioC = centsToRatio(voice.driftC);
            const double driftRatioD = centsToRatio(voice.driftD);
            const double pitchDecay = std::max(0.001, voice.patch.pitchEnvelopeDecay);
            const double pitchEnvelope = voice.patch.pitchEnvelopeSemitones * std::exp(-voice.age / pitchDecay);
            const double transientPitchDecay = std::max(0.001, voice.patch.transientPitchDecay);
            const double transientPitchEnvelope = voice.patch.transientPitchSemitones
                * std::exp(-voice.age / transientPitchDecay);
            const double frequency = voice.note.frequency()
                * centsToRatio(lfo * voice.patch.vibratoCents)
                * semitonesToRatio(pitchEnvelope + transientPitchEnvelope);
            const double detuneRatioB = centsToRatio(voice.patch.detuneCents);
            const double detuneRatioC = centsToRatio(voice.patch.detuneCCents);
            const double detuneRatioD = centsToRatio(voice.patch.detuneDCents);
            int unisonCount = unisonVoiceCount(voice.patch);
            if (activeVoiceCount > 0) {
                const int adaptiveBudget = std::max(1, 96 / activeVoiceCount);
                unisonCount = std::min(unisonCount, adaptiveBudget);
            }
            const double syncAmount = voice.patch.hardSyncEnabled ? clamp01(voice.patch.hardSync) : 0.0;
            const double pulseWidth = std::clamp(
                voice.patch.pulseWidth + lfo * clamp01(voice.patch.pwmDepth) * 0.45,
                0.03,
                0.97);
            const double fmRatio = std::max(0.125, voice.patch.fmRatio);
            const double fmDepth = voice.patch.fmEnabled ? clamp01(voice.patch.fmAmount) * 0.35 : 0.0;
            const double fmFeedback = voice.patch.fmEnabled ? clamp01(voice.patch.fmFeedback) * 0.5 : 0.0;
            const bool oscAEnabled = voice.patch.oscillatorAEnabled;
            const bool oscBEnabled = voice.patch.oscillatorBEnabled;
            const bool oscCEnabled = voice.patch.oscillatorCEnabled;
            const bool oscDEnabled = voice.patch.oscillatorDEnabled;
            double oscA = 0.0;
            double oscB = 0.0;
            double oscC = 0.0;
            double oscD = 0.0;
            double unisonSide = 0.0;
            const double oscMix = clamp01(voice.patch.oscillatorMix);
            const double oscCMix = clamp01(voice.patch.oscillatorCMix);
            const double oscDMix = clamp01(voice.patch.oscillatorDMix);
            for (int index = 0; index < unisonCount; ++index) {
                const std::size_t unisonIndex = static_cast<std::size_t>(index);
                const double position = unisonPosition(index, unisonCount);
                const double phaseA = voice.unisonPhaseA[unisonIndex];
                const double phaseB = voice.unisonPhaseB[unisonIndex];
                const double phaseC = voice.unisonPhaseC[unisonIndex];
                const double phaseD = voice.unisonPhaseD[unisonIndex];
                const double unisonRatio = centsToRatio(position * voice.patch.unisonDetuneCents);
                const double incrementA = std::clamp((frequency * unisonRatio * driftRatioA) * invSampleRate, 0.0, 0.49);
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
                const double fmCarrier = std::sin(wrapPhase(phaseA * fmRatio + phaseB + phaseC * 0.5) * twoPi);
                const double fm = (fmCarrier + voice.fmFeedbackState * fmFeedback) * fmDepth;
                const double sampleA = oscAEnabled
                    ? sampleOscillator(
                        voice.patch.oscillatorA,
                        wrapPhase(phaseA + fm),
                        incrementA,
                        voice.noiseState,
                        pulseWidth)
                    : 0.0;
                const double sampleB = oscBEnabled
                    ? sampleOscillator(
                        voice.patch.oscillatorB,
                        syncedPhaseB,
                        incrementB,
                        voice.noiseState,
                        pulseWidth)
                    : 0.0;
                const double sampleC = oscCEnabled
                    ? sampleOscillator(
                        voice.patch.oscillatorC,
                        wrapPhase(phaseC + fm * 0.5),
                        incrementC,
                        voice.noiseState,
                        pulseWidth)
                    : 0.0;
                const double sampleD = oscDEnabled
                    ? sampleOscillator(
                        voice.patch.oscillatorD,
                        wrapPhase(phaseD + fm * 0.3),
                        incrementD,
                        voice.noiseState,
                        pulseWidth)
                    : 0.0;
                oscA += sampleA;
                oscB += sampleB;
                oscC += sampleC;
                oscD += sampleD;
                const double ab = sampleA * (1.0 - oscMix) + sampleB * oscMix;
                const double abc = ab * (1.0 - oscCMix) + sampleC * oscCMix;
                const double combined = abc * (1.0 - oscDMix) + sampleD * oscDMix;
                unisonSide += combined * position;

                voice.unisonPhaseA[unisonIndex] = wrapPhase(phaseA + incrementA);
                voice.unisonPhaseB[unisonIndex] = wrapPhase(phaseB + incrementB);
                voice.unisonPhaseC[unisonIndex] = wrapPhase(phaseC + incrementC);
                voice.unisonPhaseD[unisonIndex] = wrapPhase(phaseD + incrementD);
            }
            oscA /= static_cast<double>(unisonCount);
            oscB /= static_cast<double>(unisonCount);
            oscC /= static_cast<double>(unisonCount);
            oscD /= static_cast<double>(unisonCount);
            unisonSide /= static_cast<double>(unisonCount);
            const double subIncrement = std::clamp((frequency * 0.5) * invSampleRate, 0.0, 0.49);
            const double sub = voice.patch.subEnabled
                ? sampleOscillator(Waveform::Square, voice.subPhase, subIncrement, voice.noiseState, 0.5)
                : 0.0;
            const double whiteNoise = sampleOscillator(Waveform::Noise, 0.0, 0.0, voice.noiseState);
            const double noise = voice.patch.noiseEnabled
                ? sculptNoiseSample(whiteNoise, voice.patch.noiseTone, voice.noiseColorState)
                : 0.0;
            const double filterEnvelope = envelopeFor(
                voice.patch.filterEnvelope,
                voice.age,
                voice.gateSeconds);

            const double oscAB = oscA * (1.0 - oscMix) + oscB * oscMix;
            const double oscABC = oscAB * (1.0 - oscCMix) + oscC * oscCMix;
            const double oscABCD = oscABC * (1.0 - oscDMix) + oscD * oscDMix;
            int enabledOscillators = 0;
            enabledOscillators += oscAEnabled ? 1 : 0;
            enabledOscillators += oscBEnabled ? 1 : 0;
            enabledOscillators += oscCEnabled ? 1 : 0;
            enabledOscillators += oscDEnabled ? 1 : 0;
            const double sourceWeight = static_cast<double>(enabledOscillators)
                + (voice.patch.subEnabled ? 0.85 : 0.0)
                + (voice.patch.noiseEnabled ? 0.65 : 0.0);
            const double levelComp = 1.0 / std::sqrt(std::max(1.0, sourceWeight));
            double value = oscABCD
                + sub * voice.patch.subOscillator
                + noise * voice.patch.noise;
            const double ringAmount = voice.patch.ringEnabled ? clamp01(voice.patch.ringMod) : 0.0;
            value = value * (1.0 - ringAmount)
                + (oscA * (oscB + oscC * 0.5 + oscD * 0.35)) * ringAmount;
            value *= levelComp;

            const double cutoffNormalized = clamp01(
                voice.patch.cutoff
                    + filterEnvelope * voice.patch.filterEnvelopeAmount
                    + lfo * clamp01(voice.patch.lfoFilterDepth) * 0.35
                    + (velocityNorm - 0.6) * 0.16
                    + (static_cast<double>(voice.note.midi - 60) / 48.0) * 0.12);
            const double cutoff = cutoffNormalized * cutoffNormalized;
            const double cutoffHz = std::clamp(
                25.0 * std::exp2(cutoff * 10.0),
                20.0,
                sampleRate_ * 0.45);
            const double alpha = std::clamp(1.0 - std::exp(-twoPi * cutoffHz / sampleRate_), 0.001, 0.999);
            const double resonance = clamp01(voice.patch.resonance);
            const double feedback = resonance * (1.2 - cutoff * 0.9);
            const double filterInput = value - (voice.filterState - voice.filterState2) * feedback;
            voice.filterState += alpha * (filterInput - voice.filterState);
            voice.filterState2 += alpha * (voice.filterState - voice.filterState2);
            value = voice.filterState2;

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

            value = wavefoldSample(value, voice.patch.wavefold);

            const double highPass = clamp01(voice.patch.highPass);
            if (highPass > 0.0) {
                const double hpAlpha = std::clamp(0.002 + highPass * 0.45, 0.002, 0.6);
                voice.highPassState += hpAlpha * (value - voice.highPassState);
                value -= voice.highPassState;
            }

            const double transientDecay = std::max(0.001, voice.patch.transientDecay);
            const double transientEnvelope = std::exp(-voice.age / transientDecay);
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
                    double burstDecayMul = 1.0;
                    for (int burst = 0; burst < burstCount; ++burst) {
                        const double start = static_cast<double>(burst) * burstSpacing;
                        if (voice.age < start) {
                            break;
                        }
                        const double elapsed = voice.age - start;
                        burstEnvelope += std::exp(-elapsed / transientDecay) * burstDecayMul;
                        burstDecayMul *= burstDecay;
                    }
                }
            }
            const double transientNoise = sculptNoiseSample(
                whiteNoise,
                voice.patch.transientTone,
                voice.transientColorState);
            const double transientShape = 1.0 + clamp01(voice.patch.transientShape) * 8.0;
            const double shapedBurst = std::tanh(burstEnvelope * transientShape);
            value += clickEnvelope * clamp01(voice.patch.click)
                + transientEnvelope * transientAmount * transientNoise
                + shapedBurst * transientAmount * transientNoise * 0.65;

            const double drive = std::max(0.0, voice.patch.drive);
            value = std::tanh(value * (1.0 + drive * 8.0)) * (1.0 + drive * 0.45);
            const double ampEnvelope = envelopeFor(voice.patch.ampEnvelope, voice.age, voice.gateSeconds);
            value *= ampEnvelope;
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
            const double sideValue = crushSample(
                std::tanh(unisonSide * (1.0 + drive * 4.0))
                    * ampEnvelope
                    * voice.patch.gain
                    * voice.note.velocity
                    * clamp01(voice.patch.stereoSpread),
                voice.patch.bitCrush);
            double voiceLeft = value * leftGain;
            double voiceRight = value * rightGain;
            voiceLeft -= sideValue * 0.5;
            voiceRight += sideValue * 0.5;

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
            mixedLeft += voiceLeft;
            mixedRight += voiceRight;
            voice.phaseA = voice.unisonPhaseA.front();
            voice.phaseB = voice.unisonPhaseB.front();
            voice.phaseC = voice.unisonPhaseC.front();
            voice.phaseD = voice.unisonPhaseD.front();
            voice.subPhase = wrapPhase(voice.subPhase + (frequency * 0.5) * invSampleRate);
            voice.lfoPhase = wrapPhase(voice.lfoPhase + std::max(0.0, voice.patch.lfoRate) * invSampleRate);
            voice.chorusPhase = wrapPhase(voice.chorusPhase + std::max(0.0, voice.patch.chorusRate) * invSampleRate);
            voice.age += invSampleRate;
        }

        mixedLeft = std::tanh(mixedLeft * 0.92) / 0.92;
        mixedRight = std::tanh(mixedRight * 0.92) / 0.92;
        const double dcCoeff = 0.995;
        const double dcLeft = mixedLeft - outputDcInputLeft_ + dcCoeff * outputDcOutputLeft_;
        const double dcRight = mixedRight - outputDcInputRight_ + dcCoeff * outputDcOutputRight_;
        outputDcInputLeft_ = mixedLeft;
        outputDcOutputLeft_ = dcLeft;
        outputDcInputRight_ = mixedRight;
        outputDcOutputRight_ = dcRight;
        left[sample] += static_cast<float>(dcLeft);
        right[sample] += static_cast<float>(dcRight);
    }

    voices_.erase(
        std::remove_if(voices_.begin(), voices_.end(), [](const Voice& voice) {
            const double total = voice.gateSeconds + voice.patch.ampEnvelope.release + 0.04;
            return voice.age > total;
        }),
        voices_.end());
}

double Synthesizer::sampleOscillator(
    Waveform waveform,
    double phase,
    double phaseIncrement,
    std::uint32_t& noiseState,
    double pulseWidth) const {
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
            return 1.0 - 4.0 * std::abs(phase - 0.5);
        case Waveform::Noise:
            noiseState = noiseState * 1664525u + 1013904223u;
            return (static_cast<double>((noiseState >> 8) & 0x00ffffff) / 8388607.5) - 1.0;
    }
    return 0.0;
}

double Synthesizer::envelopeFor(const Envelope& envelope, double age, double gateSeconds) const {
    const double attack = std::max(envelope.attack, 0.0001);
    const double decay = std::max(envelope.decay, 0.0001);
    const double releaseStart = std::max(attack + decay, gateSeconds);

    if (age < attack) {
        return age / attack;
    }
    if (age < attack + decay) {
        const double t = (age - attack) / decay;
        return 1.0 + (envelope.sustain - 1.0) * t;
    }
    if (age < releaseStart) {
        return envelope.sustain;
    }

    const double release = std::max(envelope.release, 0.0001);
    const double t = (age - releaseStart) / release;
    if (t >= 1.0) {
        return 0.0;
    }
    return std::max(0.0, envelope.sustain * std::exp(-5.2 * t));
}

} // namespace arachno
