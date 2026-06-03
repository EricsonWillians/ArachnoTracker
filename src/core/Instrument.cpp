#include "Instrument.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace arachno {

namespace {
std::string normalizeParameterName(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
        if (ch == '-') {
            return '_';
        }
        return static_cast<char>(std::tolower(ch));
    });
    return name;
}
} // namespace

bool setSynthPatchParameter(SynthPatch& patch, const std::string& parameter, double value) {
    const std::string name = normalizeParameterName(parameter);
    auto asEnabled = [](double raw) {
        return raw >= 0.5;
    };

    if (name == "osc_a_enabled" || name == "oscillator_a_enabled") {
        patch.oscillatorAEnabled = asEnabled(value);
    } else if (name == "osc_b_enabled" || name == "oscillator_b_enabled") {
        patch.oscillatorBEnabled = asEnabled(value);
    } else if (name == "osc_c_enabled" || name == "oscillator_c_enabled") {
        patch.oscillatorCEnabled = asEnabled(value);
    } else if (name == "osc_d_enabled" || name == "oscillator_d_enabled") {
        patch.oscillatorDEnabled = asEnabled(value);
    } else if (name == "mix" || name == "osc_mix" || name == "oscillator_mix") {
        patch.oscillatorMix = value;
    } else if (name == "osc_c_mix" || name == "oscillator_c_mix") {
        patch.oscillatorCMix = value;
    } else if (name == "osc_d_mix" || name == "oscillator_d_mix") {
        patch.oscillatorDMix = value;
    } else if (name == "osc_a_level" || name == "oscillator_a_level") {
        patch.oscALevel = value;
    } else if (name == "osc_b_level" || name == "oscillator_b_level") {
        patch.oscBLevel = value;
    } else if (name == "osc_c_level" || name == "oscillator_c_level") {
        patch.oscCLevel = value;
    } else if (name == "osc_d_level" || name == "oscillator_d_level") {
        patch.oscDLevel = value;
    } else if (name == "detune" || name == "detune_cents") {
        patch.detuneCents = value;
    } else if (name == "detune_c" || name == "detune_c_cents") {
        patch.detuneCCents = value;
    } else if (name == "detune_d" || name == "detune_d_cents") {
        patch.detuneDCents = value;
    } else if (name == "osc_a_detune" || name == "osc_a_detune_cents" || name == "oscillator_a_detune_cents") {
        patch.oscADetuneCents = value;
    } else if (name == "osc_b_detune" || name == "osc_b_detune_cents" || name == "oscillator_b_detune_cents") {
        patch.oscBDetuneCents = value;
    } else if (name == "osc_c_detune" || name == "osc_c_detune_cents" || name == "oscillator_c_detune_cents") {
        patch.oscCDetuneCents = value;
    } else if (name == "osc_d_detune" || name == "osc_d_detune_cents" || name == "oscillator_d_detune_cents") {
        patch.oscDDetuneCents = value;
    } else if (name == "pulse" || name == "pulse_width") {
        patch.pulseWidth = value;
    } else if (name == "osc_a_pulse" || name == "osc_a_pulse_width" || name == "oscillator_a_pulse_width") {
        patch.oscAPulseWidth = value;
    } else if (name == "osc_b_pulse" || name == "osc_b_pulse_width" || name == "oscillator_b_pulse_width") {
        patch.oscBPulseWidth = value;
    } else if (name == "osc_c_pulse" || name == "osc_c_pulse_width" || name == "oscillator_c_pulse_width") {
        patch.oscCPulseWidth = value;
    } else if (name == "osc_d_pulse" || name == "osc_d_pulse_width" || name == "oscillator_d_pulse_width") {
        patch.oscDPulseWidth = value;
    } else if (name == "pwm" || name == "pwm_depth") {
        patch.pwmDepth = value;
    } else if (name == "osc_a_pwm" || name == "osc_a_pwm_depth" || name == "oscillator_a_pwm_depth") {
        patch.oscAPwmDepth = value;
    } else if (name == "osc_b_pwm" || name == "osc_b_pwm_depth" || name == "oscillator_b_pwm_depth") {
        patch.oscBPwmDepth = value;
    } else if (name == "osc_c_pwm" || name == "osc_c_pwm_depth" || name == "oscillator_c_pwm_depth") {
        patch.oscCPwmDepth = value;
    } else if (name == "osc_d_pwm" || name == "osc_d_pwm_depth" || name == "oscillator_d_pwm_depth") {
        patch.oscDPwmDepth = value;
    } else if (name == "fm_enabled") {
        patch.fmEnabled = asEnabled(value);
    } else if (name == "fm" || name == "fm_amount") {
        patch.fmAmount = value;
    } else if (name == "fm_ratio") {
        patch.fmRatio = value;
    } else if (name == "fm_feedback") {
        patch.fmFeedback = value;
    } else if (name == "fm_algorithm" || name == "fm_algo") {
        patch.fmAlgorithm = static_cast<int>(std::lround(value));
    } else if (name == "chorus_enabled") {
        patch.chorusEnabled = asEnabled(value);
    } else if (name == "chorus" || name == "chorus_mix") {
        patch.chorusMix = value;
    } else if (name == "chorus_rate") {
        patch.chorusRate = value;
    } else if (name == "chorus_depth") {
        patch.chorusDepth = value;
    } else if (name == "chorus_feedback" || name == "chorus_fb") {
        patch.chorusFeedback = value;
    } else if (name == "chorus_delay" || name == "chorus_pre_delay") {
        patch.chorusDelay = value;
    } else if (name == "chorus_width") {
        patch.chorusWidth = value;
    } else if (name == "chorus_ensemble" || name == "chorus_voice_spread") {
        patch.chorusEnsemble = value;
    } else if (name == "unison" || name == "unison_voices") {
        patch.unisonVoices = static_cast<int>(std::lround(value));
    } else if (name == "unison_detune" || name == "unison_detune_cents") {
        patch.unisonDetuneCents = value;
    } else if (name == "spread" || name == "stereo_spread") {
        patch.stereoSpread = value;
    } else if (name == "sub_enabled") {
        patch.subEnabled = asEnabled(value);
    } else if (name == "sub" || name == "sub_oscillator") {
        patch.subOscillator = value;
    } else if (name == "noise_enabled") {
        patch.noiseEnabled = asEnabled(value);
    } else if (name == "noise") {
        patch.noise = value;
    } else if (name == "noise_tone") {
        patch.noiseTone = value;
    } else if (name == "cutoff") {
        patch.cutoff = value;
    } else if (name == "resonance") {
        patch.resonance = value;
    } else if (name == "filter_mode") {
        patch.filterMode = static_cast<int>(std::lround(value));
    } else if (name == "filter_drive") {
        patch.filterDrive = value;
    } else if (name == "filter_keytrack") {
        patch.filterKeytrack = value;
    } else if (name == "filter_env" || name == "filter_envelope") {
        patch.filterEnvelopeAmount = value;
    } else if (name == "lfo_filter_depth") {
        patch.lfoFilterDepth = value;
    } else if (name == "lfo_pan_depth") {
        patch.lfoPanDepth = value;
    } else if (name == "pitch_env" || name == "pitch_envelope" || name == "pitch_envelope_semitones") {
        patch.pitchEnvelopeSemitones = value;
    } else if (name == "pitch_decay" || name == "pitch_envelope_decay") {
        patch.pitchEnvelopeDecay = value;
    } else if (name == "lfo_rate") {
        patch.lfoRate = value;
    } else if (name == "vibrato" || name == "vibrato_cents") {
        patch.vibratoCents = value;
    } else if (name == "tremolo" || name == "tremolo_depth") {
        patch.tremoloDepth = value;
    } else if (name == "ring_enabled") {
        patch.ringEnabled = asEnabled(value);
    } else if (name == "ring" || name == "ring_mod" || name == "ring_modulation") {
        patch.ringMod = value;
    } else if (name == "hard_sync_enabled") {
        patch.hardSyncEnabled = asEnabled(value);
    } else if (name == "sync" || name == "hard_sync") {
        patch.hardSync = value;
    } else if (name == "drive") {
        patch.drive = value;
    } else if (name == "osc_a_drive" || name == "oscillator_a_drive") {
        patch.oscADrive = value;
    } else if (name == "osc_b_drive" || name == "oscillator_b_drive") {
        patch.oscBDrive = value;
    } else if (name == "osc_c_drive" || name == "oscillator_c_drive") {
        patch.oscCDrive = value;
    } else if (name == "osc_d_drive" || name == "oscillator_d_drive") {
        patch.oscDDrive = value;
    } else if (name == "wavefold") {
        patch.wavefold = value;
    } else if (name == "bit_crush_enabled" || name == "bitcrush_enabled") {
        patch.bitCrushEnabled = asEnabled(value);
    } else if (name == "bitcrush" || name == "bit_crush") {
        patch.bitCrush = value;
    } else if (name == "sample_reduce" || name == "sample_rate_reduction") {
        patch.sampleRateReduction = value;
    } else if (name == "comb_mix") {
        patch.combMix = value;
    } else if (name == "comb_time") {
        patch.combTime = value;
    } else if (name == "comb_feedback") {
        patch.combFeedback = value;
    } else if (name == "delay_mix" || name == "delay") {
        patch.delayMix = value;
    } else if (name == "delay_time") {
        patch.delayTime = value;
    } else if (name == "delay_feedback" || name == "delay_fb") {
        patch.delayFeedback = value;
    } else if (name == "delay_tone" || name == "delay_damping") {
        patch.delayTone = value;
    } else if (name == "delay_stereo" || name == "delay_pingpong") {
        patch.delayStereo = value;
    } else if (name == "delay_mod_depth" || name == "delay_mod") {
        patch.delayModDepth = value;
    } else if (name == "delay_drive" || name == "delay_saturation") {
        patch.delayDrive = value;
    } else if (name == "delay_ducking" || name == "delay_duck") {
        patch.delayDucking = value;
    } else if (name == "reverb_mix" || name == "reverb") {
        patch.reverbMix = value;
    } else if (name == "reverb_size" || name == "reverb_room") {
        patch.reverbSize = value;
    } else if (name == "reverb_damping" || name == "reverb_damp") {
        patch.reverbDamping = value;
    } else if (name == "reverb_pre_delay" || name == "reverb_predelay") {
        patch.reverbPreDelay = value;
    } else if (name == "reverb_diffusion" || name == "reverb_diffuse") {
        patch.reverbDiffusion = value;
    } else if (name == "reverb_width" || name == "reverb_stereo_width") {
        patch.reverbWidth = value;
    } else if (name == "reverb_shimmer") {
        patch.reverbShimmer = value;
    } else if (name == "reverb_mod_depth" || name == "reverb_mod") {
        patch.reverbModDepth = value;
    } else if (name == "highpass" || name == "high_pass") {
        patch.highPass = value;
    } else if (name == "click" || name == "transient_click") {
        patch.click = value;
    } else if (name == "transient_shape") {
        patch.transientShape = value;
    } else if (name == "transient_noise") {
        patch.transientNoise = value;
    } else if (name == "transient_pitch" || name == "transient_pitch_semitones") {
        patch.transientPitchSemitones = value;
    } else if (name == "transient_pitch_decay") {
        patch.transientPitchDecay = value;
    } else if (name == "transient_burst_count" || name == "burst_count") {
        patch.transientBurstCount = static_cast<int>(std::lround(value));
    } else if (name == "transient_burst_spacing" || name == "burst_spacing") {
        patch.transientBurstSpacing = value;
    } else if (name == "transient_burst_decay" || name == "burst_decay") {
        patch.transientBurstDecay = value;
    } else if (name == "transient_tone") {
        patch.transientTone = value;
    } else if (name == "transient_decay") {
        patch.transientDecay = value;
    } else if (name == "analog_color" || name == "analog") {
        patch.analogColor = value;
    } else if (name == "vintage_drift" || name == "drift_amount") {
        patch.vintageDrift = value;
    } else if (name == "wow_flutter" || name == "flutter") {
        patch.wowFlutter = value;
    } else if (name == "tone_tilt" || name == "tilt") {
        patch.toneTilt = value;
    } else if (name == "tape_color" || name == "tape") {
        patch.tapeColor = value;
    } else if (name == "air_boost" || name == "air") {
        patch.airBoost = value;
    } else if (name == "low_punch" || name == "punch") {
        patch.lowPunch = value;
    } else if (name == "analog_warmth" || name == "warmth") {
        patch.analogWarmth = value;
    } else if (name == "voice_slop" || name == "slop") {
        patch.voiceSlop = value;
    } else if (name == "phase_scatter" || name == "phase_chaos") {
        patch.phaseScatter = value;
    } else if (name == "unison_warp" || name == "warp") {
        patch.unisonWarp = value;
    } else if (name == "unison_humanize" || name == "humanize") {
        patch.unisonHumanize = value;
    } else if (name == "fm_color" || name == "fm_tone") {
        patch.fmColor = value;
    } else if (name == "fm_spread") {
        patch.fmSpread = value;
    } else if (name == "chorus_tone") {
        patch.chorusTone = value;
    } else if (name == "chorus_jitter") {
        patch.chorusJitter = value;
    } else if (name == "chorus_saturation" || name == "chorus_sat") {
        patch.chorusSaturation = value;
    } else if (name == "delay_diffusion" || name == "delay_diffuse") {
        patch.delayDiffusion = value;
    } else if (name == "delay_wow") {
        patch.delayWow = value;
    } else if (name == "delay_crossfeed" || name == "delay_xfeed") {
        patch.delayCrossfeed = value;
    } else if (name == "reverb_decay" || name == "reverb_tail") {
        patch.reverbDecay = value;
    } else if (name == "reverb_early_mix" || name == "reverb_early") {
        patch.reverbEarlyMix = value;
    } else if (name == "reverb_tone") {
        patch.reverbTone = value;
    } else if (name == "reverb_chorus") {
        patch.reverbChorus = value;
    } else if (name == "reverb_bloom") {
        patch.reverbBloom = value;
    } else if (name == "console_crosstalk" || name == "crosstalk") {
        patch.consoleCrosstalk = value;
    } else if (name == "stereo_depth") {
        patch.stereoDepth = value;
    } else if (name == "hifi_exciter" || name == "exciter") {
        patch.hifiExciter = value;
    } else if (name == "output_transformer" || name == "transformer") {
        patch.outputTransformer = value;
    } else if (name == "output_soft_clip" || name == "soft_clip") {
        patch.outputSoftClip = value;
    } else if (name == "output_glue" || name == "glue") {
        patch.outputGlue = value;
    } else if (name == "gain") {
        patch.gain = value;
    } else if (name == "pan") {
        patch.pan = value;
    } else if (name == "attack" || name == "amp_attack") {
        patch.ampEnvelope.attack = value;
    } else if (name == "decay" || name == "amp_decay") {
        patch.ampEnvelope.decay = value;
    } else if (name == "sustain" || name == "amp_sustain") {
        patch.ampEnvelope.sustain = value;
    } else if (name == "release" || name == "amp_release") {
        patch.ampEnvelope.release = value;
    } else if (name == "hold" || name == "sustain_hold") {
        patch.ampEnvelope.release = value;
    } else if (name == "filter_attack") {
        patch.filterEnvelope.attack = value;
    } else if (name == "filter_decay") {
        patch.filterEnvelope.decay = value;
    } else if (name == "filter_sustain") {
        patch.filterEnvelope.sustain = value;
    } else if (name == "filter_release") {
        patch.filterEnvelope.release = value;
    } else {
        return false;
    }

    return true;
}

} // namespace arachno
