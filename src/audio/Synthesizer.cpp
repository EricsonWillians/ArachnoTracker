#include "Synthesizer.h"

#include <algorithm>
#include <cmath>

namespace arachno {

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double twoPi = 2.0 * pi;

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double centsToRatio(double cents) {
    return std::pow(2.0, cents / 1200.0);
}

double wrapPhase(double phase) {
    phase = std::fmod(phase, 1.0);
    return phase < 0.0 ? phase + 1.0 : phase;
}
} // namespace

Synthesizer::Synthesizer(double sampleRate) : sampleRate_(sampleRate) {}

void Synthesizer::setSampleRate(double sampleRate) {
    sampleRate_ = sampleRate;
}

void Synthesizer::noteOn(const Note& note, const SynthPatch& patch, double pan, double gateSeconds) {
    Voice voice;
    voice.note = note;
    voice.patch = patch;
    voice.pan = std::clamp(pan + patch.pan, -1.0, 1.0);
    voice.gateSeconds = std::max(0.01, gateSeconds);
    voice.noiseState = static_cast<std::uint32_t>((note.midi + 1) * 2654435761u);
    voices_.push_back(voice);
}

bool Synthesizer::active() const {
    return !voices_.empty();
}

void Synthesizer::render(float* left, float* right, int sampleCount) {
    for (int sample = 0; sample < sampleCount; ++sample) {
        double mixedLeft = 0.0;
        double mixedRight = 0.0;

        for (Voice& voice : voices_) {
            const double lfo = std::sin(voice.lfoPhase * twoPi);
            const double frequency = voice.note.frequency() * centsToRatio(lfo * voice.patch.vibratoCents);
            const double oscA = sampleOscillator(voice.patch.oscillatorA, voice.phaseA, voice.noiseState);
            const double oscB = sampleOscillator(voice.patch.oscillatorB, voice.phaseB, voice.noiseState);
            const double sub = sampleOscillator(Waveform::Square, voice.subPhase, voice.noiseState);
            const double noise = sampleOscillator(Waveform::Noise, 0.0, voice.noiseState);
            const double oscMix = clamp01(voice.patch.oscillatorMix);
            const double filterEnvelope = envelopeFor(
                voice.patch.filterEnvelope,
                voice.age,
                voice.gateSeconds);

            double value = oscA * (1.0 - oscMix)
                + oscB * oscMix
                + sub * voice.patch.subOscillator
                + noise * voice.patch.noise;

            const double cutoff = std::pow(clamp01(
                voice.patch.cutoff + filterEnvelope * voice.patch.filterEnvelopeAmount), 2.0);
            const double alpha = std::clamp(0.005 + cutoff * 0.995, 0.005, 1.0);
            voice.filterState += alpha * (value - voice.filterState);
            value = voice.filterState;

            const double drive = std::max(0.0, voice.patch.drive);
            value = std::tanh(value * (1.0 + drive * 8.0));
            value *= envelopeFor(voice.patch.ampEnvelope, voice.age, voice.gateSeconds);
            value *= 1.0 - clamp01(voice.patch.tremoloDepth) * ((lfo + 1.0) * 0.5);
            value *= voice.patch.gain * voice.note.velocity;

            const double leftGain = std::cos((voice.pan + 1.0) * pi * 0.25);
            const double rightGain = std::sin((voice.pan + 1.0) * pi * 0.25);
            mixedLeft += value * leftGain;
            mixedRight += value * rightGain;

            voice.phaseA = wrapPhase(voice.phaseA + frequency / sampleRate_);
            voice.phaseB = wrapPhase(voice.phaseB + (frequency * centsToRatio(voice.patch.detuneCents)) / sampleRate_);
            voice.subPhase = wrapPhase(voice.subPhase + (frequency * 0.5) / sampleRate_);
            voice.lfoPhase = wrapPhase(voice.lfoPhase + std::max(0.0, voice.patch.lfoRate) / sampleRate_);
            voice.age += 1.0 / sampleRate_;
        }

        voices_.erase(
            std::remove_if(voices_.begin(), voices_.end(), [](const Voice& voice) {
                const double total = voice.gateSeconds + voice.patch.ampEnvelope.release + 0.04;
                return voice.age > total;
            }),
            voices_.end());

        left[sample] += static_cast<float>(mixedLeft);
        right[sample] += static_cast<float>(mixedRight);
    }
}

double Synthesizer::sampleOscillator(Waveform waveform, double phase, std::uint32_t& noiseState) const {
    switch (waveform) {
        case Waveform::Sine:
            return std::sin(phase * twoPi);
        case Waveform::Square:
            return phase < 0.5 ? 1.0 : -1.0;
        case Waveform::Saw:
            return phase * 2.0 - 1.0;
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
    return std::max(0.0, envelope.sustain * (1.0 - t));
}

} // namespace arachno
