from arachnotracker import Envelope, SynthPatch, Waveform
from arachnotracker import presets


def create_patch(name: str = "Plugin Industrial Bass") -> SynthPatch:
    return presets.apply_competition_quality(SynthPatch(
        name=name,
        oscillator_a=Waveform.SAW,
        oscillator_b=Waveform.SQUARE,
        oscillator_mix=0.25,
        pulse_width=0.4,
        pwm_depth=0.12,
        unison_voices=3,
        unison_detune_cents=7.0,
        stereo_spread=0.18,
        sub_oscillator=0.4,
        cutoff=0.34,
        resonance=0.18,
        filter_envelope_amount=0.28,
        drive=0.34,
        hard_sync=0.2,
        bit_crush=0.05,
        gain=0.62,
        amp_envelope=Envelope(attack=0.002, decay=0.08, sustain=0.6, release=0.07),
        filter_envelope=Envelope(attack=0.001, decay=0.09, sustain=0.25, release=0.08),
    ))
