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
    double chorusFeedback = 0.12;
    double chorusDelay = 0.42;
    double chorusWidth = 0.55;
    double chorusEnsemble = 0.0;
    int unisonVoices = 1;
    double unisonDetuneCents = 0.0;
    double stereoSpread = 0.0;
    bool subEnabled = false;
    double subOscillator = 0.24;
    bool noiseEnabled = false;
    double noise = 0.0;
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
    double delayMix = 0.0;
    double delayTime = 0.22;
    double delayFeedback = 0.32;
    double delayTone = 0.6;
    double delayStereo = 0.0;
    double delayModDepth = 0.333;
    double delayDrive = 0.0;
    double delayDucking = 0.0;
    double reverbMix = 0.0;
    double reverbSize = 0.58;
    double reverbDamping = 0.46;
    double reverbPreDelay = 0.08;
    double reverbDiffusion = 0.5;
    double reverbWidth = 0.0;
    double reverbShimmer = 0.0;
    double reverbModDepth = 0.0;
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
    double vintageDrift = 0.35;
    double wowFlutter = 0.08;
    double toneTilt = -0.10;
    double tapeColor = 0.0;
    double airBoost = 0.0;
    double lowPunch = 0.0;
    double analogWarmth = 0.36;
    double voiceSlop = 0.28;
    double phaseScatter = 0.22;
    double unisonWarp = 0.18;
    double unisonHumanize = 0.32;
    double fmColor = 0.5;
    double fmSpread = 0.0;
    double chorusTone = 0.58;
    double chorusJitter = 0.26;
    double chorusSaturation = 0.24;
    double delayDiffusion = 0.24;
    double delayWow = 0.22;
    double delayCrossfeed = 0.36;
    double reverbDecay = 0.62;
    double reverbEarlyMix = 0.28;
    double reverbTone = 0.52;
    double reverbChorus = 0.2;
    double reverbBloom = 0.24;
    double consoleCrosstalk = 0.06;
    double stereoDepth = 0.22;
    double hifiExciter = 0.24;
    double outputTransformer = 0.2;
    double outputSoftClip = 0.28;
    double outputGlue = 0.24;
    double gain = 0.52;
    double pan = 0.0;
    Envelope ampEnvelope;
    Envelope filterEnvelope;
    // Velocity expression system (Phase 1)
    double velocityToAmp = 0.85;       // depth of velocity→amplitude (0=ignore, 1=full)
    double velocityToFilter = 0.2;     // depth of velocity→cutoff boost
    double velocityToAttack = 0.15;    // depth of velocity→attack shortening
    int velocityCurve = 1;             // 0=linear, 1=exponential, 2=logarithmic, 3=DX7-style
    // Filter enhancements (Phase 3)
    double filterKeytrackResonance = 0.0; // key-scaled resonance (higher notes = more bite)
    double filterNonlinearity = 0.25;     // TPT SVF internal state saturation (0=linear, 1=max)
    // Envelope curve (Phase 4)
    int ampEnvelopeCurve = 0;          // 0=exponential, 1=linear, 2=log, 3=analog-RC
    int filterEnvelopeCurve = 0;       // same curves for filter envelope
};

struct Instrument {
    int id = 0;
    SynthPatch patch;
};

const char* waveformName(Waveform waveform);
Waveform waveformFromName(const std::string& name);
bool setSynthPatchParameter(SynthPatch& patch, const std::string& parameter, double value);

} // namespace arachno
