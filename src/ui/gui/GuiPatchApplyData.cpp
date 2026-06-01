#include "ui/gui/GuiPatchApplyData.h"

#include <algorithm>
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

std::vector<SynthWaveAssignment> synthPatchWaveAssignments(const SynthPatch& patch) {
    return {
        {"A", lowerCopy(waveformName(patch.oscillatorA))},
        {"B", lowerCopy(waveformName(patch.oscillatorB))},
        {"C", lowerCopy(waveformName(patch.oscillatorC))},
        {"D", lowerCopy(waveformName(patch.oscillatorD))},
    };
}

std::vector<std::pair<std::string, double>> synthPatchParameterAssignments(const SynthPatch& patch) {
    return {
        {"osc_a_enabled", patch.oscillatorAEnabled ? 1.0 : 0.0},
        {"osc_b_enabled", patch.oscillatorBEnabled ? 1.0 : 0.0},
        {"osc_c_enabled", patch.oscillatorCEnabled ? 1.0 : 0.0},
        {"osc_d_enabled", patch.oscillatorDEnabled ? 1.0 : 0.0},
        {"oscillator_mix", patch.oscillatorMix},
        {"oscillator_c_mix", patch.oscillatorCMix},
        {"oscillator_d_mix", patch.oscillatorDMix},
        {"detune_cents", patch.detuneCents},
        {"detune_c_cents", patch.detuneCCents},
        {"detune_d_cents", patch.detuneDCents},
        {"pulse_width", patch.pulseWidth},
        {"pwm_depth", patch.pwmDepth},
        {"fm_enabled", patch.fmEnabled ? 1.0 : 0.0},
        {"fm_amount", patch.fmAmount},
        {"fm_ratio", patch.fmRatio},
        {"fm_feedback", patch.fmFeedback},
        {"fm_algorithm", static_cast<double>(patch.fmAlgorithm)},
        {"chorus_enabled", patch.chorusEnabled ? 1.0 : 0.0},
        {"chorus_mix", patch.chorusMix},
        {"chorus_rate", patch.chorusRate},
        {"chorus_depth", patch.chorusDepth},
        {"unison_voices", static_cast<double>(patch.unisonVoices)},
        {"unison_detune_cents", patch.unisonDetuneCents},
        {"stereo_spread", patch.stereoSpread},
        {"sub_enabled", patch.subEnabled ? 1.0 : 0.0},
        {"sub_oscillator", patch.subOscillator},
        {"noise_enabled", patch.noiseEnabled ? 1.0 : 0.0},
        {"noise", patch.noise},
        {"noise_tone", patch.noiseTone},
        {"cutoff", patch.cutoff},
        {"resonance", patch.resonance},
        {"filter_mode", static_cast<double>(patch.filterMode)},
        {"filter_drive", patch.filterDrive},
        {"filter_keytrack", patch.filterKeytrack},
        {"filter_envelope", patch.filterEnvelopeAmount},
        {"lfo_filter_depth", patch.lfoFilterDepth},
        {"lfo_pan_depth", patch.lfoPanDepth},
        {"pitch_envelope_semitones", patch.pitchEnvelopeSemitones},
        {"pitch_envelope_decay", patch.pitchEnvelopeDecay},
        {"lfo_rate", patch.lfoRate},
        {"vibrato_cents", patch.vibratoCents},
        {"tremolo_depth", patch.tremoloDepth},
        {"ring_enabled", patch.ringEnabled ? 1.0 : 0.0},
        {"ring_mod", patch.ringMod},
        {"hard_sync_enabled", patch.hardSyncEnabled ? 1.0 : 0.0},
        {"hard_sync", patch.hardSync},
        {"drive", patch.drive},
        {"wavefold", patch.wavefold},
        {"bit_crush_enabled", patch.bitCrushEnabled ? 1.0 : 0.0},
        {"bit_crush", patch.bitCrush},
        {"sample_rate_reduction", patch.sampleRateReduction},
        {"comb_mix", patch.combMix},
        {"comb_time", patch.combTime},
        {"comb_feedback", patch.combFeedback},
        {"high_pass", patch.highPass},
        {"click", patch.click},
        {"transient_shape", patch.transientShape},
        {"transient_noise", patch.transientNoise},
        {"transient_pitch_semitones", patch.transientPitchSemitones},
        {"transient_pitch_decay", patch.transientPitchDecay},
        {"transient_burst_count", static_cast<double>(patch.transientBurstCount)},
        {"transient_burst_spacing", patch.transientBurstSpacing},
        {"transient_burst_decay", patch.transientBurstDecay},
        {"transient_tone", patch.transientTone},
        {"transient_decay", patch.transientDecay},
        {"analog_color", patch.analogColor},
        {"tone_tilt", patch.toneTilt},
        {"gain", patch.gain},
        {"pan", patch.pan},
        {"amp_attack", patch.ampEnvelope.attack},
        {"amp_decay", patch.ampEnvelope.decay},
        {"amp_sustain", patch.ampEnvelope.sustain},
        {"amp_release", patch.ampEnvelope.release},
        {"filter_attack", patch.filterEnvelope.attack},
        {"filter_decay", patch.filterEnvelope.decay},
        {"filter_sustain", patch.filterEnvelope.sustain},
        {"filter_release", patch.filterEnvelope.release},
        {"osc_a_level", patch.oscALevel},
        {"osc_b_level", patch.oscBLevel},
        {"osc_c_level", patch.oscCLevel},
        {"osc_d_level", patch.oscDLevel},
        {"osc_a_detune_cents", patch.oscADetuneCents},
        {"osc_b_detune_cents", patch.oscBDetuneCents},
        {"osc_c_detune_cents", patch.oscCDetuneCents},
        {"osc_d_detune_cents", patch.oscDDetuneCents},
        {"osc_a_pulse_width", patch.oscAPulseWidth},
        {"osc_b_pulse_width", patch.oscBPulseWidth},
        {"osc_c_pulse_width", patch.oscCPulseWidth},
        {"osc_d_pulse_width", patch.oscDPulseWidth},
        {"osc_a_pwm_depth", patch.oscAPwmDepth},
        {"osc_b_pwm_depth", patch.oscBPwmDepth},
        {"osc_c_pwm_depth", patch.oscCPwmDepth},
        {"osc_d_pwm_depth", patch.oscDPwmDepth},
        {"osc_a_drive", patch.oscADrive},
        {"osc_b_drive", patch.oscBDrive},
        {"osc_c_drive", patch.oscCDrive},
        {"osc_d_drive", patch.oscDDrive},
    };
}

} // namespace arachno

