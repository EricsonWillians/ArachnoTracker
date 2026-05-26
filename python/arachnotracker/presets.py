from .model import Envelope, SynthPatch, Waveform


def low_saw() -> SynthPatch:
    return SynthPatch(
        name="Low Saw",
        oscillator_a=Waveform.SAW,
        oscillator_b=Waveform.SQUARE,
        oscillator_mix=0.22,
        pulse_width=0.42,
        pwm_depth=0.08,
        unison_voices=2,
        unison_detune_cents=4.0,
        stereo_spread=0.08,
        sub_oscillator=0.35,
        cutoff=0.36,
        filter_envelope_amount=0.22,
        hard_sync=0.18,
        drive=0.22,
        bit_crush=0.06,
        amp_envelope=Envelope(attack=0.002, decay=0.07, sustain=0.55, release=0.08),
    )


def ebm_kick() -> SynthPatch:
    return SynthPatch(
        name="EBM Kick",
        oscillator_a=Waveform.SINE,
        oscillator_b=Waveform.TRIANGLE,
        oscillator_mix=0.08,
        pitch_envelope_semitones=34.0,
        pitch_envelope_decay=0.055,
        sub_oscillator=0.22,
        noise=0.03,
        click=0.42,
        transient_noise=0.08,
        transient_decay=0.01,
        cutoff=0.62,
        filter_envelope_amount=0.16,
        drive=0.42,
        gain=0.86,
        amp_envelope=Envelope(attack=0.001, decay=0.11, sustain=0.0, release=0.035),
    )


def gated_snare() -> SynthPatch:
    return SynthPatch(
        name="Gated Snare",
        oscillator_a=Waveform.NOISE,
        oscillator_b=Waveform.SQUARE,
        oscillator_mix=0.32,
        fm_amount=0.16,
        fm_ratio=3.0,
        noise=0.82,
        click=0.25,
        transient_noise=0.58,
        transient_decay=0.025,
        pitch_envelope_semitones=9.0,
        pitch_envelope_decay=0.035,
        cutoff=0.74,
        high_pass=0.36,
        ring_mod=0.18,
        drive=0.28,
        bit_crush=0.12,
        gain=0.52,
        amp_envelope=Envelope(attack=0.001, decay=0.09, sustain=0.0, release=0.16),
    )


def metal_hat() -> SynthPatch:
    return SynthPatch(
        name="Metal Hat",
        oscillator_a=Waveform.NOISE,
        oscillator_b=Waveform.SQUARE,
        oscillator_mix=0.65,
        pulse_width=0.28,
        fm_amount=0.22,
        fm_ratio=5.0,
        chorus_mix=0.08,
        chorus_rate=0.9,
        chorus_depth=0.18,
        noise=0.92,
        click=0.18,
        transient_noise=0.72,
        transient_decay=0.008,
        cutoff=0.95,
        high_pass=0.78,
        ring_mod=0.35,
        hard_sync=0.44,
        bit_crush=0.22,
        sample_rate_reduction=0.18,
        gain=0.28,
        amp_envelope=Envelope(attack=0.001, decay=0.035, sustain=0.0, release=0.025),
    )


def bright_twin() -> SynthPatch:
    return SynthPatch(
        name="Bright Twin",
        oscillator_a=Waveform.SAW,
        oscillator_b=Waveform.TRIANGLE,
        oscillator_mix=0.42,
        detune_cents=11.0,
        pulse_width=0.46,
        pwm_depth=0.12,
        fm_amount=0.08,
        fm_ratio=2.0,
        chorus_mix=0.18,
        chorus_rate=0.42,
        chorus_depth=0.3,
        unison_voices=3,
        unison_detune_cents=9.0,
        stereo_spread=0.32,
        cutoff=0.86,
        resonance=0.18,
        lfo_rate=5.8,
        vibrato_cents=4.0,
        tremolo_depth=0.08,
        ring_mod=0.08,
        hard_sync=0.12,
        drive=0.06,
        amp_envelope=Envelope(attack=0.004, decay=0.1, sustain=0.7, release=0.16),
    )


def soft_wide() -> SynthPatch:
    return SynthPatch(
        name="Soft Wide",
        oscillator_a=Waveform.SINE,
        oscillator_b=Waveform.TRIANGLE,
        oscillator_mix=0.5,
        detune_cents=-8.0,
        unison_voices=5,
        unison_detune_cents=14.0,
        stereo_spread=0.62,
        chorus_mix=0.32,
        chorus_rate=0.24,
        chorus_depth=0.58,
        cutoff=0.55,
        lfo_rate=0.45,
        vibrato_cents=2.5,
        tremolo_depth=0.12,
        gain=0.42,
        amp_envelope=Envelope(attack=0.08, decay=0.3, sustain=0.78, release=0.45),
    )
