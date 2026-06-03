#include "ui/gui/SynthUiData.h"

#include <algorithm>
#include <cmath>
#include <cctype>

namespace arachno {

namespace {

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

} // namespace

const std::vector<SynthParamDef>& synthParamDefinitions() {
    static const std::vector<SynthParamDef> defs {
        {"osc_a_enabled", "OSC A ON", 0.0, 1.0, 1.0},
        {"osc_b_enabled", "OSC B ON", 0.0, 1.0, 1.0},
        {"osc_c_enabled", "OSC C ON", 0.0, 1.0, 1.0},
        {"osc_d_enabled", "OSC D ON", 0.0, 1.0, 1.0},
        {"oscillator_mix", "MIX", 0.0, 1.0, 0.02},
        {"oscillator_c_mix", "OSC C MIX", 0.0, 1.0, 0.02},
        {"oscillator_d_mix", "OSC D MIX", 0.0, 1.0, 0.02},
        {"osc_a_level", "OSC A LVL", 0.0, 1.0, 0.02},
        {"osc_b_level", "OSC B LVL", 0.0, 1.0, 0.02},
        {"osc_c_level", "OSC C LVL", 0.0, 1.0, 0.02},
        {"osc_d_level", "OSC D LVL", 0.0, 1.0, 0.02},
        {"detune_cents", "DETUNE", -48.0, 48.0, 0.5},
        {"detune_c_cents", "DETUNE C", -48.0, 48.0, 0.5},
        {"detune_d_cents", "DETUNE D", -48.0, 48.0, 0.5},
        {"osc_a_detune_cents", "A DTN", -48.0, 48.0, 0.5},
        {"osc_b_detune_cents", "B DTN", -48.0, 48.0, 0.5},
        {"osc_c_detune_cents", "C DTN", -48.0, 48.0, 0.5},
        {"osc_d_detune_cents", "D DTN", -48.0, 48.0, 0.5},
        {"pulse_width", "PULSE", 0.03, 0.97, 0.01},
        {"osc_a_pulse_width", "A PULSE", 0.03, 0.97, 0.01},
        {"osc_b_pulse_width", "B PULSE", 0.03, 0.97, 0.01},
        {"osc_c_pulse_width", "C PULSE", 0.03, 0.97, 0.01},
        {"osc_d_pulse_width", "D PULSE", 0.03, 0.97, 0.01},
        {"pwm_depth", "PWM", 0.0, 1.0, 0.02},
        {"osc_a_pwm_depth", "A PWM", 0.0, 1.0, 0.02},
        {"osc_b_pwm_depth", "B PWM", 0.0, 1.0, 0.02},
        {"osc_c_pwm_depth", "C PWM", 0.0, 1.0, 0.02},
        {"osc_d_pwm_depth", "D PWM", 0.0, 1.0, 0.02},
        {"unison_voices", "UNISON", 1.0, 8.0, 1.0},
        {"unison_detune_cents", "UNI DTN", 0.0, 40.0, 0.5},
        {"stereo_spread", "SPREAD", 0.0, 1.0, 0.02},
        {"sub_enabled", "SUB ON", 0.0, 1.0, 1.0},
        {"sub_oscillator", "SUB", 0.0, 1.0, 0.02},
        {"noise_enabled", "NOISE ON", 0.0, 1.0, 1.0},
        {"noise", "NOISE", 0.0, 1.0, 0.02},
        {"noise_tone", "NOI TONE", 0.0, 1.0, 0.02},
        {"pan", "PAN", -1.0, 1.0, 0.02},
        {"gain", "GAIN", 0.0, 1.0, 0.02},
        {"cutoff", "CUTOFF", 0.0, 1.0, 0.02},
        {"resonance", "RESO", 0.0, 1.0, 0.02},
        {"filter_mode", "F MODE", 0.0, 2.0, 1.0},
        {"filter_drive", "F DRIVE", 0.0, 1.0, 0.02},
        {"filter_keytrack", "F KEYTRK", 0.0, 1.0, 0.02},
        {"filter_envelope", "F-ENV", -1.0, 1.0, 0.02},
        {"lfo_filter_depth", "LFO->CUT", 0.0, 1.0, 0.02},
        {"lfo_pan_depth", "LFO->PAN", 0.0, 1.0, 0.02},
        {"filter_attack", "F A", 0.001, 4.0, 0.01},
        {"filter_decay", "F D", 0.001, 4.0, 0.01},
        {"filter_sustain", "F S", 0.0, 1.0, 0.02},
        {"filter_release", "F R", 0.001, 6.0, 0.02},
        {"drive", "DRIVE", 0.0, 1.0, 0.02},
        {"osc_a_drive", "A DRIVE", 0.0, 1.0, 0.02},
        {"osc_b_drive", "B DRIVE", 0.0, 1.0, 0.02},
        {"osc_c_drive", "C DRIVE", 0.0, 1.0, 0.02},
        {"osc_d_drive", "D DRIVE", 0.0, 1.0, 0.02},
        {"wavefold", "FOLD", 0.0, 1.0, 0.02},
        {"lfo_rate", "LFO", 0.05, 24.0, 0.1},
        {"vibrato_cents", "VIB", 0.0, 120.0, 1.0},
        {"tremolo_depth", "TREM", 0.0, 1.0, 0.02},
        {"pitch_envelope_semitones", "P ENV", -36.0, 36.0, 0.5},
        {"pitch_envelope_decay", "P DEC", 0.001, 2.0, 0.01},
        {"ring_enabled", "RING ON", 0.0, 1.0, 1.0},
        {"ring_mod", "RING", 0.0, 1.0, 0.02},
        {"fm_enabled", "FM ON", 0.0, 1.0, 1.0},
        {"fm_amount", "FM", 0.0, 1.0, 0.02},
        {"fm_ratio", "FM RAT", 0.1, 16.0, 0.1},
        {"fm_feedback", "FM FB", 0.0, 1.0, 0.02},
        {"fm_algorithm", "FM ALG", 0.0, 3.0, 1.0},
        {"hard_sync_enabled", "SYNC ON", 0.0, 1.0, 1.0},
        {"hard_sync", "SYNC", 0.0, 1.0, 0.02},
        {"chorus_enabled", "CHORUS ON", 0.0, 1.0, 1.0},
        {"chorus_mix", "CHORUS", 0.0, 1.0, 0.02},
        {"chorus_rate", "CH RATE", 0.05, 5.0, 0.05},
        {"chorus_depth", "CH DEP", 0.0, 1.0, 0.02},
        {"chorus_feedback", "CH FB", 0.0, 0.98, 0.02},
        {"chorus_delay", "CH DLY", 0.0, 1.0, 0.02},
        {"chorus_width", "CH WID", 0.0, 1.0, 0.02},
        {"chorus_ensemble", "CH ENS", 0.0, 1.0, 0.02},
        {"bit_crush_enabled", "CRUSH ON", 0.0, 1.0, 1.0},
        {"bit_crush", "CRUSH", 0.0, 1.0, 0.02},
        {"sample_rate_reduction", "SR RED", 0.0, 1.0, 0.02},
        {"comb_mix", "COMB MIX", 0.0, 1.0, 0.02},
        {"comb_time", "COMB T", 0.001, 0.5, 0.005},
        {"comb_feedback", "COMB FB", 0.0, 0.98, 0.02},
        {"delay_mix", "DLY MIX", 0.0, 1.0, 0.02},
        {"delay_time", "DLY TIME", 0.0, 1.0, 0.02},
        {"delay_feedback", "DLY FB", 0.0, 0.98, 0.02},
        {"delay_tone", "DLY TONE", 0.0, 1.0, 0.02},
        {"delay_stereo", "DLY STER", 0.0, 1.0, 0.02},
        {"delay_mod_depth", "DLY MOD", 0.0, 1.0, 0.02},
        {"delay_drive", "DLY DRV", 0.0, 1.0, 0.02},
        {"delay_ducking", "DLY DUCK", 0.0, 1.0, 0.02},
        {"reverb_mix", "REV MIX", 0.0, 1.0, 0.02},
        {"reverb_size", "REV SIZE", 0.0, 1.0, 0.02},
        {"reverb_damping", "REV DAMP", 0.0, 1.0, 0.02},
        {"reverb_pre_delay", "REV PRE", 0.0, 1.0, 0.02},
        {"reverb_diffusion", "REV DIFF", 0.0, 1.0, 0.02},
        {"reverb_width", "REV WID", 0.0, 1.0, 0.02},
        {"reverb_shimmer", "REV SHIM", 0.0, 1.0, 0.02},
        {"reverb_mod_depth", "REV MOD", 0.0, 1.0, 0.02},
        {"high_pass", "HI PASS", 0.0, 1.0, 0.02},
        {"click", "CLICK", 0.0, 1.0, 0.02},
        {"transient_shape", "TR SHAPE", 0.0, 1.0, 0.02},
        {"transient_noise", "TR NOISE", 0.0, 1.0, 0.02},
        {"transient_pitch_semitones", "TR PITCH", -36.0, 36.0, 0.5},
        {"transient_pitch_decay", "TR P DEC", 0.001, 0.25, 0.005},
        {"transient_burst_count", "TR BURST", 1.0, 12.0, 1.0},
        {"transient_burst_spacing", "TR SPACE", 0.0005, 0.05, 0.0005},
        {"transient_burst_decay", "TR B DEC", 0.0, 1.0, 0.02},
        {"transient_tone", "TR TONE", 0.0, 1.0, 0.02},
        {"transient_decay", "TR DEC", 0.001, 0.25, 0.005},
        {"analog_color", "ANALOG", 0.0, 1.0, 0.02},
        {"vintage_drift", "DRIFT", 0.0, 1.0, 0.02},
        {"wow_flutter", "FLUTTER", 0.0, 1.0, 0.02},
        {"tone_tilt", "TILT", -1.0, 1.0, 0.02},
        {"tape_color", "TAPE", 0.0, 1.0, 0.02},
        {"air_boost", "AIR", 0.0, 1.0, 0.02},
        {"low_punch", "PUNCH", 0.0, 1.0, 0.02},
        {"analog_warmth", "WARMTH", 0.0, 1.0, 0.02},
        {"voice_slop", "SLOP", 0.0, 1.0, 0.02},
        {"phase_scatter", "PHASE", 0.0, 1.0, 0.02},
        {"unison_warp", "UNI WARP", 0.0, 1.0, 0.02},
        {"unison_humanize", "UNI HUM", 0.0, 1.0, 0.02},
        {"fm_color", "FM COLOR", 0.0, 1.0, 0.02},
        {"fm_spread", "FM SPRD", 0.0, 1.0, 0.02},
        {"chorus_tone", "CH TONE", 0.0, 1.0, 0.02},
        {"chorus_jitter", "CH JIT", 0.0, 1.0, 0.02},
        {"chorus_saturation", "CH SAT", 0.0, 1.0, 0.02},
        {"delay_diffusion", "DLY DIFF", 0.0, 1.0, 0.02},
        {"delay_wow", "DLY WOW", 0.0, 1.0, 0.02},
        {"delay_crossfeed", "DLY XFD", 0.0, 1.0, 0.02},
        {"reverb_decay", "REV DEC", 0.0, 1.0, 0.02},
        {"reverb_early_mix", "REV ERLY", 0.0, 1.0, 0.02},
        {"reverb_tone", "REV TONE", 0.0, 1.0, 0.02},
        {"reverb_chorus", "REV CHR", 0.0, 1.0, 0.02},
        {"reverb_bloom", "REV BLM", 0.0, 1.0, 0.02},
        {"console_crosstalk", "XTALK", 0.0, 1.0, 0.02},
        {"stereo_depth", "ST DEPTH", 0.0, 1.0, 0.02},
        {"hifi_exciter", "EXCITER", 0.0, 1.0, 0.02},
        {"output_transformer", "TRANS", 0.0, 1.0, 0.02},
        {"output_soft_clip", "SOFTCLIP", 0.0, 1.0, 0.02},
        {"output_glue", "GLUE", 0.0, 1.0, 0.02},
        {"amp_attack", "A", 0.001, 4.0, 0.01},
        {"amp_decay", "D", 0.001, 4.0, 0.01},
        {"amp_sustain", "S", 0.0, 1.0, 0.02},
        {"amp_release", "R", 0.001, 6.0, 0.02},
        {"sustain_hold", "HOLD", 0.001, 10.0, 0.02},
    };
    return defs;
}

bool synthParamBelongsToPage(const std::string& name, int page) {
    const std::string n = lowerCopy(name);
    const bool osc = n.rfind("osc_", 0) == 0 || n.find("oscillator_") != std::string::npos;
    const bool mod = n.rfind("lfo_", 0) == 0
        || n.find("vibrato") != std::string::npos
        || n.find("tremolo") != std::string::npos
        || n == "cutoff"
        || n == "resonance"
        || n.find("filter_") != std::string::npos
        || n == "filter_mode"
        || n == "filter_drive"
        || n == "filter_keytrack";
    const bool fx = n.find("chorus") != std::string::npos
        || n.find("delay_") != std::string::npos
        || n.find("reverb_") != std::string::npos
        || n.find("ring") != std::string::npos
        || n.find("fm_") != std::string::npos
        || n.find("sync") != std::string::npos
        || n.find("drive") != std::string::npos
        || n.find("fold") != std::string::npos
        || n.find("crush") != std::string::npos
        || n.find("comb") != std::string::npos
        || n.find("high_pass") != std::string::npos
        || n.find("click") != std::string::npos
        || n.find("transient") != std::string::npos
        || n.find("analog") != std::string::npos
        || n.find("drift") != std::string::npos
        || n.find("flutter") != std::string::npos
        || n.find("tilt") != std::string::npos
        || n.find("tape") != std::string::npos
        || n.find("air") != std::string::npos
        || n.find("punch") != std::string::npos
        || n.find("warmth") != std::string::npos
        || n.find("slop") != std::string::npos
        || n.find("scatter") != std::string::npos
        || n.find("xtalk") != std::string::npos
        || n.find("crosstalk") != std::string::npos
        || n.find("stereo_depth") != std::string::npos
        || n.find("exciter") != std::string::npos
        || n.find("transformer") != std::string::npos
        || n.find("soft_clip") != std::string::npos
        || n.find("glue") != std::string::npos;

    if (page == 0) {
        return osc || n.find("detune") != std::string::npos || n == "pulse_width" || n == "pwm_depth"
            || n.find("sub") != std::string::npos || n.find("noise") != std::string::npos
            || n.find("unison") != std::string::npos || n == "stereo_spread" || n == "pan" || n == "gain";
    }
    if (page == 1) {
        return mod;
    }
    if (page == 2) {
        return fx;
    }
    return n.rfind("amp_", 0) == 0 || n.rfind("filter_", 0) == 0 || n.rfind("pitch_", 0) == 0;
}

double getSynthParameterValue(const SynthPatch& patch, const std::string& name) {
    if (name == "osc_a_enabled") return patch.oscillatorAEnabled ? 1.0 : 0.0;
    if (name == "osc_b_enabled") return patch.oscillatorBEnabled ? 1.0 : 0.0;
    if (name == "osc_c_enabled") return patch.oscillatorCEnabled ? 1.0 : 0.0;
    if (name == "osc_d_enabled") return patch.oscillatorDEnabled ? 1.0 : 0.0;
    if (name == "oscillator_mix") return patch.oscillatorMix;
    if (name == "oscillator_c_mix") return patch.oscillatorCMix;
    if (name == "oscillator_d_mix") return patch.oscillatorDMix;
    if (name == "osc_a_level") return patch.oscALevel;
    if (name == "osc_b_level") return patch.oscBLevel;
    if (name == "osc_c_level") return patch.oscCLevel;
    if (name == "osc_d_level") return patch.oscDLevel;
    if (name == "detune_cents") return patch.detuneCents;
    if (name == "detune_c_cents") return patch.detuneCCents;
    if (name == "detune_d_cents") return patch.detuneDCents;
    if (name == "osc_a_detune_cents") return patch.oscADetuneCents;
    if (name == "osc_b_detune_cents") return patch.oscBDetuneCents;
    if (name == "osc_c_detune_cents") return patch.oscCDetuneCents;
    if (name == "osc_d_detune_cents") return patch.oscDDetuneCents;
    if (name == "pulse_width") return patch.pulseWidth;
    if (name == "osc_a_pulse_width") return patch.oscAPulseWidth;
    if (name == "osc_b_pulse_width") return patch.oscBPulseWidth;
    if (name == "osc_c_pulse_width") return patch.oscCPulseWidth;
    if (name == "osc_d_pulse_width") return patch.oscDPulseWidth;
    if (name == "pwm_depth") return patch.pwmDepth;
    if (name == "osc_a_pwm_depth") return patch.oscAPwmDepth;
    if (name == "osc_b_pwm_depth") return patch.oscBPwmDepth;
    if (name == "osc_c_pwm_depth") return patch.oscCPwmDepth;
    if (name == "osc_d_pwm_depth") return patch.oscDPwmDepth;
    if (name == "unison_detune_cents") return patch.unisonDetuneCents;
    if (name == "sub_enabled") return patch.subEnabled ? 1.0 : 0.0;
    if (name == "sub_oscillator") return patch.subOscillator;
    if (name == "noise_enabled") return patch.noiseEnabled ? 1.0 : 0.0;
    if (name == "noise") return patch.noise;
    if (name == "noise_tone") return patch.noiseTone;
    if (name == "pan") return patch.pan;
    if (name == "cutoff") return patch.cutoff;
    if (name == "resonance") return patch.resonance;
    if (name == "filter_mode") return static_cast<double>(patch.filterMode);
    if (name == "filter_drive") return patch.filterDrive;
    if (name == "filter_keytrack") return patch.filterKeytrack;
    if (name == "filter_envelope") return patch.filterEnvelopeAmount;
    if (name == "lfo_filter_depth") return patch.lfoFilterDepth;
    if (name == "lfo_pan_depth") return patch.lfoPanDepth;
    if (name == "filter_attack") return patch.filterEnvelope.attack;
    if (name == "filter_decay") return patch.filterEnvelope.decay;
    if (name == "filter_sustain") return patch.filterEnvelope.sustain;
    if (name == "filter_release") return patch.filterEnvelope.release;
    if (name == "drive") return patch.drive;
    if (name == "osc_a_drive") return patch.oscADrive;
    if (name == "osc_b_drive") return patch.oscBDrive;
    if (name == "osc_c_drive") return patch.oscCDrive;
    if (name == "osc_d_drive") return patch.oscDDrive;
    if (name == "wavefold") return patch.wavefold;
    if (name == "lfo_rate") return patch.lfoRate;
    if (name == "vibrato_cents") return patch.vibratoCents;
    if (name == "tremolo_depth") return patch.tremoloDepth;
    if (name == "pitch_envelope_semitones") return patch.pitchEnvelopeSemitones;
    if (name == "pitch_envelope_decay") return patch.pitchEnvelopeDecay;
    if (name == "ring_enabled") return patch.ringEnabled ? 1.0 : 0.0;
    if (name == "ring_mod") return patch.ringMod;
    if (name == "fm_enabled") return patch.fmEnabled ? 1.0 : 0.0;
    if (name == "fm_amount") return patch.fmAmount;
    if (name == "fm_ratio") return patch.fmRatio;
    if (name == "fm_feedback") return patch.fmFeedback;
    if (name == "fm_algorithm") return static_cast<double>(patch.fmAlgorithm);
    if (name == "hard_sync_enabled") return patch.hardSyncEnabled ? 1.0 : 0.0;
    if (name == "hard_sync") return patch.hardSync;
    if (name == "chorus_enabled") return patch.chorusEnabled ? 1.0 : 0.0;
    if (name == "chorus_mix") return patch.chorusMix;
    if (name == "chorus_rate") return patch.chorusRate;
    if (name == "chorus_depth") return patch.chorusDepth;
    if (name == "chorus_feedback") return patch.chorusFeedback;
    if (name == "chorus_delay") return patch.chorusDelay;
    if (name == "chorus_width") return patch.chorusWidth;
    if (name == "chorus_ensemble") return patch.chorusEnsemble;
    if (name == "bit_crush_enabled") return patch.bitCrushEnabled ? 1.0 : 0.0;
    if (name == "bit_crush") return patch.bitCrush;
    if (name == "sample_rate_reduction") return patch.sampleRateReduction;
    if (name == "comb_mix") return patch.combMix;
    if (name == "comb_time") return patch.combTime;
    if (name == "comb_feedback") return patch.combFeedback;
    if (name == "delay_mix") return patch.delayMix;
    if (name == "delay_time") return patch.delayTime;
    if (name == "delay_feedback") return patch.delayFeedback;
    if (name == "delay_tone") return patch.delayTone;
    if (name == "delay_stereo") return patch.delayStereo;
    if (name == "delay_mod_depth") return patch.delayModDepth;
    if (name == "delay_drive") return patch.delayDrive;
    if (name == "delay_ducking") return patch.delayDucking;
    if (name == "reverb_mix") return patch.reverbMix;
    if (name == "reverb_size") return patch.reverbSize;
    if (name == "reverb_damping") return patch.reverbDamping;
    if (name == "reverb_pre_delay") return patch.reverbPreDelay;
    if (name == "reverb_diffusion") return patch.reverbDiffusion;
    if (name == "reverb_width") return patch.reverbWidth;
    if (name == "reverb_shimmer") return patch.reverbShimmer;
    if (name == "reverb_mod_depth") return patch.reverbModDepth;
    if (name == "high_pass") return patch.highPass;
    if (name == "click") return patch.click;
    if (name == "transient_shape") return patch.transientShape;
    if (name == "transient_noise") return patch.transientNoise;
    if (name == "transient_pitch_semitones") return patch.transientPitchSemitones;
    if (name == "transient_pitch_decay") return patch.transientPitchDecay;
    if (name == "transient_burst_count") return static_cast<double>(patch.transientBurstCount);
    if (name == "transient_burst_spacing") return patch.transientBurstSpacing;
    if (name == "transient_burst_decay") return patch.transientBurstDecay;
    if (name == "transient_tone") return patch.transientTone;
    if (name == "transient_decay") return patch.transientDecay;
    if (name == "analog_color") return patch.analogColor;
    if (name == "vintage_drift") return patch.vintageDrift;
    if (name == "wow_flutter") return patch.wowFlutter;
    if (name == "tone_tilt") return patch.toneTilt;
    if (name == "tape_color") return patch.tapeColor;
    if (name == "air_boost") return patch.airBoost;
    if (name == "low_punch") return patch.lowPunch;
    if (name == "analog_warmth") return patch.analogWarmth;
    if (name == "voice_slop") return patch.voiceSlop;
    if (name == "phase_scatter") return patch.phaseScatter;
    if (name == "unison_warp") return patch.unisonWarp;
    if (name == "unison_humanize") return patch.unisonHumanize;
    if (name == "fm_color") return patch.fmColor;
    if (name == "fm_spread") return patch.fmSpread;
    if (name == "chorus_tone") return patch.chorusTone;
    if (name == "chorus_jitter") return patch.chorusJitter;
    if (name == "chorus_saturation") return patch.chorusSaturation;
    if (name == "delay_diffusion") return patch.delayDiffusion;
    if (name == "delay_wow") return patch.delayWow;
    if (name == "delay_crossfeed") return patch.delayCrossfeed;
    if (name == "reverb_decay") return patch.reverbDecay;
    if (name == "reverb_early_mix") return patch.reverbEarlyMix;
    if (name == "reverb_tone") return patch.reverbTone;
    if (name == "reverb_chorus") return patch.reverbChorus;
    if (name == "reverb_bloom") return patch.reverbBloom;
    if (name == "console_crosstalk") return patch.consoleCrosstalk;
    if (name == "stereo_depth") return patch.stereoDepth;
    if (name == "hifi_exciter") return patch.hifiExciter;
    if (name == "output_transformer") return patch.outputTransformer;
    if (name == "output_soft_clip") return patch.outputSoftClip;
    if (name == "output_glue") return patch.outputGlue;
    if (name == "unison_voices") return static_cast<double>(patch.unisonVoices);
    if (name == "stereo_spread") return patch.stereoSpread;
    if (name == "gain") return patch.gain;
    if (name == "amp_attack") return patch.ampEnvelope.attack;
    if (name == "amp_decay") return patch.ampEnvelope.decay;
    if (name == "amp_sustain") return patch.ampEnvelope.sustain;
    if (name == "amp_release") return patch.ampEnvelope.release;
    if (name == "sustain_hold") return patch.ampEnvelope.release;
    return 0.0;
}

std::string oscTargetName(int target) {
    switch (target) {
        case 0: return "a";
        case 1: return "b";
        case 2: return "c";
        case 3: return "d";
        default: return "all";
    }
}

std::string mapSynthParameterToOscTarget(const std::string& base, int target) {
    if (target < 0 || target > 3) {
        return base;
    }
    const std::string osc = oscTargetName(target);
    if (base == "oscillator_mix" || base == "oscillator_c_mix" || base == "oscillator_d_mix") {
        return "osc_" + osc + "_level";
    }
    if (base == "detune_cents" || base == "detune_c_cents" || base == "detune_d_cents") {
        return "osc_" + osc + "_detune_cents";
    }
    if (base == "pulse_width") {
        return "osc_" + osc + "_pulse_width";
    }
    if (base == "pwm_depth") {
        return "osc_" + osc + "_pwm_depth";
    }
    if (base == "drive") {
        return "osc_" + osc + "_drive";
    }
    return base;
}

std::string synthParamBaseName(const std::string& name) {
    if (name == "osc_a_level" || name == "osc_b_level" || name == "osc_c_level" || name == "osc_d_level") {
        return "gain";
    }
    if (name == "osc_a_detune_cents" || name == "osc_b_detune_cents" || name == "osc_c_detune_cents" || name == "osc_d_detune_cents") {
        return "detune_cents";
    }
    if (name == "osc_a_pulse_width" || name == "osc_b_pulse_width" || name == "osc_c_pulse_width" || name == "osc_d_pulse_width") {
        return "pulse_width";
    }
    if (name == "osc_a_pwm_depth" || name == "osc_b_pwm_depth" || name == "osc_c_pwm_depth" || name == "osc_d_pwm_depth") {
        return "pwm_depth";
    }
    if (name == "osc_a_drive" || name == "osc_b_drive" || name == "osc_c_drive" || name == "osc_d_drive") {
        return "drive";
    }
    return name;
}

const SynthParamDef* findSynthParamDef(const std::string& name) {
    const std::string base = synthParamBaseName(name);
    for (const SynthParamDef& def : synthParamDefinitions()) {
        if (def.name == name || def.name == base) {
            return &def;
        }
    }
    return nullptr;
}

double clampQuantizedSynthParamValue(const SynthParamDef& def, double value) {
    double clamped = std::clamp(value, def.minimum, def.maximum);
    if (def.step > 0.0 && std::isfinite(def.step)) {
        const double slots = std::round((clamped - def.minimum) / def.step);
        clamped = def.minimum + (slots * def.step);
        clamped = std::clamp(clamped, def.minimum, def.maximum);
    }
    return clamped;
}

std::string synthParamTooltipText(const std::string& name) {
    const std::string base = synthParamBaseName(name);
    if (base == "osc_a_enabled" || base == "osc_b_enabled" || base == "osc_c_enabled" || base == "osc_d_enabled") {
        return "Enable or disable this oscillator voice.";
    }
    if (base == "gain") return "Oscillator output level before master gain.";
    if (base == "detune_cents") return "Fine pitch offset in cents for width, thickness, and drift.";
    if (base == "pulse_width") return "Square duty shape. Lower = nasal, higher = hollow.";
    if (base == "pwm_depth") return "Adds LFO pulse movement for animated digital tone.";
    if (base == "drive") return "Saturation amount. Adds harmonics and bite.";
    if (base == "cutoff") return "Low-pass filter cutoff frequency.";
    if (base == "resonance") return "Filter resonance around cutoff for sharper tone.";
    if (base == "filter_mode") return "Filter model: Ladder, 4-pole ladder, or MS bite.";
    if (base == "filter_drive") return "Pre-filter drive for analog-style push.";
    if (base == "filter_keytrack") return "Makes cutoff follow played note pitch.";
    if (base == "filter_envelope") return "Filter envelope amount applied to cutoff.";
    if (base == "lfo_rate") return "LFO speed used by modulation destinations.";
    if (base == "lfo_filter_depth") return "LFO modulation depth to filter cutoff.";
    if (base == "lfo_pan_depth") return "LFO modulation depth to stereo pan.";
    if (base == "vibrato_cents") return "Pitch modulation depth in cents.";
    if (base == "tremolo_depth") return "Amplitude modulation depth.";
    if (base == "fm_amount") return "Frequency modulation depth.";
    if (base == "fm_ratio") return "FM modulator frequency ratio.";
    if (base == "fm_feedback") return "FM feedback for harsher metallic character.";
    if (base == "fm_algorithm") return "FM routing topology for different timbres.";
    if (base == "ring_mod") return "Ring modulation depth.";
    if (base == "hard_sync") return "Hard-sync intensity between oscillators.";
    if (base == "chorus_mix") return "Chorus wet/dry balance.";
    if (base == "chorus_rate") return "Chorus modulation speed.";
    if (base == "chorus_depth") return "Chorus modulation depth.";
    if (base == "chorus_feedback") return "Chorus recirculation for denser ensemble tails.";
    if (base == "chorus_delay") return "Base chorus delay time. Higher values feel wider and softer.";
    if (base == "chorus_width") return "Stereo phase spread between left/right chorus movement.";
    if (base == "chorus_ensemble") return "Adds extra chorus voices for rich 80s/90s ensemble width.";
    if (base == "bit_crush") return "Bit depth reduction for lo-fi character.";
    if (base == "sample_rate_reduction") return "Sample-rate reduction amount.";
    if (base == "comb_mix") return "Comb filter wet/dry mix.";
    if (base == "comb_time") return "Comb delay time.";
    if (base == "comb_feedback") return "Comb feedback for resonant tails.";
    if (base == "delay_mix") return "Stereo delay wet/dry amount.";
    if (base == "delay_time") return "Stereo delay time from slapback to echo.";
    if (base == "delay_feedback") return "Delay feedback for repeat length and density.";
    if (base == "delay_tone") return "Delay damping tone. Lower is darker, higher is brighter.";
    if (base == "delay_stereo") return "Ping-pong and stereo offset amount in delay repeats.";
    if (base == "delay_mod_depth") return "Tape-style modulation depth on delay time.";
    if (base == "delay_drive") return "Saturation in delay feedback path for analog echo thickness.";
    if (base == "delay_ducking") return "Ducks delay repeats under the dry signal for mix clarity.";
    if (base == "reverb_mix") return "Hall-space blend amount.";
    if (base == "reverb_size") return "Virtual room size and diffusion span.";
    if (base == "reverb_damping") return "High-frequency damping in the reverb tail.";
    if (base == "reverb_pre_delay") return "Pre-delay before the reverb bloom starts.";
    if (base == "reverb_diffusion") return "Controls reverb density and smoothness of late reflections.";
    if (base == "reverb_width") return "Stereo width of reverb tail.";
    if (base == "reverb_shimmer") return "Adds shimmering high overtone glow to the reverb tail.";
    if (base == "reverb_mod_depth") return "Subtle motion in reverb taps for expensive hardware movement.";
    if (base == "vintage_drift") return "Analog oscillator drift intensity and voice wander.";
    if (base == "wow_flutter") return "Slow tape-like wow/flutter pitch modulation.";
    if (base == "tape_color") return "Tape-like glue saturation on the voice output stage.";
    if (base == "air_boost") return "Adds polished top-end presence and sheen.";
    if (base == "low_punch") return "Adds controlled low-end body and punch.";
    if (base == "analog_warmth") return "Adds low-order analog harmonics and rounded transistor warmth.";
    if (base == "voice_slop") return "Per-voice oscillator slop for expensive vintage polysynth movement.";
    if (base == "phase_scatter") return "Randomizes oscillator phase and stereo decorrelation for thicker starts.";
    if (base == "unison_warp") return "Curves unison detune spacing for JP/Juno-style edge emphasis.";
    if (base == "unison_humanize") return "Adds subtle per-voice tuning variance for expensive analog behavior.";
    if (base == "fm_color") return "FM waveshaping color from smooth sine to bright folded metal.";
    if (base == "fm_spread") return "Offsets FM ratios across unison voices for wider harmonics.";
    if (base == "chorus_tone") return "Brightness control for chorus wet signal.";
    if (base == "chorus_jitter") return "BBD-style clock jitter for vintage chorus motion.";
    if (base == "chorus_saturation") return "Saturates chorus returns for thick premium ensemble tone.";
    if (base == "delay_diffusion") return "Smears delay repeats with analog BBD-style diffusion.";
    if (base == "delay_wow") return "Independent wow/flutter character in delay repeats.";
    if (base == "delay_crossfeed") return "Stereo cross-feedback amount between delay channels.";
    if (base == "reverb_decay") return "Extends reverb tail length and sustain.";
    if (base == "reverb_early_mix") return "Blends in early reflections for premium room definition.";
    if (base == "reverb_tone") return "Reverb spectral tone from dark velvety to bright glossy.";
    if (base == "reverb_chorus") return "Adds chorused modulation to the reverb tail.";
    if (base == "reverb_bloom") return "Expands low-mid bloom and tail bloom in the reverb body.";
    if (base == "console_crosstalk") return "Subtle channel bleed for analog console stereo cohesion.";
    if (base == "stereo_depth") return "Pushes stereo side depth while keeping mono center stable.";
    if (base == "hifi_exciter") return "High-fidelity harmonic exciter for polished top-end detail.";
    if (base == "output_transformer") return "Transformer-like output stage color and low-mid heft.";
    if (base == "output_soft_clip") return "Premium bus soft clipping before the final output limiter.";
    if (base == "output_glue") return "Per-voice glue saturation/compression for cohesive mix placement.";
    if (base == "pan") return "Stereo placement of the voice.";
    if (base == "amp_attack") return "Volume envelope attack time.";
    if (base == "amp_decay") return "Volume envelope decay time.";
    if (base == "amp_sustain") return "Volume envelope sustain level.";
    if (base == "amp_release") return "Volume envelope release time.";
    if (base == "sustain_hold") return "Sustain hold tail in seconds. Higher values keep notes ringing longer.";
    if (base == "filter_attack") return "Filter envelope attack time.";
    if (base == "filter_decay") return "Filter envelope decay time.";
    if (base == "filter_sustain") return "Filter envelope sustain level.";
    if (base == "filter_release") return "Filter envelope release time.";
    return "Tweaks this synth parameter. Use Target OSC to edit A/B/C/D individually.";
}

} // namespace arachno
