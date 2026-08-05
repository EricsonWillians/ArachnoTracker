from __future__ import annotations

from .model import Envelope, SynthPatch, Waveform


DEFAULT_COMPETITION_INTENSITY = 0.85


def _looks_percussive(name: str) -> bool:
    lower = name.lower()
    return any(token in lower for token in (
        "kick",
        "snare",
        "hat",
        "hihat",
        "clap",
        "tom",
        "ride",
        "rim",
        "cym",
        "drum",
        "perc",
    ))


def apply_competition_quality(
    patch: SynthPatch, intensity: float = DEFAULT_COMPETITION_INTENSITY
) -> SynthPatch:
    """
    Darkwave / 80s/90s gothic production preset pass.
    Keeps the requested "80s/90s synthpop character" while retaining the
    anti-glitch constraints introduced for percussive sources.
    """
    t = max(0.0, min(intensity, 1.0))
    percussive = _looks_percussive(patch.name)

    # 1) Vintage analog + digital blend foundation
    if patch.detune_cents >= 0.0:
        patch.detune_cents = max(patch.detune_cents, 8.0 + t * 4.0)
    patch.sub_oscillator = max(patch.sub_oscillator, 0.25 + t * 0.20)
    patch.noise = min(max(patch.noise, 0.02 + t * 0.08), 0.55)
    patch.cutoff = min(max(patch.cutoff, 0.15), 0.85)
    patch.filter_envelope_amount = max(patch.filter_envelope_amount, 0.16 + t * 0.24)
    patch.lfo_rate = max(patch.lfo_rate, 0.20 + t * 0.50)
    patch.drive = max(patch.drive, 0.20 + t * 0.40)
    patch.gain = max(patch.gain, 0.30 + t * 0.20)

    # 2) Juno-style movement: thick chorus + stereo
    patch.chorus_mix = max(patch.chorus_mix, 0.30 + t * 0.40)
    patch.chorus_rate = max(patch.chorus_rate, 0.25 + t * 0.35)
    patch.chorus_depth = max(patch.chorus_depth, 0.40 + t * 0.30)
    patch.unison_detune_cents = max(patch.unison_detune_cents, 4.0 + t * 8.0)
    patch.stereo_spread = max(patch.stereo_spread, 0.20 + t * 0.40)

    patch.amp_envelope.attack = min(max(patch.amp_envelope.attack, 0.0012), 0.06)
    patch.amp_envelope.decay = max(patch.amp_envelope.decay, 0.04 + t * 0.14)
    patch.amp_envelope.sustain = max(patch.amp_envelope.sustain, 0.38)
    patch.amp_envelope.release = max(patch.amp_envelope.release, 0.03 + t * 0.16)

    patch.filter_envelope.attack = min(max(patch.filter_envelope.attack, 0.001), 0.05)
    patch.filter_envelope.decay = max(patch.filter_envelope.decay, 0.05 + t * 0.16)
    patch.filter_envelope.sustain = max(patch.filter_envelope.sustain, 0.08)
    patch.filter_envelope.release = max(patch.filter_envelope.release, 0.02 + t * 0.16)

    if percussive:
        patch.bit_crush = max(0.0, min(patch.bit_crush + t * 0.02, 0.05))
        patch.sample_rate_reduction = max(0.0, min(patch.sample_rate_reduction + t * 0.02, 0.05))
        patch.noise = min(max(patch.noise, 0.02 + t * 0.10), 0.55)
        patch.click = min(max(patch.click, 0.03 + t * 0.04), 0.10)
        patch.transient_noise = min(max(patch.transient_noise, 0.08 + t * 0.06), 0.14)
        patch.transient_decay = min(max(patch.transient_decay, 0.010), 0.020)
        patch.transient_tone = min(max(patch.transient_tone, 0.40), 0.72)
        if "snare" in patch.name.lower():
            # reverb_decay is not part of the serializable SynthPatch model; keep the
            # tuning intent without breaking patch construction.
            patch.amp_envelope.release = min(max(patch.amp_envelope.release, 0.05), 0.16)
        patch.gain = min(max(patch.gain, 0.28), 0.72)
    else:
        patch.hard_sync = max(patch.hard_sync, 0.10 + t * 0.10)
        patch.fm_amount = max(patch.fm_amount, 0.12 + t * 0.22)
        patch.fm_ratio = max(patch.fm_ratio, 1.05 + t * 0.9)
        patch.vibrato_cents = max(patch.vibrato_cents, 0.5 + t * 3.0)

    patch.gain = min(max(patch.gain, 0.28), 0.86)
    patch.cutoff = min(max(patch.cutoff, 0.2), 0.96)
    patch.resonance = min(max(patch.resonance, 0.0), 0.56)
    patch.chorus_mix = min(patch.chorus_mix, 0.90)
    patch.chorus_depth = min(patch.chorus_depth, 0.90)
    patch.stereo_spread = min(patch.stereo_spread, 0.8)

    return patch


_apply_competition_quality = apply_competition_quality


def low_saw() -> SynthPatch:
    return apply_competition_quality(SynthPatch(
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
    ))


def ebm_kick() -> SynthPatch:
    return apply_competition_quality(SynthPatch(
        name="EBM Kick",
        oscillator_a=Waveform.SINE,
        oscillator_b=Waveform.TRIANGLE,
        oscillator_mix=0.08,
        pitch_envelope_semitones=34.0,
        pitch_envelope_decay=0.055,
        sub_oscillator=0.22,
        noise=0.03,
        click=0.06,
        transient_noise=0.03,
        transient_decay=0.01,
        cutoff=0.62,
        filter_envelope_amount=0.16,
        drive=0.12,
        gain=0.82,
        amp_envelope=Envelope(attack=0.001, decay=0.11, sustain=0.0, release=0.035),
    ))


def gated_snare() -> SynthPatch:
    return apply_competition_quality(SynthPatch(
        name="Gated Snare",
        oscillator_a=Waveform.NOISE,
        oscillator_b=Waveform.SQUARE,
        oscillator_mix=0.32,
        fm_amount=0.16,
        fm_ratio=3.0,
        noise=0.82,
        click=0.10,
        transient_noise=0.10,
        transient_decay=0.025,
        pitch_envelope_semitones=9.0,
        pitch_envelope_decay=0.035,
        cutoff=0.72,
        high_pass=0.2,
        ring_mod=0.18,
        drive=0.08,
        bit_crush=0.03,
        gain=0.52,
        amp_envelope=Envelope(attack=0.001, decay=0.09, sustain=0.0, release=0.16),
    ))


def metal_hat() -> SynthPatch:
    return apply_competition_quality(SynthPatch(
        name="Metal Hat",
        oscillator_a=Waveform.NOISE,
        oscillator_b=Waveform.SQUARE,
        oscillator_mix=0.65,
        pulse_width=0.28,
        fm_amount=0.22,
        fm_ratio=5.0,
        chorus_mix=0.08,
        chorus_rate=0.9,
        chorus_depth=0.16,
        noise=0.72,
        click=0.10,
        transient_noise=0.22,
        transient_decay=0.008,
        cutoff=0.84,
        high_pass=0.5,
        ring_mod=0.35,
        hard_sync=0.44,
        bit_crush=0.04,
        sample_rate_reduction=0.03,
        gain=0.28,
        amp_envelope=Envelope(attack=0.001, decay=0.035, sustain=0.0, release=0.025),
    ))


def bright_twin() -> SynthPatch:
    return apply_competition_quality(SynthPatch(
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
        chorus_depth=0.16,
        unison_voices=3,
        unison_detune_cents=9.0,
        stereo_spread=0.32,
        cutoff=0.82,
        resonance=0.18,
        lfo_rate=5.8,
        vibrato_cents=4.0,
        tremolo_depth=0.08,
        ring_mod=0.08,
        hard_sync=0.12,
        drive=0.06,
        amp_envelope=Envelope(attack=0.004, decay=0.1, sustain=0.7, release=0.16),
    ))


def soft_wide() -> SynthPatch:
    return apply_competition_quality(SynthPatch(
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
        chorus_depth=0.34,
        cutoff=0.55,
        lfo_rate=0.45,
        vibrato_cents=2.5,
        tremolo_depth=0.12,
        gain=0.42,
        amp_envelope=Envelope(attack=0.08, decay=0.3, sustain=0.78, release=0.45),
    ))


def classic_strings() -> SynthPatch:
    """Lush 80s ensemble strings; full ADSR sustains for as long as a key is held."""
    return apply_competition_quality(SynthPatch(
        name="Vintage Strings",
        oscillator_a=Waveform.SAW,
        oscillator_b=Waveform.TRIANGLE,
        oscillator_mix=0.5,
        unison_voices=5,
        unison_detune_cents=8.0,
        stereo_spread=0.55,
        chorus_mix=0.32,
        chorus_rate=0.18,
        chorus_depth=0.42,
        cutoff=0.48,
        resonance=0.08,
        filter_envelope_amount=0.12,
        vibrato_cents=4.0,
        gain=0.38,
        amp_envelope=Envelope(attack=0.09, decay=0.25, sustain=0.78, release=0.55),
        filter_envelope=Envelope(attack=0.12, decay=0.3, sustain=0.5, release=0.4),
    ))


def synth_strings_85() -> SynthPatch:
    """Brighter mid-80s synth strings with a faster response; sustains while held."""
    return apply_competition_quality(SynthPatch(
        name="Synth Strings 85",
        oscillator_a=Waveform.SAW,
        oscillator_b=Waveform.SAW,
        oscillator_mix=0.5,
        detune_cents=9.0,
        unison_voices=4,
        unison_detune_cents=9.0,
        stereo_spread=0.58,
        chorus_mix=0.3,
        chorus_rate=0.25,
        chorus_depth=0.4,
        cutoff=0.52,
        resonance=0.12,
        filter_envelope_amount=0.18,
        lfo_rate=2.6,
        vibrato_cents=5.0,
        gain=0.4,
        amp_envelope=Envelope(attack=0.035, decay=0.22, sustain=0.8, release=0.45),
        filter_envelope=Envelope(attack=0.05, decay=0.3, sustain=0.55, release=0.35),
    ))


def analog_string_machine() -> SynthPatch:
    """Solina-style string machine: slow swell, deep ensemble chorus, long release."""
    return apply_competition_quality(SynthPatch(
        name="Analog String Machine",
        oscillator_a=Waveform.SAW,
        oscillator_b=Waveform.SQUARE,
        oscillator_mix=0.55,
        unison_voices=6,
        unison_detune_cents=12.0,
        stereo_spread=0.68,
        chorus_mix=0.5,
        chorus_rate=0.14,
        chorus_depth=0.6,
        cutoff=0.44,
        resonance=0.06,
        filter_envelope_amount=0.1,
        vibrato_cents=2.5,
        gain=0.34,
        amp_envelope=Envelope(attack=0.3, decay=0.4, sustain=0.9, release=0.8),
        filter_envelope=Envelope(attack=0.35, decay=0.5, sustain=0.6, release=0.6),
    ))
