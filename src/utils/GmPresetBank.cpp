#include "GmPresetBank.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace arachno {

namespace {

// Neutral, warm starting point shared by all GM presets. Each family overrides
// what it needs. Gain staging is conservative so dense imports do not slam the
// master limiter.
SynthPatch basePreset() {
    SynthPatch p;
    p.oscillatorA = Waveform::Saw;
    p.oscillatorB = Waveform::Square;
    p.oscillatorC = Waveform::Triangle;
    p.oscillatorBEnabled = true;
    p.oscillatorCEnabled = false;
    p.oscillatorMix = 0.40;
    p.detuneCents = 5.0;
    p.detuneCCents = -5.0;
    p.cutoff = 0.62;
    p.resonance = 0.14;
    p.filterDrive = 0.12;
    p.filterNonlinearity = 0.08;
    p.filterKeytrack = 0.45;
    p.filterEnvelopeAmount = 0.25;
    p.filterEnvelope.attack = 0.004;
    p.filterEnvelope.decay = 0.18;
    p.filterEnvelope.sustain = 0.60;
    p.filterEnvelope.release = 0.20;
    p.drive = 0.10;
    p.ampEnvelope.attack = 0.004;
    p.ampEnvelope.decay = 0.25;
    p.ampEnvelope.sustain = 0.75;
    p.ampEnvelope.release = 0.22;
    p.reverbMix = 0.14;
    p.reverbSize = 0.62;
    p.reverbDamping = 0.42;
    p.reverbDecay = 0.60;
    p.reverbWidth = 0.70;
    p.delayMix = 0.0;
    // Keep the shared "house tone" subtle: heavy uniform color was masking the
    // differences between families. Families re-add character where it belongs.
    p.analogColor = 0.16;
    p.analogWarmth = 0.38;
    p.vintageDrift = 0.08;
    p.wowFlutter = 0.02;
    p.voiceSlop = 0.06;
    p.phaseScatter = 0.08;
    p.tapeColor = 0.05;
    p.hifiExciter = 0.10;
    p.outputTransformer = 0.12;
    p.outputSoftClip = 0.14;
    p.outputGlue = 0.16;
    p.velocityToAmp = 0.70;
    p.velocityToFilter = 0.25;
    p.velocityCurve = 1;
    p.gain = 0.70;
    return p;
}

// Key-scaled envelope compensation: higher registers decay/release faster,
// like real instruments (and rompler key scaling).
void applyKeyScaling(SynthPatch& p, double averageMidi) {
    const double ks = std::clamp(1.0 - (averageMidi - 60.0) / 96.0, 0.50, 1.50);
    p.ampEnvelope.decay = std::max(0.005, p.ampEnvelope.decay * ks);
    p.ampEnvelope.release = std::max(0.005, p.ampEnvelope.release * ks);
    p.filterEnvelope.decay = std::max(0.005, p.filterEnvelope.decay * ks);
}

void enableJunoChorus(SynthPatch& p, double mix, double ensemble) {
    p.chorusEnabled = true;
    p.chorusMix = mix;
    p.chorusEnsemble = ensemble;
    p.chorusRate = 0.55;
    p.chorusDepth = 0.42;
    p.chorusWidth = 0.72;
}

// Layered acoustic piano skeleton (programs 0/1/3): a slow triangle body plus
// decaying string-partial layers (per-osc ratio + 1-pole layer decay) and a
// hammer transient. Low notes ring longer (keyTrackDecay), hard strikes ring
// longer and brighter (velocityToDecay / velocityToFilter).
SynthPatch acousticPianoBase() {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Triangle; // slow body
    p.oscillatorB = Waveform::Sine;     // 3rd-partial string brightness, dies fast
    p.oscBRatio = 3.0;
    p.oscBDecay = 0.28;
    p.oscBLevel = 0.90;
    p.oscillatorC = Waveform::Sine;     // fast inharmonic clang layer
    p.oscillatorCEnabled = true;
    p.oscillatorCMix = 0.28;
    p.oscCRatio = 4.2;
    p.oscCDecay = 0.09;
    p.oscCLevel = 0.80;
    p.oscillatorMix = 0.30;
    p.unisonVoices = 2; // paired strings per note
    p.unisonDetuneCents = 4.0;
    p.cutoff = 0.60;
    p.resonance = 0.06;
    p.filterKeytrack = 0.80; // true 1:1 tracking: bright highs, dark lows
    p.filterEnvelopeAmount = 0.20;
    p.filterEnvelope.attack = 0.001;
    p.filterEnvelope.decay = 0.30;
    p.filterEnvelope.sustain = 0.10;
    p.ampEnvelope.attack = 0.001;
    p.ampEnvelope.decay = 2.60;
    p.ampEnvelope.sustain = 0.0;
    p.ampEnvelope.release = 0.25;
    p.keyTrackDecay = 0.60;
    p.velocityToDecay = 0.50;
    p.velocityToAmp = 0.92;
    p.velocityToFilter = 0.45;
    p.velocityCurve = 3; // DX7-style dynamics
    p.click = 0.05;
    p.transientNoise = 0.10; // hammer thump
    p.transientDecay = 0.010;
    p.lowPunch = 0.18;
    p.analogColor = 0.10; // clean, not woozy
    p.analogWarmth = 0.30;
    p.vintageDrift = 0.05;
    p.reverbMix = 0.20;
    p.reverbSize = 0.75;
    p.reverbDecay = 0.70;
    p.gain = 0.62;
    return p;
}

// Family 0: Piano — M1/D-50 layered acoustics + DX7 electric pianos
SynthPatch pianoPreset(int program) {
    if (program <= 1 || program == 3) {
        SynthPatch p = acousticPianoBase();
        if (program == 1) { // Bright Acoustic
            p.cutoff = 0.68;
            p.oscBRatio = 3.5;
            p.oscBDecay = 0.35;
            p.oscillatorMix = 0.38;
            p.transientShape = 0.20;
            p.ampEnvelope.decay = 2.20;
        } else if (program == 3) { // Honky-tonk (detuned triple strike)
            p.detuneCents = 12.0;
            p.detuneCCents = -12.0;
            p.oscillatorCMix = 0.35;
            p.oscCDecay = 0.15;
            p.ampEnvelope.decay = 1.90;
        }
        return p;
    }
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Triangle;
    p.oscillatorB = Waveform::Square;
    p.oscillatorMix = 0.25;
    p.unisonVoices = 2;
    p.unisonDetuneCents = 3.0;
    p.cutoff = 0.66;
    p.resonance = 0.08;
    p.filterKeytrack = 0.55;
    p.filterEnvelopeAmount = 0.30;
    p.filterEnvelope.attack = 0.001;
    p.filterEnvelope.decay = 0.30;
    p.filterEnvelope.sustain = 0.15;
    p.ampEnvelope.attack = 0.002;
    p.ampEnvelope.decay = 1.60;
    p.ampEnvelope.sustain = 0.12;
    p.ampEnvelope.release = 0.18;
    p.velocityToAmp = 0.90;
    p.velocityToFilter = 0.35;
    p.click = 0.06;
    p.transientNoise = 0.08; // hammer thump
    p.transientDecay = 0.012;
    p.lowPunch = 0.15;
    p.reverbMix = 0.20;
    p.reverbSize = 0.75;
    p.reverbDecay = 0.70;
    p.gain = 0.66;
    switch (program) {
        case 1: // Bright Acoustic
            break;
        case 2: { // Electric Grand (CP-80-ish: FM edge + chorus + FM decay)
            p = basePreset();
            p.oscillatorA = Waveform::Triangle;
            p.oscillatorB = Waveform::Sine;
            p.oscillatorMix = 0.30;
            p.fmEnabled = true;
            p.fmAmount = 0.16;
            p.fmRatio = 2.0;
            p.fmDecay = 0.25;
            p.cutoff = 0.72;
            p.ampEnvelope.attack = 0.002;
            p.ampEnvelope.decay = 1.80;
            p.ampEnvelope.sustain = 0.10;
            p.ampEnvelope.release = 0.20;
            p.transientNoise = 0.10;
            p.velocityToAmp = 0.88;
            p.velocityToDecay = 0.40;
            p.keyTrackDecay = 0.40;
            enableJunoChorus(p, 0.15, 0.10);
            p.reverbMix = 0.18;
            p.gain = 0.64;
            break;
        }
        case 3: // Honky-tonk (detuned triple strike)
            break;
        case 4: { // Electric Piano 1 — DX7 Rhodes: tine clang decays to warm body
            p = basePreset();
            p.oscillatorA = Waveform::Sine;
            p.oscillatorB = Waveform::Sine;
            p.oscillatorMix = 0.15;
            p.fmEnabled = true;
            p.fmAmount = 0.40;
            p.fmRatio = 1.0;
            p.fmFeedback = 0.10;
            p.fmDecay = 0.45;       // the DX7 signature: metallic attack dies to warmth
            p.velocityToFm = 0.55;  // soft = mellow, hard = bark
            p.cutoff = 0.74;
            p.filterEnvelopeAmount = 0.20;
            p.ampEnvelope.attack = 0.002;
            p.ampEnvelope.decay = 2.20;
            p.ampEnvelope.sustain = 0.05;
            p.ampEnvelope.release = 0.25;
            p.tremoloDepth = 0.14;
            p.lfoRate = 4.5;
            p.velocityCurve = 3; // DX7-style
            p.velocityToAmp = 0.90;
            p.velocityToDecay = 0.45; // hard strike = longer tine ring
            p.keyTrackDecay = 0.35;
            p.reverbMix = 0.20;
            p.reverbSize = 0.75;
            p.gain = 0.62;
            break;
        }
        case 5: { // Electric Piano 2 — brighter FM "Dyno" EP with bite
            p = basePreset();
            p.oscillatorA = Waveform::Sine;
            p.oscillatorB = Waveform::Sine;
            p.oscillatorMix = 0.20;
            p.fmEnabled = true;
            p.fmAmount = 0.48;
            p.fmRatio = 3.0;
            p.fmFeedback = 0.08;
            p.fmDecay = 0.30;
            p.velocityToFm = 0.60;
            p.cutoff = 0.78;
            p.transientNoise = 0.10;
            p.ampEnvelope.attack = 0.002;
            p.ampEnvelope.decay = 1.60;
            p.ampEnvelope.sustain = 0.04;
            p.ampEnvelope.release = 0.22;
            p.velocityCurve = 3;
            p.velocityToAmp = 0.88;
            p.velocityToDecay = 0.45;
            p.keyTrackDecay = 0.35;
            p.reverbMix = 0.18;
            p.gain = 0.62;
            break;
        }
        case 6: // Harpsichord
            p.oscillatorA = Waveform::Square;
            p.oscillatorB = Waveform::Saw;
            p.oscBRatio = 2.0;   // octave string partial
            p.oscBDecay = 0.12;  // plectrum snap dies into the body
            p.keyTrackDecay = 0.50;
            p.oscillatorMix = 0.45;
            p.unisonVoices = 1;
            p.highPass = 0.20;
            p.cutoff = 0.80;
            p.ampEnvelope.decay = 0.55;
            p.ampEnvelope.sustain = 0.0;
            p.ampEnvelope.release = 0.06;
            p.click = 0.18;
            p.transientNoise = 0.06;
            p.velocityToAmp = 0.25;
            p.velocityCurve = 0;
            break;
        case 7: // Clavinet
            p.oscillatorA = Waveform::Square;
            p.unisonVoices = 1;
            p.oscBRatio = 2.0;  // string partial layer
            p.oscBDecay = 0.06; // fast "twang" snap
            p.oscAPulseWidth = 0.30;
            p.pulseWidth = 0.30;
            p.highPass = 0.35;
            p.cutoff = 0.60;
            p.filterEnvelopeAmount = 0.50;
            p.filterEnvelope.decay = 0.08;
            p.ampEnvelope.decay = 0.35;
            p.ampEnvelope.sustain = 0.0;
            p.ampEnvelope.release = 0.05;
            p.click = 0.22;
            p.transientShape = 0.25;
            p.drive = 0.30;
            p.velocityToAmp = 0.55;
            break;
        default:
            break;
    }
    return p;
}

// Family 1: Chromatic percussion (mallets/bells — FM with natural modulator decay)
SynthPatch chromaticPercPreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Sine;
    p.oscillatorB = Waveform::Sine;
    p.oscillatorMix = 0.25;
    p.fmEnabled = true;
    p.fmAmount = 0.30;
    p.fmRatio = 4.0;
    p.fmDecay = 0.60;
    p.velocityToFm = 0.40;
    p.cutoff = 0.80;
    p.resonance = 0.05;
    p.ampEnvelope.attack = 0.001;
    p.ampEnvelope.decay = 1.20;
    p.ampEnvelope.sustain = 0.0;
    p.ampEnvelope.release = 0.40;
    p.velocityToAmp = 0.85;
    p.reverbMix = 0.20;
    p.gain = 0.60;
    switch (program) {
        case 9: // Glockenspiel
            p.fmAmount = 0.45;
            p.fmRatio = 7.0;
            p.fmDecay = 0.25;
            p.ampEnvelope.decay = 0.90;
            p.highPass = 0.30;
            p.cutoff = 0.90;
            break;
        case 10: // Music Box
            p.fmAmount = 0.35;
            p.fmRatio = 5.0;
            p.fmDecay = 0.50;
            p.ampEnvelope.decay = 1.50;
            p.delayMix = 0.12;
            p.delayTime = 0.30;
            p.delayFeedback = 0.30;
            break;
        case 11: // Vibraphone — sine body, slow tremolo, long tail
            p.fmAmount = 0.14;
            p.fmRatio = 1.0;
            p.fmDecay = 0.20;
            p.ampEnvelope.decay = 2.80;
            p.tremoloDepth = 0.50;
            p.lfoRate = 5.0;
            p.reverbMix = 0.22;
            break;
        case 12: // Marimba — warm, woody, short knock
            p.oscillatorA = Waveform::Triangle;
            p.fmAmount = 0.18;
            p.fmRatio = 2.0;
            p.fmDecay = 0.06;
            p.ampEnvelope.decay = 0.70;
            p.lowPunch = 0.30;
            p.cutoff = 0.60;
            break;
        case 13: // Xylophone — bright, very short
            p.fmAmount = 0.30;
            p.fmRatio = 3.0;
            p.fmDecay = 0.05;
            p.ampEnvelope.decay = 0.45;
            p.highPass = 0.25;
            p.cutoff = 0.75;
            break;
        case 14: // Tubular Bells — long FM chime
            p.fmAmount = 0.50;
            p.fmRatio = 5.4;
            p.fmFeedback = 0.10;
            p.fmDecay = 1.40;
            p.ampEnvelope.decay = 3.50;
            p.ampEnvelope.release = 0.80;
            p.reverbMix = 0.30;
            p.reverbSize = 0.80;
            break;
        case 15: // Dulcimer
            p.oscillatorA = Waveform::Square;
            p.oscillatorMix = 0.30;
            p.fmAmount = 0.10;
            p.fmRatio = 2.0;
            p.fmDecay = 0.20;
            p.ampEnvelope.decay = 0.90;
            p.ampEnvelope.sustain = 0.05;
            p.cutoff = 0.70;
            p.stereoSpread = 0.30;
            break;
        default:
            break; // 8 Celesta = base
    }
    return p;
}

// Family 2: Organ
SynthPatch organPreset(int program) {
    SynthPatch p = basePreset();
    // Drawbar-style additive sines: 16' sub + 8' + 4' + 2 2/3'
    p.oscillatorA = Waveform::Sine;
    p.oscillatorB = Waveform::Sine;
    p.oscBDetuneCents = 1200.0;
    p.oscillatorC = Waveform::Sine;
    p.oscillatorCEnabled = true;
    p.oscCDetuneCents = 1902.0; // octave + fifth
    p.oscillatorCMix = 0.35;
    p.oscillatorMix = 0.45;
    p.subEnabled = true;
    p.subOscillator = 0.30;
    p.cutoff = 0.85;
    p.resonance = 0.0;
    p.filterKeytrack = 0.0;
    p.filterEnvelopeAmount = 0.0;
    p.ampEnvelope.attack = 0.004;
    p.ampEnvelope.decay = 0.05;
    p.ampEnvelope.sustain = 1.0;
    p.ampEnvelope.release = 0.06;
    p.velocityToAmp = 0.40;
    p.velocityToFilter = 0.0;
    p.click = 0.05;
    p.reverbMix = 0.12;
    p.gain = 0.60;
    switch (program) {
        case 16: // Drawbar — slow leslie
            p.tremoloDepth = 0.15;
            p.lfoRate = 6.5;
            enableJunoChorus(p, 0.20, 0.10);
            p.chorusRate = 0.8;
            break;
        case 17: // Percussive organ
            p.click = 0.18;
            p.ampEnvelope.decay = 0.40;
            p.ampEnvelope.sustain = 0.85;
            p.transientPitchSemitones = 7.0;
            p.transientPitchDecay = 0.10;
            break;
        case 18: // Rock organ — driven, fast leslie
            p.oscillatorA = Waveform::Square;
            p.oscillatorB = Waveform::Square;
            p.drive = 0.30;
            enableJunoChorus(p, 0.25, 0.10);
            p.chorusRate = 6.0;
            break;
        case 19: // Church organ — big hall
            p.ampEnvelope.attack = 0.03;
            p.ampEnvelope.release = 0.50;
            p.click = 0.0;
            p.reverbMix = 0.35;
            p.reverbSize = 0.85;
            p.reverbDecay = 0.80;
            break;
        case 20: // Reed organ
            p.oscillatorA = Waveform::Saw;
            p.oscillatorMix = 0.30;
            p.ampEnvelope.attack = 0.02;
            p.ampEnvelope.sustain = 0.90;
            p.noiseEnabled = true;
            p.noise = 0.03;
            break;
        case 21: // Accordion — double-reed detune + musette
            p.oscillatorA = Waveform::Square;
            p.oscillatorB = Waveform::Saw;
            p.detuneCents = 8.0;
            p.ampEnvelope.attack = 0.02;
            p.ampEnvelope.sustain = 0.95;
            p.vibratoCents = 8.0;
            p.lfoRate = 5.5;
            p.tremoloDepth = 0.10;
            break;
        case 22: // Harmonica
            p.oscillatorA = Waveform::Saw;
            p.oscillatorCEnabled = false;
            p.subEnabled = false;
            p.highPass = 0.15;
            p.vibratoCents = 10.0;
            p.ampEnvelope.attack = 0.015;
            p.ampEnvelope.sustain = 0.90;
            break;
        case 23: // Tango accordion (bandoneon)
            p.oscillatorA = Waveform::Square;
            p.detuneCents = 12.0;
            p.ampEnvelope.attack = 0.01;
            p.ampEnvelope.sustain = 1.0;
            break;
        default:
            break;
    }
    return p;
}

// Family 3: Guitar
SynthPatch guitarPreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Saw;
    p.oscillatorB = Waveform::Square;
    p.oscillatorMix = 0.35;
    // String partial layer: high partials die fast, the body rings on — the
    // plucked-string signature the old single-envelope design was missing.
    p.oscillatorC = Waveform::Sine;
    p.oscillatorCEnabled = true;
    p.oscillatorCMix = 0.22;
    p.oscCRatio = 3.0;
    p.oscCDecay = 0.16;
    p.oscCLevel = 0.85;
    p.keyTrackDecay = 0.35;
    p.velocityToDecay = 0.30;
    p.cutoff = 0.55;
    p.filterKeytrack = 0.60;
    p.filterEnvelopeAmount = 0.45;
    p.filterEnvelope.attack = 0.001;
    p.filterEnvelope.decay = 0.12;
    p.filterEnvelope.sustain = 0.10;
    p.ampEnvelope.attack = 0.002;
    p.ampEnvelope.decay = 0.90;
    p.ampEnvelope.sustain = 0.06;
    p.ampEnvelope.release = 0.12;
    p.velocityToAmp = 0.80;
    p.velocityToFilter = 0.40;
    p.reverbMix = 0.12;
    p.gain = 0.62;
    switch (program) {
        case 24: // Nylon — soft, round: low partial layer, gentle pick
            p.oscillatorA = Waveform::Triangle;
            p.oscCRatio = 2.0;
            p.oscCDecay = 0.25;
            p.oscillatorCMix = 0.18;
            p.cutoff = 0.50;
            p.ampEnvelope.decay = 0.80;
            p.lowPunch = 0.10;
            break;
        case 25: // Steel — bright with pick transient: high fast partials
            p.oscCRatio = 4.0;
            p.oscCDecay = 0.10;
            p.oscillatorCMix = 0.28;
            p.cutoff = 0.65;
            p.ampEnvelope.decay = 1.10;
            p.transientShape = 0.15;
            break;
        case 26: // Jazz — mellow hollow-body: minimal partials, warm
            p.oscCRatio = 2.0;
            p.oscCDecay = 0.20;
            p.oscillatorCMix = 0.12;
            p.cutoff = 0.42;
            p.ampEnvelope.decay = 0.70;
            p.analogWarmth = 0.70;
            break;
        case 27: // Clean — 80s chorus/delay clean strat
            p.oscillatorA = Waveform::Square;
            enableJunoChorus(p, 0.20, 0.15);
            p.ampEnvelope.decay = 0.80;
            p.delayMix = 0.10;
            p.delayTime = 0.28;
            p.delayFeedback = 0.25;
            break;
        case 28: // Muted — short funk chuck
            p.oscillatorCMix = 0.30; // the "chuck" is all partials
            p.oscCDecay = 0.05;
            p.ampEnvelope.decay = 0.18;
            p.ampEnvelope.sustain = 0.0;
            p.highPass = 0.25;
            p.cutoff = 0.50;
            p.filterEnvelopeAmount = 0.60;
            p.filterEnvelope.decay = 0.05;
            break;
        case 29: // Overdriven
            p.drive = 0.80;
            p.ampEnvelope.decay = 1.20;
            p.ampEnvelope.sustain = 0.50;
            p.cutoff = 0.60;
            p.delayMix = 0.06;
            break;
        case 30: // Distortion
            p.drive = 1.40;
            p.wavefold = 0.20;
            p.ampEnvelope.decay = 1.40;
            p.ampEnvelope.sustain = 0.60;
            p.cutoff = 0.70;
            p.highPass = 0.10;
            break;
        case 31: // Harmonics — flute-like octave ring
            p.oscillatorA = Waveform::Sine;
            p.oscCRatio = 2.0;  // octave ring (keeps the FM tuning coherent)
            p.oscCDecay = 0.0;
            p.oscillatorCMix = 0.15;
            p.fmEnabled = true;
            p.fmAmount = 0.20;
            p.fmRatio = 2.0;
            p.ampEnvelope.decay = 1.40;
            p.highPass = 0.40;
            p.cutoff = 0.85;
            break;
        default:
            break;
    }
    return p;
}

// Family 4: Bass — mono-mode classics (SH-101 / TB-303 / funk / fretless)
SynthPatch bassPreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Saw;
    p.oscillatorB = Waveform::Square;
    p.oscillatorMix = 0.30;
    p.subEnabled = true;
    p.subOscillator = 0.35;
    p.monoMode = true; // classic monosynth: no overlapping bass voices
    p.cutoff = 0.50;
    p.filterKeytrack = 0.40;
    p.filterEnvelopeAmount = 0.45;
    p.filterEnvelope.attack = 0.002;
    p.filterEnvelope.decay = 0.14;
    p.filterEnvelope.sustain = 0.22;
    p.ampEnvelope.attack = 0.002;
    p.ampEnvelope.decay = 0.28;
    p.ampEnvelope.sustain = 0.85;
    p.ampEnvelope.release = 0.06;
    p.transientShape = 0.12; // pick/finger attack
    p.lowPunch = 0.40;
    p.velocityToAmp = 0.70;
    p.velocityToFilter = 0.35;
    p.reverbMix = 0.05;
    p.gain = 0.70;
    switch (program) {
        case 32: // Acoustic bass — muted pluck with body
            p.oscillatorA = Waveform::Triangle;
            p.monoMode = false;
            p.cutoff = 0.45;
            p.ampEnvelope.decay = 0.50;
            p.ampEnvelope.sustain = 0.40;
            p.click = 0.06;
            p.lowPunch = 0.30;
            break;
        case 33: // Finger bass — round, warm, tight
            p.oscillatorA = Waveform::Triangle;
            p.oscillatorB = Waveform::Saw;
            p.oscillatorMix = 0.40;
            p.cutoff = 0.48;
            p.ampEnvelope.decay = 0.30;
            p.ampEnvelope.sustain = 0.75;
            p.transientShape = 0.10;
            p.portamentoTime = 0.02;
            p.portamentoLegato = true;
            break;
        case 34: // Pick bass — aggressive attack, fast env
            p.cutoff = 0.58;
            p.click = 0.14;
            p.transientShape = 0.28;
            p.filterEnvelopeAmount = 0.55;
            p.filterEnvelope.decay = 0.10;
            p.ampEnvelope.decay = 0.24;
            p.drive = 0.20;
            break;
        case 35: // Fretless — warm mwah with legato glide
            p.oscillatorA = Waveform::Sine;
            p.oscillatorB = Waveform::Triangle;
            p.oscillatorMix = 0.45;
            p.cutoff = 0.45;
            p.ampEnvelope.sustain = 0.90;
            p.portamentoTime = 0.06;
            p.portamentoLegato = true;
            p.vibratoCents = 9.0;
            p.subOscillator = 0.30;
            break;
        case 36: // Slap 1 — bright pop + snap
            p.cutoff = 0.68;
            p.filterEnvelopeAmount = 0.65;
            p.filterEnvelope.decay = 0.06;
            p.click = 0.22;
            p.transientShape = 0.35;
            p.transientNoise = 0.10;
            p.ampEnvelope.decay = 0.18;
            p.ampEnvelope.sustain = 0.50;
            p.drive = 0.25;
            break;
        case 37: // Slap 2 — brighter, more resonance
            p.cutoff = 0.76;
            p.resonance = 0.32;
            p.filterEnvelopeAmount = 0.65;
            p.filterEnvelope.decay = 0.06;
            p.click = 0.24;
            p.transientShape = 0.38;
            p.ampEnvelope.decay = 0.16;
            p.ampEnvelope.sustain = 0.45;
            p.drive = 0.30;
            break;
        case 38: // Synth Bass 1 — SH-101/Moog mono driver (EBM staple)
            p.oscillatorA = Waveform::Saw;
            p.detuneCents = 5.0;
            p.cutoff = 0.44;
            p.resonance = 0.35;
            p.filterDrive = 0.45;
            p.filterEnvelopeAmount = 0.55;
            p.filterEnvelope.decay = 0.16;
            p.drive = 0.35;
            p.subOscillator = 0.45;
            p.portamentoTime = 0.025;
            p.ampEnvelope.decay = 0.22;
            p.ampEnvelope.release = 0.05;
            p.lowPunch = 0.50;
            break;
        case 39: // Synth Bass 2 — TB-303 rubber acid
            p.oscillatorA = Waveform::Saw;
            p.oscillatorBEnabled = false;
            p.subEnabled = false;
            p.cutoff = 0.35;
            p.resonance = 0.62;
            p.filterEnvelopeAmount = 0.72;
            p.filterEnvelope.decay = 0.11;
            p.filterDrive = 0.50;
            p.velocityToFilter = 0.55;
            p.portamentoTime = 0.035;
            p.portamentoLegato = true;
            p.ampEnvelope.decay = 0.20;
            p.drive = 0.25;
            break;
        default:
            break;
    }
    return p;
}

// Family 5: Strings (solo orchestral)
SynthPatch soloStringsPreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Saw;
    p.oscillatorMix = 0.30;
    p.cutoff = 0.60;
    p.ampEnvelope.attack = 0.08;
    p.ampEnvelope.decay = 0.20;
    p.ampEnvelope.sustain = 0.90;
    p.ampEnvelope.release = 0.25;
    p.vibratoCents = 14.0;
    p.lfoRate = 5.5;
    enableJunoChorus(p, 0.15, 0.20);
    p.reverbMix = 0.20;
    p.gain = 0.60;
    switch (program) {
        case 40: // Violin
            p.cutoff = 0.65;
            break;
        case 41: // Viola
            p.cutoff = 0.55;
            p.analogWarmth = 0.65;
            break;
        case 42: // Cello
            p.cutoff = 0.45;
            p.lowPunch = 0.20;
            p.ampEnvelope.attack = 0.06;
            break;
        case 43: // Contrabass
            p.cutoff = 0.35;
            p.subEnabled = true;
            p.subOscillator = 0.30;
            break;
        case 44: // Tremolo strings
            p.tremoloDepth = 0.55;
            p.lfoRate = 7.0;
            p.chorusEnsemble = 0.40;
            break;
        case 45: // Pizzicato
            p.ampEnvelope.attack = 0.002;
            p.ampEnvelope.decay = 0.25;
            p.ampEnvelope.sustain = 0.0;
            p.ampEnvelope.release = 0.08;
            p.filterEnvelopeAmount = 0.50;
            p.filterEnvelope.decay = 0.08;
            p.click = 0.08;
            p.vibratoCents = 0.0;
            p.chorusEnabled = false;
            break;
        case 46: // Orchestral harp
            p.oscillatorA = Waveform::Triangle;
            p.fmEnabled = true;
            p.fmAmount = 0.12;
            p.fmRatio = 2.0;
            p.ampEnvelope.attack = 0.002;
            p.ampEnvelope.decay = 1.80;
            p.ampEnvelope.sustain = 0.0;
            p.ampEnvelope.release = 0.40;
            p.cutoff = 0.70;
            p.vibratoCents = 0.0;
            p.chorusEnabled = false;
            break;
        case 47: // Timpani
            p.oscillatorA = Waveform::Sine;
            p.oscillatorBEnabled = false;
            p.pitchEnvelopeSemitones = -4.0;
            p.pitchEnvelopeDecay = 0.05;
            p.noiseEnabled = true;
            p.noise = 0.15;
            p.ampEnvelope.attack = 0.003;
            p.ampEnvelope.decay = 0.90;
            p.ampEnvelope.sustain = 0.0;
            p.lowPunch = 0.50;
            p.cutoff = 0.40;
            p.vibratoCents = 0.0;
            p.chorusEnabled = false;
            break;
        default:
            break;
    }
    return p;
}

// Family 6: Ensemble — the Juno-60/106 zone
SynthPatch ensemblePreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Saw;
    p.oscillatorB = Waveform::Square;
    p.oscillatorMix = 0.25;
    p.subEnabled = true;
    p.subOscillator = 0.22;
    p.unisonVoices = 2;
    p.unisonDetuneCents = 9.0;
    p.stereoSpread = 0.45;
    p.cutoff = 0.48;
    p.analogColor = 0.32;
    p.ampEnvelope.attack = 0.12;
    p.ampEnvelope.decay = 0.25;
    p.ampEnvelope.sustain = 0.90;
    p.ampEnvelope.release = 0.35;
    enableJunoChorus(p, 0.40, 0.55);
    p.reverbMix = 0.24;
    p.reverbSize = 0.75;
    p.gain = 0.58;
    switch (program) {
        case 49: // String Ensemble 2 — slower, darker
            p.ampEnvelope.attack = 0.20;
            p.cutoff = 0.50;
            break;
        case 50: // SynthStrings 1 — PWM sheen
            p.unisonVoices = 3;
            p.cutoff = 0.52;
            p.pwmDepth = 0.30;
            p.lfoRate = 0.4;
            break;
        case 51: // SynthStrings 2 — lush slow
            p.ampEnvelope.attack = 0.25;
            p.reverbMix = 0.30;
            p.pwmDepth = 0.25;
            break;
        case 52: // Choir Aahs
            p.cutoff = 0.50;
            p.resonance = 0.25;
            p.ampEnvelope.attack = 0.18;
            p.vibratoCents = 10.0;
            p.lfoRate = 4.5;
            p.noiseEnabled = true;
            p.noise = 0.03;
            p.reverbMix = 0.30;
            p.reverbSize = 0.85;
            break;
        case 53: // Voice Oohs — darker, narrower
            p.cutoff = 0.42;
            p.resonance = 0.20;
            p.ampEnvelope.attack = 0.15;
            p.vibratoCents = 9.0;
            p.reverbMix = 0.28;
            break;
        case 54: // Synth Voice — bandpass formant sweep
            p.filterMode = 2;
            p.resonance = 0.40;
            p.ampEnvelope.attack = 0.10;
            p.lfoFilterDepth = 0.10;
            p.cutoff = 0.55;
            break;
        case 55: // Orchestra Hit — aggressive stacked stab
            p.unisonVoices = 4;
            p.unisonDetuneCents = 14.0;
            p.noiseEnabled = true;
            p.noise = 0.30;
            p.transientBurstCount = 3;
            p.transientShape = 0.40;
            p.pitchEnvelopeSemitones = -2.0;
            p.pitchEnvelopeDecay = 0.04;
            p.ampEnvelope.attack = 0.002;
            p.ampEnvelope.decay = 0.70;
            p.ampEnvelope.sustain = 0.0;
            p.cutoff = 0.75;
            p.drive = 0.40;
            p.chorusEnabled = false;
            break;
        default:
            break; // 48 String Ensemble 1 = base
    }
    return p;
}

// Family 7: Brass
SynthPatch brassPreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Saw;
    p.oscillatorMix = 0.30;
    // Bite layer: an octave-up partial that dies quickly after the attack,
    // leaving the sustained body — the brass "blat" transient.
    p.oscillatorC = Waveform::Square;
    p.oscillatorCEnabled = true;
    p.oscillatorCMix = 0.20;
    p.oscCRatio = 2.0;
    p.oscCDecay = 0.25;
    p.oscCLevel = 0.80;
    p.unisonVoices = 2;
    p.unisonDetuneCents = 8.0;
    p.cutoff = 0.48;
    // Brass swell: filter opens after the attack
    p.filterEnvelope.attack = 0.06;
    p.filterEnvelope.decay = 0.20;
    p.filterEnvelope.sustain = 0.70;
    p.filterEnvelopeAmount = 0.35;
    p.ampEnvelope.attack = 0.04;
    p.ampEnvelope.decay = 0.20;
    p.ampEnvelope.sustain = 0.85;
    p.ampEnvelope.release = 0.15;
    p.vibratoCents = 8.0;
    p.drive = 0.15;
    p.reverbMix = 0.16;
    p.gain = 0.60;
    switch (program) {
        case 56: // Trumpet — bright, fast bite
            p.cutoff = 0.55;
            p.oscCDecay = 0.20;
            p.oscillatorCMix = 0.24;
            p.ampEnvelope.attack = 0.02;
            break;
        case 57: // Trombone — darker, slower bite
            p.cutoff = 0.50;
            p.oscCDecay = 0.35;
            p.oscillatorCMix = 0.16;
            p.ampEnvelope.attack = 0.05;
            break;
        case 58: // Tuba
            p.cutoff = 0.38;
            p.oscCDecay = 0.40;
            p.oscillatorCMix = 0.14;
            p.subEnabled = true;
            p.subOscillator = 0.30;
            break;
        case 59: // Muted trumpet — buzzy, thin, no bite layer
            p.oscillatorCEnabled = false;
            p.oscillatorCMix = 0.0;
            p.highPass = 0.30;
            p.cutoff = 0.50;
            p.resonance = 0.30;
            p.filterEnvelopeAmount = 0.50;
            p.ampEnvelope.attack = 0.01;
            break;
        case 60: // French horn — soft swell, gentle bite
            p.oscCDecay = 0.45;
            p.oscillatorCMix = 0.12;
            p.ampEnvelope.attack = 0.08;
            p.cutoff = 0.45;
            p.reverbMix = 0.25;
            break;
        case 61: // Brass section
            p.unisonVoices = 3;
            enableJunoChorus(p, 0.15, 0.20);
            p.cutoff = 0.50;
            break;
        case 62: // Synth Brass 1 — OBX sync brass
            p.hardSyncEnabled = true;
            p.hardSync = 0.40;
            p.filterEnvelope.attack = 0.03;
            p.filterEnvelopeAmount = 0.50;
            p.drive = 0.20;
            break;
        case 63: // Synth Brass 2 — "Jump"-style stab brass
            p.oscillatorB = Waveform::Square;
            p.oscillatorMix = 0.45;
            enableJunoChorus(p, 0.30, 0.30);
            p.filterEnvelope.attack = 0.02;
            p.filterEnvelopeAmount = 0.45;
            p.cutoff = 0.52;
            p.unisonVoices = 3;
            break;
        default:
            break;
    }
    return p;
}

// Family 8: Reed
SynthPatch reedPreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Saw;
    p.oscillatorB = Waveform::Square;
    p.oscillatorMix = 0.45;
    p.cutoff = 0.50;
    p.ampEnvelope.attack = 0.03;
    p.ampEnvelope.decay = 0.15;
    p.ampEnvelope.sustain = 0.85;
    p.ampEnvelope.release = 0.12;
    p.vibratoCents = 10.0;
    p.lfoRate = 5.0;
    p.noiseEnabled = true;
    p.noise = 0.04;
    p.filterKeytrack = 0.50;
    p.reverbMix = 0.14;
    p.gain = 0.58;
    switch (program) {
        case 64: // Soprano sax — brighter
            p.cutoff = 0.56;
            p.vibratoCents = 12.0;
            break;
        case 66: // Tenor sax — breathy mid
            p.cutoff = 0.52;
            p.noise = 0.06;
            p.drive = 0.12;
            break;
        case 67: // Baritone sax
            p.cutoff = 0.42;
            p.subEnabled = true;
            p.subOscillator = 0.20;
            break;
        case 68: // Oboe — reedy, focused
            p.oscillatorA = Waveform::Square;
            p.oscillatorMix = 0.55;
            p.cutoff = 0.55;
            p.resonance = 0.15;
            p.vibratoCents = 9.0;
            p.noise = 0.02;
            break;
        case 69: // English horn — warmer oboe
            p.oscillatorA = Waveform::Square;
            p.cutoff = 0.48;
            p.vibratoCents = 8.0;
            p.noise = 0.02;
            break;
        case 70: // Bassoon
            p.cutoff = 0.40;
            p.subEnabled = true;
            p.subOscillator = 0.15;
            p.noise = 0.02;
            break;
        case 71: // Clarinet — hollow square (odd harmonics)
            p.oscillatorA = Waveform::Square;
            p.oscillatorB = Waveform::Sine;
            p.oscillatorMix = 0.30;
            p.cutoff = 0.50;
            p.vibratoCents = 6.0;
            p.noise = 0.02;
            break;
        default:
            break; // 65 Alto sax = base
    }
    return p;
}

// Family 9: Pipe (flutes/whistles)
SynthPatch pipePreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Sine;
    p.oscillatorB = Waveform::Triangle;
    p.oscillatorMix = 0.30;
    p.cutoff = 0.80;
    p.resonance = 0.05;
    p.ampEnvelope.attack = 0.03;
    p.ampEnvelope.decay = 0.15;
    p.ampEnvelope.sustain = 0.90;
    p.ampEnvelope.release = 0.15;
    p.vibratoCents = 8.0;
    p.lfoRate = 5.0;
    p.noiseEnabled = true;
    p.noise = 0.05;
    p.reverbMix = 0.20;
    p.gain = 0.60;
    switch (program) {
        case 72: // Piccolo
            p.noise = 0.08;
            p.highPass = 0.30;
            p.cutoff = 0.90;
            break;
        case 74: // Recorder — plain, direct
            p.noise = 0.04;
            p.ampEnvelope.attack = 0.03;
            p.vibratoCents = 5.0;
            break;
        case 75: // Pan flute — breathy chiff
            p.oscillatorB = Waveform::Square;
            p.oscillatorMix = 0.20;
            p.noise = 0.12;
            p.pitchEnvelopeSemitones = -0.5;
            p.pitchEnvelopeDecay = 0.02;
            p.cutoff = 0.70;
            break;
        case 76: // Blown bottle — dark, round
            p.noise = 0.15;
            p.noiseTone = 0.30;
            p.cutoff = 0.55;
            p.vibratoCents = 6.0;
            break;
        case 77: // Shakuhachi — very breathy, slow speak
            p.noise = 0.20;
            p.ampEnvelope.attack = 0.06;
            p.vibratoCents = 12.0;
            p.cutoff = 0.60;
            break;
        case 78: // Whistle — pure sine
            p.noiseEnabled = false;
            p.vibratoCents = 10.0;
            p.highPass = 0.35;
            break;
        case 79: // Ocarina — soft, sweet
            p.noise = 0.03;
            p.cutoff = 0.75;
            p.vibratoCents = 8.0;
            break;
        default:
            break; // 73 Flute = base
    }
    return p;
}

// Family 10: Synth Lead
SynthPatch synthLeadPreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Square;
    p.oscillatorB = Waveform::Saw;
    p.oscillatorMix = 0.30;
    p.unisonVoices = 2;
    p.unisonDetuneCents = 8.0;
    p.cutoff = 0.58;
    p.ampEnvelope.attack = 0.005;
    p.ampEnvelope.decay = 0.15;
    p.ampEnvelope.sustain = 0.90;
    p.ampEnvelope.release = 0.12;
    p.vibratoCents = 12.0;
    p.lfoRate = 5.5;
    p.drive = 0.15;
    p.portamentoTime = 0.035;
    p.reverbMix = 0.12;
    p.delayMix = 0.08;
    p.delayTime = 0.30;
    p.delayFeedback = 0.30;
    p.gain = 0.60;
    switch (program) {
        case 80: // Square lead — PWM solo voice
            p.pulseWidth = 0.40;
            p.oscAPulseWidth = 0.40;
            p.pwmDepth = 0.35;
            break;
        case 81: // Saw lead — sync screamer
            p.oscillatorA = Waveform::Saw;
            p.hardSyncEnabled = true;
            p.hardSync = 0.35;
            p.unisonDetuneCents = 10.0;
            break;
        case 82: // Calliope — octave square/sine stack
            p.oscillatorB = Waveform::Sine;
            p.oscBDetuneCents = 1200.0;
            p.oscillatorMix = 0.50;
            enableJunoChorus(p, 0.15, 0.10);
            break;
        case 83: // Chiff lead — breathy attack
            p.oscillatorA = Waveform::Saw;
            p.transientNoise = 0.30;
            p.cutoff = 0.62;
            break;
        case 84: // Charang — aggressive wavefolded saw
            p.oscillatorA = Waveform::Saw;
            p.drive = 0.35;
            p.wavefold = 0.10;
            break;
        case 85: // Voice lead — formant-ish
            p.resonance = 0.35;
            p.cutoff = 0.55;
            p.noiseEnabled = true;
            p.noise = 0.05;
            break;
        case 86: // Fifths lead
            p.oscillatorB = Waveform::Saw;
            p.oscBDetuneCents = 700.0;
            p.oscillatorMix = 0.50;
            break;
        case 87: // Bass + lead
            p.oscillatorA = Waveform::Saw;
            p.subEnabled = true;
            p.subOscillator = 0.50;
            p.cutoff = 0.60;
            break;
        default:
            break;
    }
    return p;
}

// Family 11: Synth Pad — lush 80s territory
SynthPatch synthPadPreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Saw;
    p.oscillatorB = Waveform::Square;
    p.oscillatorMix = 0.30;
    p.cutoff = 0.48;
    p.analogColor = 0.35;
    p.ampEnvelope.attack = 0.40;
    p.ampEnvelope.decay = 0.30;
    p.ampEnvelope.sustain = 0.85;
    p.ampEnvelope.release = 0.80;
    p.filterEnvelope.attack = 0.30;
    p.filterEnvelopeAmount = 0.20;
    p.stereoSpread = 0.50;
    enableJunoChorus(p, 0.45, 0.60);
    p.reverbMix = 0.32;
    p.reverbSize = 0.80;
    p.reverbDecay = 0.75;
    p.gain = 0.55;
    switch (program) {
        case 88: // New Age — shimmering PWM pad
            p.pwmDepth = 0.40;
            p.lfoRate = 0.30;
            p.lfoFilterDepth = 0.15;
            break;
        case 89: // Warm pad — dark, cozy
            p.cutoff = 0.42;
            p.analogWarmth = 0.80;
            p.ampEnvelope.attack = 0.30;
            break;
        case 90: // Polysynth — JX-8P style
            p.unisonVoices = 2;
            p.unisonDetuneCents = 8.0;
            p.cutoff = 0.52;
            p.ampEnvelope.attack = 0.20;
            p.pwmDepth = 0.25;
            break;
        case 91: // Choir pad
            p.ampEnvelope.attack = 0.50;
            p.resonance = 0.20;
            p.noiseEnabled = true;
            p.noise = 0.03;
            p.reverbMix = 0.40;
            p.vibratoCents = 8.0;
            break;
        case 92: // Bowed pad — glassy swell
            p.ampEnvelope.attack = 0.35;
            p.vibratoCents = 8.0;
            p.cutoff = 0.50;
            break;
        case 93: // Metallic pad — FM shimmer
            p.fmEnabled = true;
            p.fmAmount = 0.35;
            p.fmRatio = 3.1;
            p.ringEnabled = true;
            p.ringMod = 0.20;
            p.cutoff = 0.60;
            break;
        case 94: // Halo pad — huge shimmer hall
            p.oscillatorA = Waveform::Sine;
            p.oscillatorMix = 0.40;
            p.ampEnvelope.attack = 0.60;
            p.reverbMix = 0.45;
            p.reverbSize = 0.90;
            p.reverbShimmer = 0.30;
            break;
        case 95: // Sweep pad — slow resonant filter LFO
            p.lfoFilterDepth = 0.50;
            p.lfoRate = 0.15;
            p.resonance = 0.45;
            p.ampEnvelope.attack = 0.40;
            p.cutoff = 0.55;
            break;
        default:
            break;
    }
    return p;
}

// Family 12: Synth Effects
SynthPatch synthFxPreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Saw;
    p.cutoff = 0.55;
    p.ampEnvelope.attack = 0.30;
    p.ampEnvelope.decay = 0.40;
    p.ampEnvelope.sustain = 0.60;
    p.ampEnvelope.release = 0.60;
    p.reverbMix = 0.35;
    p.reverbSize = 0.85;
    p.gain = 0.50;
    switch (program) {
        case 96: // Rain — filtered noise wash
            p.noiseEnabled = true;
            p.noise = 0.50;
            p.noiseTone = 0.60;
            p.highPass = 0.30;
            p.ampEnvelope.attack = 0.80;
            p.reverbMix = 0.40;
            break;
        case 97: // Soundtrack — dark evolving pad
            p.cutoff = 0.45;
            p.lfoFilterDepth = 0.25;
            p.lfoRate = 0.20;
            p.ampEnvelope.attack = 0.50;
            p.delayMix = 0.20;
            p.delayFeedback = 0.40;
            break;
        case 98: // Crystal — FM bell pad with shimmer
            p.oscillatorA = Waveform::Sine;
            p.fmEnabled = true;
            p.fmAmount = 0.40;
            p.fmRatio = 6.0;
            p.reverbShimmer = 0.35;
            p.ampEnvelope.attack = 0.01;
            p.ampEnvelope.decay = 2.50;
            p.ampEnvelope.sustain = 0.20;
            break;
        case 99: // Atmosphere — airy noise-pad
            p.noiseEnabled = true;
            p.noise = 0.25;
            p.lfoFilterDepth = 0.30;
            p.lfoRate = 0.12;
            p.ampEnvelope.attack = 0.70;
            p.reverbMix = 0.45;
            p.reverbSize = 0.90;
            break;
        case 100: // Brightness — sparkling sine/saw
            p.oscillatorA = Waveform::Sine;
            p.oscillatorMix = 0.45;
            p.airBoost = 0.50;
            p.reverbShimmer = 0.25;
            p.ampEnvelope.attack = 0.30;
            break;
        case 101: // Goblins — dark squelch
            p.cutoff = 0.30;
            p.resonance = 0.50;
            p.lfoFilterDepth = 0.40;
            p.lfoRate = 0.30;
            p.wavefold = 0.20;
            break;
        case 102: // Echoes — pluck into big delay
            p.ampEnvelope.attack = 0.005;
            p.ampEnvelope.decay = 0.50;
            p.ampEnvelope.sustain = 0.10;
            p.delayMix = 0.40;
            p.delayTime = 0.45;
            p.delayFeedback = 0.55;
            p.delayStereo = 0.60;
            break;
        case 103: // Sci-fi — ring-mod sweep
            p.ringEnabled = true;
            p.ringMod = 0.50;
            p.vibratoCents = 50.0;
            p.lfoRate = 0.80;
            p.reverbMix = 0.40;
            break;
        default:
            break;
    }
    return p;
}

// Family 13: Ethnic / plucked
SynthPatch ethnicPreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Square;
    p.oscillatorB = Waveform::Saw;
    p.oscillatorMix = 0.40;
    p.cutoff = 0.65;
    p.filterEnvelopeAmount = 0.40;
    p.filterEnvelope.decay = 0.10;
    p.ampEnvelope.attack = 0.002;
    p.ampEnvelope.decay = 0.80;
    p.ampEnvelope.sustain = 0.05;
    p.ampEnvelope.release = 0.15;
    p.reverbMix = 0.14;
    p.gain = 0.60;
    switch (program) {
        case 104: // Sitar — buzzy comb resonance
            p.combMix = 0.30;
            p.combTime = 0.02;
            p.combFeedback = 0.40;
            p.ampEnvelope.decay = 1.20;
            p.resonance = 0.40;
            p.cutoff = 0.60;
            break;
        case 105: // Banjo — very short bright pluck
            p.ampEnvelope.decay = 0.30;
            p.ampEnvelope.sustain = 0.0;
            p.cutoff = 0.70;
            p.transientShape = 0.20;
            break;
        case 106: // Shamisen — sharp, percussive
            p.ampEnvelope.decay = 0.50;
            p.cutoff = 0.65;
            p.highPass = 0.10;
            p.click = 0.12;
            break;
        case 107: // Koto — round pluck
            p.oscillatorA = Waveform::Triangle;
            p.ampEnvelope.decay = 0.80;
            p.cutoff = 0.70;
            break;
        case 108: // Kalimba — FM thumb piano
            p.oscillatorA = Waveform::Sine;
            p.fmEnabled = true;
            p.fmAmount = 0.20;
            p.fmRatio = 3.0;
            p.fmDecay = 0.12;
            p.ampEnvelope.decay = 0.60;
            p.cutoff = 0.75;
            break;
        case 109: // Bagpipe — sustained reedy drone
            p.ampEnvelope.attack = 0.03;
            p.ampEnvelope.decay = 0.10;
            p.ampEnvelope.sustain = 1.0;
            p.vibratoCents = 6.0;
            p.cutoff = 0.55;
            break;
        case 110: // Fiddle — folk violin
            p.oscillatorA = Waveform::Saw;
            p.ampEnvelope.attack = 0.03;
            p.ampEnvelope.sustain = 0.85;
            p.vibratoCents = 10.0;
            enableJunoChorus(p, 0.10, 0.15);
            break;
        case 111: // Shanai — nasal double-reed
            p.oscillatorA = Waveform::Saw;
            p.resonance = 0.30;
            p.cutoff = 0.60;
            p.ampEnvelope.sustain = 0.90;
            p.vibratoCents = 8.0;
            break;
        default:
            break;
    }
    return p;
}

// Family 14: Melodic percussion
SynthPatch melodicPercPreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Sine;
    p.oscillatorB = Waveform::Triangle;
    p.oscillatorMix = 0.30;
    p.cutoff = 0.70;
    p.ampEnvelope.attack = 0.001;
    p.ampEnvelope.decay = 0.60;
    p.ampEnvelope.sustain = 0.0;
    p.ampEnvelope.release = 0.15;
    p.reverbMix = 0.14;
    p.gain = 0.62;
    switch (program) {
        case 112: // Tinkle bell
            p.fmEnabled = true;
            p.fmAmount = 0.50;
            p.fmRatio = 7.0;
            p.fmDecay = 0.30;
            p.ampEnvelope.decay = 1.00;
            p.highPass = 0.30;
            break;
        case 113: // Agogo — metallic ring
            p.oscillatorA = Waveform::Square;
            p.ringEnabled = true;
            p.ringMod = 0.30;
            p.ampEnvelope.decay = 0.40;
            p.highPass = 0.25;
            break;
        case 114: // Steel drums — FM pan with fast-decaying clang
            p.fmEnabled = true;
            p.fmAmount = 0.45;
            p.fmRatio = 3.9;
            p.fmDecay = 0.35;
            p.ampEnvelope.decay = 0.90;
            p.cutoff = 0.80;
            enableJunoChorus(p, 0.12, 0.10);
            break;
        case 115: // Woodblock — short knock
            p.oscillatorA = Waveform::Triangle;
            p.ampEnvelope.decay = 0.12;
            p.highPass = 0.35;
            p.click = 0.20;
            p.cutoff = 0.60;
            break;
        case 116: // Taiko — deep booming drum
            p.pitchEnvelopeSemitones = -7.0;
            p.pitchEnvelopeDecay = 0.06;
            p.noiseEnabled = true;
            p.noise = 0.10;
            p.ampEnvelope.decay = 0.70;
            p.lowPunch = 0.60;
            p.cutoff = 0.45;
            break;
        case 117: // Melodic tom
            p.pitchEnvelopeSemitones = -5.0;
            p.pitchEnvelopeDecay = 0.05;
            p.ampEnvelope.decay = 0.50;
            p.cutoff = 0.50;
            break;
        case 118: // Synth drum — 808 kick/tom territory
            p.pitchEnvelopeSemitones = -12.0;
            p.pitchEnvelopeDecay = 0.04;
            p.ampEnvelope.decay = 0.80;
            p.lowPunch = 0.70;
            p.click = 0.15;
            p.cutoff = 0.55;
            break;
        case 119: // Reverse cymbal — noise swell
            p.noiseEnabled = true;
            p.noise = 0.60;
            p.highPass = 0.40;
            p.ampEnvelope.attack = 0.50;
            p.ampEnvelope.sustain = 0.30;
            p.ampEnvelope.release = 0.10;
            break;
        default:
            break;
    }
    return p;
}

// Family 15: Sound effects
SynthPatch sfxPreset(int program) {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Sine;
    p.oscillatorBEnabled = false;
    p.noiseEnabled = true;
    p.noise = 0.50;
    p.cutoff = 0.60;
    p.ampEnvelope.attack = 0.01;
    p.ampEnvelope.decay = 0.30;
    p.ampEnvelope.sustain = 0.50;
    p.ampEnvelope.release = 0.20;
    p.reverbMix = 0.20;
    p.gain = 0.55;
    switch (program) {
        case 120: // Guitar fret noise
            p.noise = 0.50;
            p.ampEnvelope.decay = 0.10;
            p.ampEnvelope.sustain = 0.0;
            p.highPass = 0.40;
            break;
        case 121: // Breath noise
            p.noise = 0.60;
            p.noiseTone = 0.40;
            p.ampEnvelope.attack = 0.10;
            p.ampEnvelope.sustain = 0.80;
            p.cutoff = 0.40;
            break;
        case 122: // Seashore — slow surf noise
            p.noise = 0.80;
            p.noiseTone = 0.25;
            p.lfoFilterDepth = 0.50;
            p.lfoRate = 0.10;
            p.ampEnvelope.attack = 1.00;
            p.ampEnvelope.sustain = 1.0;
            p.reverbMix = 0.30;
            break;
        case 123: // Bird tweet — chirpy sine
            p.noiseEnabled = false;
            p.vibratoCents = 200.0;
            p.lfoRate = 8.0;
            p.pitchEnvelopeSemitones = 6.0;
            p.pitchEnvelopeDecay = 0.15;
            p.ampEnvelope.decay = 0.20;
            p.ampEnvelope.sustain = 0.30;
            break;
        case 124: // Telephone ring — dual-tone trill
            p.oscillatorB = Waveform::Square;
            p.oscillatorBEnabled = true;
            p.ringEnabled = true;
            p.ringMod = 0.40;
            p.noiseEnabled = false;
            p.tremoloDepth = 1.0;
            p.lfoRate = 8.0;
            p.ampEnvelope.sustain = 0.50;
            break;
        case 125: // Helicopter — chopping low noise
            p.noise = 0.70;
            p.cutoff = 0.30;
            p.tremoloDepth = 0.90;
            p.lfoRate = 12.0;
            p.ampEnvelope.sustain = 1.0;
            break;
        case 126: // Applause — crowd noise burst
            p.noise = 0.70;
            p.highPass = 0.35;
            p.ampEnvelope.attack = 0.30;
            p.ampEnvelope.sustain = 0.70;
            p.reverbMix = 0.35;
            break;
        case 127: // Gunshot — noise crack with body
            p.noise = 0.90;
            p.transientShape = 0.80;
            p.ampEnvelope.decay = 0.15;
            p.ampEnvelope.sustain = 0.0;
            p.lowPunch = 0.80;
            p.drive = 0.50;
            break;
        default:
            break;
    }
    return p;
}

} // namespace

SynthPatch gmPresetForProgram(int program, double averageMidi) {
    const int clampedProgram = std::clamp(program, 0, 127);
    const int family = clampedProgram / 8;
    SynthPatch patch;
    switch (family) {
        case 0: patch = pianoPreset(clampedProgram); break;
        case 1: patch = chromaticPercPreset(clampedProgram); break;
        case 2: patch = organPreset(clampedProgram); break;
        case 3: patch = guitarPreset(clampedProgram); break;
        case 4: patch = bassPreset(clampedProgram); break;
        case 5: patch = soloStringsPreset(clampedProgram); break;
        case 6: patch = ensemblePreset(clampedProgram); break;
        case 7: patch = brassPreset(clampedProgram); break;
        case 8: patch = reedPreset(clampedProgram); break;
        case 9: patch = pipePreset(clampedProgram); break;
        case 10: patch = synthLeadPreset(clampedProgram); break;
        case 11: patch = synthPadPreset(clampedProgram); break;
        case 12: patch = synthFxPreset(clampedProgram); break;
        case 13: patch = ethnicPreset(clampedProgram); break;
        case 14: patch = melodicPercPreset(clampedProgram); break;
        case 15: patch = sfxPreset(clampedProgram); break;
        default: patch = basePreset(); break;
    }
    patch.name = gmProgramName(clampedProgram);
    applyKeyScaling(patch, averageMidi);
    return patch;
}

// (Drum-class presets for GM percussion live at the end of this file; the
// importer splits channel-10 lanes per class and calls gmDrumClassPreset.)

namespace {

SynthPatch drumBase() {
    SynthPatch p = basePreset();
    p.oscillatorA = Waveform::Noise;
    p.oscillatorBEnabled = false;
    p.oscillatorCEnabled = false;
    // One-shot drum voice: no wobble, no release, no pad-like FX.
    p.vintageDrift = 0.04;
    p.voiceSlop = 0.04;
    p.wowFlutter = 0.0;
    p.analogColor = 0.10;
    p.ampEnvelope.attack = 0.001;
    p.ampEnvelope.decay = 0.25;
    p.ampEnvelope.sustain = 0.0;
    p.ampEnvelope.release = 0.04;
    p.filterEnvelopeAmount = 0.0;
    p.reverbMix = 0.05;
    p.reverbDecay = 0.40;
    p.velocityToAmp = 0.80;
    p.velocityCurve = 1;
    p.gain = 0.70;
    return p;
}

} // namespace

int drumClassForNote(int midiNote) {
    switch (midiNote) {
        case 35: case 36: return 0;                       // kick
        case 38: case 40: return 1;                       // snare
        case 39: return 2;                                // clap
        case 42: case 44: case 46: return 3;              // hats
        case 41: case 43: case 45: case 47: case 48: case 50: return 4; // toms
        case 49: case 51: case 52: case 55: case 57: case 59: return 5; // cymbals
        default: return 6;                                // other percussion
    }
}

const char* drumClassName(int drumClass) {
    static const std::array<const char*, 7> names {
        "Kick", "Snare", "Clap", "Hi-Hat", "Tom", "Cymbal", "Percussion"
    };
    return names[static_cast<std::size_t>(std::clamp(drumClass, 0, 6))];
}

SynthPatch gmDrumClassPreset(int drumClass) {
    SynthPatch p = drumBase();
    p.name = std::string("GM ") + drumClassName(drumClass);
    switch (std::clamp(drumClass, 0, 6)) {
        case 0: { // Kick — 909/808 hybrid: click snap, pitch-drop body, driven thump
            p.oscillatorA = Waveform::Sine;
            // Fast snap region (transient pitch) + slower 808-style body drop.
            p.transientPitchSemitones = 18.0;
            p.transientPitchDecay = 0.004;
            p.pitchEnvelopeSemitones = 36.0;
            p.pitchEnvelopeDecay = 0.055;
            p.click = 0.16;
            p.transientShape = 0.35;
            p.transientDecay = 0.008;
            p.subEnabled = true;
            p.subOscillator = 0.40;
            p.drive = 0.38;
            p.filterDrive = 0.30;
            p.cutoff = 0.62;
            p.lowPunch = 0.65;
            p.ampEnvelope.decay = 0.34;
            p.velocityToAttack = 0.35; // harder hits = snappier attack
            p.reverbMix = 0.03;
            p.gain = 0.88;
            break;
        }
        case 1: { // Snare — 909 body + gated 80s crack
            p.oscillatorA = Waveform::Triangle;
            p.oscillatorB = Waveform::Noise;
            p.oscillatorBEnabled = true;
            p.oscillatorMix = 0.62; // mostly noise crack
            p.noiseEnabled = true;
            p.noise = 0.55;
            p.noiseTone = 0.62;
            p.pitchEnvelopeSemitones = 7.0;
            p.pitchEnvelopeDecay = 0.03;
            p.transientNoise = 0.20;
            p.transientShape = 0.30;
            p.highPass = 0.10;
            p.cutoff = 0.78;
            p.filterDrive = 0.20;
            p.lowPunch = 0.35;
            p.ampEnvelope.decay = 0.16;
            // Short gated tail for the 80s snare feel
            p.reverbMix = 0.16;
            p.reverbDecay = 0.35;
            p.reverbSize = 0.55;
            p.gain = 0.86;
            break;
        }
        case 2: { // Clap — multi-burst band noise with short room
            p.noiseEnabled = true;
            p.noise = 0.72;
            p.noiseTone = 0.58;
            p.transientBurstCount = 4;
            p.transientBurstSpacing = 0.012;
            p.transientBurstDecay = 0.55;
            p.transientTone = 0.60;
            p.highPass = 0.25;
            p.cutoff = 0.78;
            p.ampEnvelope.decay = 0.22;
            p.ampEnvelope.release = 0.10;
            p.reverbMix = 0.14;
            p.reverbDecay = 0.38;
            p.gain = 0.74;
            break;
        }
        case 3: { // Hi-Hat — metallic 808 tick
            p.oscillatorA = Waveform::Square;
            p.oscillatorB = Waveform::Noise;
            p.oscillatorBEnabled = true;
            p.oscillatorMix = 0.68;
            p.noiseEnabled = true;
            p.noise = 0.85;
            p.noiseTone = 0.85;
            p.fmEnabled = true;
            p.fmAmount = 0.18;
            p.fmRatio = 5.2;
            p.fmDecay = 0.03; // metallic edge dies instantly
            p.highPass = 0.60;
            p.cutoff = 0.85;
            p.ampEnvelope.decay = 0.07;
            p.ampEnvelope.release = 0.02;
            p.reverbMix = 0.0;
            p.gain = 0.64;
            break;
        }
        case 4: { // Tom — falling sine body + comb shell resonance
            p.oscillatorA = Waveform::Sine;
            p.pitchEnvelopeSemitones = 15.0;
            p.pitchEnvelopeDecay = 0.07;
            p.combMix = 0.20;
            p.combTime = 0.045;
            p.combFeedback = 0.30;
            p.lowPunch = 0.45;
            p.drive = 0.15;
            p.cutoff = 0.55;
            p.ampEnvelope.decay = 0.30;
            p.ampEnvelope.release = 0.06;
            p.reverbMix = 0.08;
            p.gain = 0.78;
            break;
        }
        case 5: { // Cymbal — FM-metallic shimmer over long noise wash
            p.oscillatorA = Waveform::Square;
            p.noiseEnabled = true;
            p.noise = 0.70;
            p.noiseTone = 0.90;
            p.fmEnabled = true;
            p.fmAmount = 0.30;
            p.fmRatio = 5.6;
            p.fmDecay = 0.90; // metallic shimmer decays into the wash
            p.highPass = 0.50;
            p.cutoff = 0.82;
            p.ampEnvelope.decay = 0.55;
            p.ampEnvelope.release = 0.20;
            p.reverbMix = 0.12;
            p.gain = 0.58;
            break;
        }
        default: { // 6 Percussion — FM knock with natural mallet decay
            p.oscillatorA = Waveform::Sine;
            p.fmEnabled = true;
            p.fmAmount = 0.35;
            p.fmRatio = 3.0;
            p.fmDecay = 0.10;
            p.highPass = 0.30;
            p.cutoff = 0.70;
            p.ampEnvelope.decay = 0.14;
            p.gain = 0.62;
            break;
        }
    }
    return p;
}

const char* gmProgramName(int program) {
    static const std::array<const char*, 128> names = {
        "Acoustic Grand Piano", "Bright Acoustic Piano", "Electric Grand Piano", "Honky-tonk Piano",
        "Electric Piano 1", "Electric Piano 2", "Harpsichord", "Clavinet",
        "Celesta", "Glockenspiel", "Music Box", "Vibraphone",
        "Marimba", "Xylophone", "Tubular Bells", "Dulcimer",
        "Drawbar Organ", "Percussive Organ", "Rock Organ", "Church Organ",
        "Reed Organ", "Accordion", "Harmonica", "Tango Accordion",
        "Acoustic Guitar (nylon)", "Acoustic Guitar (steel)", "Electric Guitar (jazz)", "Electric Guitar (clean)",
        "Electric Guitar (muted)", "Overdriven Guitar", "Distortion Guitar", "Guitar Harmonics",
        "Acoustic Bass", "Electric Bass (finger)", "Electric Bass (pick)", "Fretless Bass",
        "Slap Bass 1", "Slap Bass 2", "Synth Bass 1", "Synth Bass 2",
        "Violin", "Viola", "Cello", "Contrabass",
        "Tremolo Strings", "Pizzicato Strings", "Orchestral Harp", "Timpani",
        "String Ensemble 1", "String Ensemble 2", "SynthStrings 1", "SynthStrings 2",
        "Choir Aahs", "Voice Oohs", "Synth Voice", "Orchestra Hit",
        "Trumpet", "Trombone", "Tuba", "Muted Trumpet",
        "French Horn", "Brass Section", "Synth Brass 1", "Synth Brass 2",
        "Soprano Sax", "Alto Sax", "Tenor Sax", "Baritone Sax",
        "Oboe", "English Horn", "Bassoon", "Clarinet",
        "Piccolo", "Flute", "Recorder", "Pan Flute",
        "Blown Bottle", "Shakuhachi", "Whistle", "Ocarina",
        "Lead 1 (square)", "Lead 2 (sawtooth)", "Lead 3 (calliope)", "Lead 4 (chiff)",
        "Lead 5 (charang)", "Lead 6 (voice)", "Lead 7 (fifths)", "Lead 8 (bass + lead)",
        "Pad 1 (new age)", "Pad 2 (warm)", "Pad 3 (polysynth)", "Pad 4 (choir)",
        "Pad 5 (bowed)", "Pad 6 (metallic)", "Pad 7 (halo)", "Pad 8 (sweep)",
        "FX 1 (rain)", "FX 2 (soundtrack)", "FX 3 (crystal)", "FX 4 (atmosphere)",
        "FX 5 (brightness)", "FX 6 (goblins)", "FX 7 (echoes)", "FX 8 (sci-fi)",
        "Sitar", "Banjo", "Shamisen", "Koto",
        "Kalimba", "Bagpipe", "Fiddle", "Shanai",
        "Tinkle Bell", "Agogo", "Steel Drums", "Woodblock",
        "Taiko Drum", "Melodic Tom", "Synth Drum", "Reverse Cymbal",
        "Guitar Fret Noise", "Breath Noise", "Seashore", "Bird Tweet",
        "Telephone Ring", "Helicopter", "Applause", "Gunshot"};
    return names[static_cast<std::size_t>(std::clamp(program, 0, 127))];
}

} // namespace arachno
