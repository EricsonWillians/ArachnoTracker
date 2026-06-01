#pragma once

#include <string>

namespace arachno {

enum class Waveform {
    Sine,
    Square,
    Saw,
    Triangle,
    Noise,
    SuperSaw
};

struct Envelope {
    double attack = 0.005;
    double decay = 0.08;
    double sustain = 0.72;
    double release = 0.18;
};

struct SynthPatch {
    std::string name = "Init";
    Waveform oscillatorA = Waveform::Sine;
    Waveform oscillatorB = Waveform::Sine;
    Waveform oscillatorC = Waveform::Sine;
    Waveform oscillatorD = Waveform::Sine;
    bool oscillatorAEnabled = true;
    bool oscillatorBEnabled = false;
    bool oscillatorCEnabled = false;
    bool oscillatorDEnabled = false;
    double oscillatorMix = 0.35;
    double oscillatorCMix = 0.0;
    double oscillatorDMix = 0.0;
    double oscALevel = 1.0;
    double oscBLevel = 1.0;
    double oscCLevel = 1.0;
    double oscDLevel = 1.0;
    double detuneCents = 7.0;
    double detuneCCents = -7.0;
    double detuneDCents = 12.0;
    double oscADetuneCents = 0.0;
    double oscBDetuneCents = 0.0;
    double oscCDetuneCents = 0.0;
    double oscDDetuneCents = 0.0;
    double pulseWidth = 0.5;
    double pwmDepth = 0.0;
    double oscAPulseWidth = 0.5;
    double oscBPulseWidth = 0.5;
    double oscCPulseWidth = 0.5;
    double oscDPulseWidth = 0.5;
    double oscAPwmDepth = 0.0;
    double oscBPwmDepth = 0.0;
    double oscCPwmDepth = 0.0;
    double oscDPwmDepth = 0.0;
    double oscADrive = 0.0;
    double oscBDrive = 0.0;
    double oscCDrive = 0.0;
    double oscDDrive = 0.0;
    bool fmEnabled = false;
    double fmAmount = 0.0;
    double fmRatio = 2.0;
    double fmFeedback = 0.0;
    int fmAlgorithm = 0;
    bool chorusEnabled = false;
    double chorusMix = 0.0;
    double chorusRate = 0.35;
    double chorusDepth = 0.25;
    int unisonVoices = 1;
    double unisonDetuneCents = 0.0;
    double stereoSpread = 0.0;
    bool subEnabled = true;
    double subOscillator = 0.24;
    bool noiseEnabled = true;
    double noise = 0.012;
    double noiseTone = 0.58;
    double cutoff = 0.72;
    double resonance = 0.16;
    int filterMode = 0;
    double filterDrive = 0.2;
    double filterKeytrack = 0.35;
    double filterEnvelopeAmount = 0.18;
    double lfoFilterDepth = 0.0;
    double lfoPanDepth = 0.0;
    double pitchEnvelopeSemitones = 0.0;
    double pitchEnvelopeDecay = 0.08;
    double lfoRate = 5.5;
    double vibratoCents = 0.0;
    double tremoloDepth = 0.0;
    bool ringEnabled = false;
    double ringMod = 0.0;
    bool hardSyncEnabled = false;
    double hardSync = 0.0;
    double drive = 0.12;
    double wavefold = 0.0;
    bool bitCrushEnabled = false;
    double bitCrush = 0.0;
    double sampleRateReduction = 0.0;
    double combMix = 0.0;
    double combTime = 0.08;
    double combFeedback = 0.15;
    double highPass = 0.0;
    double click = 0.0;
    double transientShape = 0.0;
    double transientNoise = 0.0;
    double transientPitchSemitones = 0.0;
    double transientPitchDecay = 0.01;
    int transientBurstCount = 1;
    double transientBurstSpacing = 0.004;
    double transientBurstDecay = 0.7;
    double transientTone = 0.72;
    double transientDecay = 0.012;
    double analogColor = 0.45;
    double toneTilt = -0.10;
    double gain = 0.52;
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
