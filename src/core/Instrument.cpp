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

std::string lowerCopy(const std::string& value) {
    std::string lowered = value;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return lowered;
}

bool containsToken(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

bool looksLikeBassPatchByName(const std::string& name) {
    const std::string lowerName = lowerCopy(name);
    constexpr const char* kBassTokens[] = {
        "bass",
        "sub",
        "drone",
        "reese",
        "fmbass",
        "fm bass",
        "dist",
        "low",
        "crusher",
        "destroyer",
        "industrial"};

    return std::any_of(std::begin(kBassTokens), std::end(kBassTokens), [&](const char* token) {
        return containsToken(lowerName, token);
    });
}

bool looksLikeLeadPatchByName(const std::string& name) {
    const std::string lowerName = lowerCopy(name);
    constexpr const char* kLeadTokens[] = {
        "lead",
        "arp",
        "scream",
        "saw",
        "stab",
        "acid",
        "bright",
        "twin",
        "glass",
        "bell",
        "choir",
        "pad"};

    return std::any_of(std::begin(kLeadTokens), std::end(kLeadTokens), [&](const char* token) {
        return containsToken(lowerName, token);
    });
}

bool looksLikePercussivePatchByName(const std::string& name) {
    const std::string lowerName = lowerCopy(name);
    constexpr const char* kPercussiveTokens[] = {
        "kick",
        "snare",
        "hat",
        "hihat",
        "clap",
        "tom",
        "ride",
        "rim",
        "cym",
        "cymbal",
        "shaker",
        "drum",
        "perc",
        "noisehit",
        "closed",
        "open",
        "attack"};

    return std::any_of(std::begin(kPercussiveTokens), std::end(kPercussiveTokens), [&](const char* token) {
        return containsToken(lowerName, token);
    });
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
    } else if (name == "portamento" || name == "portamento_time" || name == "glide") {
        patch.portamentoTime = std::clamp(value, 0.0, 5.0);
    } else if (name == "portamento_legato" || name == "glide_legato") {
        patch.portamentoLegato = value >= 0.5;
    } else if (name == "fm_decay") {
        patch.fmDecay = std::clamp(value, 0.0, 8.0);
    } else if (name == "velocity_to_fm" || name == "vel_fm") {
        patch.velocityToFm = std::clamp(value, 0.0, 1.0);
    } else if (name == "mono_mode" || name == "mono") {
        patch.monoMode = value >= 0.5;
    } else if (name == "osc_b_ratio") {
        patch.oscBRatio = std::clamp(value, 0.0625, 16.0);
    } else if (name == "osc_c_ratio") {
        patch.oscCRatio = std::clamp(value, 0.0625, 16.0);
    } else if (name == "osc_d_ratio") {
        patch.oscDRatio = std::clamp(value, 0.0625, 16.0);
    } else if (name == "osc_b_decay") {
        patch.oscBDecay = std::clamp(value, 0.0, 8.0);
    } else if (name == "osc_c_decay") {
        patch.oscCDecay = std::clamp(value, 0.0, 8.0);
    } else if (name == "osc_d_decay") {
        patch.oscDDecay = std::clamp(value, 0.0, 8.0);
    } else if (name == "velocity_to_decay" || name == "vel_decay") {
        patch.velocityToDecay = std::clamp(value, 0.0, 1.0);
    } else if (name == "key_track_decay" || name == "keytrack_decay") {
        patch.keyTrackDecay = std::clamp(value, 0.0, 1.0);
    } else {
        return false;
    }

    return true;
}

void applyCompetitionPresetQuality(SynthPatch& patch, double intensity, bool percussive) {
    const double t = std::clamp(intensity, 0.0, 1.0);
    const bool isPercussive = percussive || looksLikePercussivePatchByName(patch.name);
    const bool looksBass = looksLikeBassPatchByName(patch.name);
    const bool looksLead = looksLikeLeadPatchByName(patch.name);
    const auto applyMin = [&](double& value, double minValue, double range) {
        value = std::clamp(std::max(value, minValue + t * range), 0.0, 1.0);
    };

    applyMin(patch.analogColor, 0.40, 0.26);
    applyMin(patch.analogWarmth, 0.40, 0.30);
    applyMin(patch.voiceSlop, 0.20, 0.28);
    applyMin(patch.phaseScatter, 0.18, 0.28);
    applyMin(patch.unisonWarp, 0.10, 0.30);
    applyMin(patch.unisonHumanize, 0.18, 0.36);
    applyMin(patch.fmColor, 0.34, 0.34);
    applyMin(patch.fmSpread, 0.10, 0.22);
    applyMin(patch.chorusTone, 0.28, 0.26);
    applyMin(patch.chorusJitter, 0.14, 0.20);
    applyMin(patch.chorusSaturation, 0.10, 0.12);
    applyMin(patch.delayDiffusion, 0.20, 0.24);
    applyMin(patch.delayWow, 0.10, 0.28);
    applyMin(patch.delayCrossfeed, 0.18, 0.22);
    applyMin(patch.reverbDecay, 0.38, 0.30);
    applyMin(patch.reverbEarlyMix, 0.16, 0.18);
    applyMin(patch.reverbTone, 0.24, 0.22);
    applyMin(patch.reverbChorus, 0.08, 0.16);
    applyMin(patch.reverbBloom, 0.12, 0.20);
    applyMin(patch.consoleCrosstalk, 0.03, 0.14);
    applyMin(patch.stereoDepth, 0.14, 0.30);
    applyMin(patch.hifiExciter, 0.08, 0.18);
    applyMin(patch.outputTransformer, 0.08, 0.16);
    applyMin(patch.outputSoftClip, 0.07, 0.15);
    applyMin(patch.outputGlue, 0.18, 0.16);
    applyMin(patch.tapeColor, 0.10, 0.18);
    applyMin(patch.airBoost, 0.10, 0.16);
    applyMin(patch.lowPunch, 0.14, 0.18);
    applyMin(patch.wowFlutter, 0.05, 0.16);
    applyMin(patch.vintageDrift, 0.20, 0.30);
    applyMin(patch.chorusEnsemble, 0.10, 0.24);
    patch.cutoff = std::clamp(std::max(patch.cutoff, 0.28 + t * 0.24), 0.0, 1.0);
    patch.subOscillator = std::clamp(std::max(patch.subOscillator, 0.16 + t * 0.28), 0.0, 1.0);
    patch.pan = std::clamp(patch.pan, -0.90, 0.90);
    patch.chorusFeedback = std::clamp(std::max(patch.chorusFeedback, 0.06 + t * 0.22), 0.0, 0.60);
    patch.chorusDelay = std::clamp(std::max(patch.chorusDelay, 0.12 + t * 0.24), 0.0, 1.0);
    patch.chorusWidth = std::clamp(std::max(patch.chorusWidth, 0.36 + t * 0.20), 0.0, 1.0);
    patch.delayDrive = std::clamp(std::max(patch.delayDrive, 0.02 + t * 0.14), 0.0, 1.0);
    applyMin(patch.delayModDepth, 0.08, 0.14);
    patch.delayStereo = std::clamp(std::max(patch.delayStereo, 0.10 + t * 0.20), 0.0, 1.0);
    applyMin(patch.delayDucking, 0.04, 0.16);
    patch.reverbDiffusion = std::clamp(std::max(patch.reverbDiffusion, 0.28 + t * 0.22), 0.0, 1.0);
    patch.reverbWidth = std::clamp(std::max(patch.reverbWidth, 0.12 + t * 0.16), 0.0, 1.0);
    patch.reverbModDepth = std::clamp(std::max(patch.reverbModDepth, 0.04 + t * 0.10), 0.0, 1.0);
    patch.reverbShimmer = std::clamp(std::max(patch.reverbShimmer, 0.02 + t * 0.10), 0.0, 1.0);

    if (isPercussive) {
        patch.delayMix = std::clamp(std::max(patch.delayMix, 0.02 + t * 0.10), 0.0, 0.22);
        patch.reverbMix = std::clamp(std::max(patch.reverbMix, 0.04 + t * 0.12), 0.0, 0.24);
        patch.combMix = std::clamp(std::max(patch.combMix, 0.02 + t * 0.08), 0.0, 0.16);
        patch.transientShape = std::clamp(std::max(patch.transientShape, 0.34 + t * 0.30), 0.0, 1.0);
    } else {
        patch.delayMix = std::clamp(std::max(patch.delayMix, 0.04 + t * 0.08), 0.0, 0.18);
        patch.reverbMix = std::clamp(std::max(patch.reverbMix, 0.06 + t * 0.10), 0.0, 0.20);
        patch.combMix = std::clamp(std::max(patch.combMix, 0.02 + t * 0.08), 0.0, 0.16);
        if (patch.chorusEnabled) {
            patch.chorusMix = std::clamp(std::max(patch.chorusMix, 0.04 + t * 0.08), 0.0, 0.26);
        }
    }

    patch.gain = std::clamp(std::max(patch.gain, 0.06), 0.06, 0.80);
    patch.drive = std::clamp(std::max(patch.drive, 0.04), 0.0, 0.66);
    patch.wavefold = std::clamp(patch.wavefold, 0.0, 0.28);
    patch.filterDrive = std::clamp(patch.filterDrive, 0.0, 0.72);
    patch.filterEnvelope.decay = std::clamp(patch.filterEnvelope.decay, 0.001, 1.50);
    patch.ampEnvelope.decay = std::clamp(patch.ampEnvelope.decay, 0.001, 0.70);
    patch.hifiExciter = std::min(patch.hifiExciter, 0.16);
    patch.outputTransformer = std::min(patch.outputTransformer, 0.24);
    patch.outputSoftClip = std::min(patch.outputSoftClip, 0.18);
    patch.outputGlue = std::min(patch.outputGlue, 0.26);
    patch.toneTilt = std::clamp(patch.toneTilt, -0.9, 0.9);
    patch.pitchEnvelopeSemitones = std::clamp(patch.pitchEnvelopeSemitones, -24.0, 24.0);
    patch.pitchEnvelopeDecay = std::clamp(patch.pitchEnvelopeDecay, 0.0, 0.60);
    patch.delayFeedback = std::clamp(patch.delayFeedback, 0.0, 0.72);
    patch.delayTime = std::clamp(patch.delayTime, 0.0, 0.95);
    patch.combFeedback = std::clamp(patch.combFeedback, 0.0, 0.56);
    patch.reverbDecay = std::clamp(patch.reverbDecay, 0.0, 0.86);
    patch.reverbDiffusion = std::clamp(patch.reverbDiffusion, 0.0, 0.86);
    patch.delayDiffusion = std::clamp(patch.delayDiffusion, 0.0, 0.86);
    patch.reverbWidth = std::clamp(patch.reverbWidth, 0.0, 0.72);
    patch.outputTransformer = std::clamp(patch.outputTransformer, 0.0, 0.32);
    patch.outputSoftClip = std::clamp(patch.outputSoftClip, 0.0, 0.24);
    if (!patch.bitCrushEnabled) {
        patch.bitCrush = 0.0;
        patch.sampleRateReduction = 0.0;
    } else {
        patch.bitCrush = std::min(patch.bitCrush, 0.14);
        patch.sampleRateReduction = std::min(patch.sampleRateReduction, 0.10);
        if (patch.bitCrush <= 0.01 && patch.sampleRateReduction <= 0.01) {
            patch.bitCrushEnabled = false;
            patch.bitCrush = 0.0;
            patch.sampleRateReduction = 0.0;
        }
    }
    if (isPercussive) {
        patch.delayWow = std::clamp(patch.delayWow, 0.0, 0.22);
        patch.delayDiffusion = std::clamp(patch.delayDiffusion, 0.0, 0.70);
        patch.reverbShimmer = std::clamp(patch.reverbShimmer, 0.0, 0.08);
        patch.delayDrive = std::clamp(patch.delayDrive, 0.0, 0.12);
        patch.delayModDepth = std::clamp(patch.delayModDepth, 0.0, 0.18);
        patch.reverbModDepth = std::clamp(patch.reverbModDepth, 0.0, 0.18);
        patch.outputSoftClip = std::min(patch.outputSoftClip, 0.16);
        patch.outputTransformer = std::min(patch.outputTransformer, 0.30);
        patch.delayFeedback = std::min(patch.delayFeedback, 0.56);
        patch.ampEnvelope.attack = std::clamp(patch.ampEnvelope.attack, 0.0002, 0.006);
        patch.ampEnvelope.decay = std::clamp(patch.ampEnvelope.decay, 0.001, 0.18);
        patch.filterEnvelope.decay = std::clamp(patch.filterEnvelope.decay, 0.001, 0.16);
    }
    if (looksBass) {
        patch.cutoff = std::clamp(std::max(patch.cutoff, 0.20 + t * 0.20), 0.0, 0.88);
        patch.subOscillator = std::clamp(std::max(patch.subOscillator, 0.18 + t * 0.40), 0.0, 1.0);
        patch.reverbMix = std::clamp(patch.reverbMix, 0.0, 0.18);
        patch.delayMix = std::clamp(patch.delayMix, 0.0, 0.18);
        patch.lowPunch = std::clamp(std::max(patch.lowPunch, 0.24 + t * 0.40), 0.0, 0.90);
        patch.filterDrive = std::clamp(patch.filterDrive, 0.0, 0.82);
        patch.resonance = std::clamp(patch.resonance, 0.0, 0.62);
        patch.filterEnvelope.attack = std::clamp(patch.filterEnvelope.attack, 0.001, 0.16);
        patch.ampEnvelope.attack = std::clamp(patch.ampEnvelope.attack, 0.0005, 0.08);
        patch.ampEnvelope.release = std::clamp(patch.ampEnvelope.release, 0.02, 0.45);
    }
    if (looksLead) {
        patch.delayMix = std::clamp(patch.delayMix, 0.0, 0.24);
        patch.reverbMix = std::clamp(patch.reverbMix, 0.0, 0.22);
        patch.hifiExciter = std::clamp(patch.hifiExciter, 0.02, 0.24);
        patch.airBoost = std::clamp(patch.airBoost, 0.0, 0.24);
        patch.chorusTone = std::clamp(patch.chorusTone, 0.0, 0.90);
        patch.ampEnvelope.attack = std::clamp(patch.ampEnvelope.attack, 0.001, 0.12);
        patch.filterEnvelope.attack = std::clamp(patch.filterEnvelope.attack, 0.001, 0.20);
    }
    patch.chorusSaturation = std::clamp(patch.chorusSaturation, 0.0, 0.28);
    patch.delayModDepth = std::clamp(patch.delayModDepth, 0.0, 0.36);
    patch.reverbModDepth = std::clamp(patch.reverbModDepth, 0.0, 0.30);
    patch.reverbChorus = std::clamp(patch.reverbChorus, 0.0, 0.32);
    patch.lowPunch = std::min(patch.lowPunch, 0.88);
    patch.toneTilt = std::clamp(patch.toneTilt, -0.40, 0.45);
    patch.airBoost = std::clamp(patch.airBoost, 0.0, 0.28);
    patch.highPass = std::clamp(patch.highPass, 0.0, 0.34);
    patch.click = std::clamp(patch.click, 0.0, 0.18);
    patch.transientNoise = std::min(patch.transientNoise, 0.44);
}

} // namespace arachno
