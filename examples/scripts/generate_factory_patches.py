#!/usr/bin/env python3
"""Generate ArachnoTracker's factory patch library (patches/factory/).

The first files in each category folder (01_*) are renamed exports of the
original C++ demo-song instruments and are NOT regenerated here. This script
writes the remaining library entries using the Python composition SDK, so the
catalog can be reproduced and extended:

    PYTHONPATH=python python3 examples/scripts/generate_factory_patches.py

Use --check to validate that every .arachnopatch in patches/factory loads.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

import arachnotracker as at
from arachnotracker import Envelope, SynthPatch, Waveform

REPO_ROOT = Path(__file__).resolve().parents[2]
FACTORY_DIR = REPO_ROOT / "patches" / "factory"


def env(attack: float, decay: float, sustain: float, release: float) -> Envelope:
    return Envelope(attack=attack, decay=decay, sustain=sustain, release=release)


def patch(name: str, **fields) -> SynthPatch:
    """Hand-tuned factory patch (no global polish, for maximum variety)."""
    return SynthPatch(name=name, **fields)


def slug(name: str) -> str:
    value = re.sub(r"[^a-z0-9]+", "_", name.lower())
    return re.sub(r"_+", "_", value).strip("_")


def _vary(value: float, name: str, salt: str, amount: float = 0.35) -> float:
    """Deterministic per-patch variation (no RNG, reproducible output)."""
    h = (sum(ord(c) for c in name + salt) % 100) / 100.0
    return max(0.0, min(1.0, value * (1.0 - amount + 2.0 * amount * h)))


# Per-category color palettes. Until now every generated patch carried the same
# hardcoded color block (analog warmth, glue, exciter, transformer, soft-clip),
# which was a major source of the "everything sounds the same" complaint. Each
# category gets a sensible base palette; _vary adds per-patch spread.
CATEGORY_COLOR = {
    "bass": dict(warmth=0.50, color=0.32, drift=0.06, glue=0.20, exciter=0.06,
                 transformer=0.16, softclip=0.20, tilt=-0.06),
    "drums": dict(warmth=0.30, color=0.22, drift=0.02, glue=0.16, exciter=0.10,
                  transformer=0.10, softclip=0.22, tilt=0.02),
    "keys": dict(warmth=0.26, color=0.14, drift=0.03, glue=0.10, exciter=0.08,
                 transformer=0.06, softclip=0.10, tilt=0.0),
    "lead": dict(warmth=0.40, color=0.30, drift=0.05, glue=0.18, exciter=0.12,
                 transformer=0.12, softclip=0.16, tilt=0.03),
    "pads": dict(warmth=0.44, color=0.26, drift=0.05, glue=0.14, exciter=0.05,
                 transformer=0.08, softclip=0.08, tilt=-0.03),
    "strings": dict(warmth=0.40, color=0.22, drift=0.04, glue=0.12, exciter=0.06,
                    transformer=0.08, softclip=0.10, tilt=-0.02),
    "arps": dict(warmth=0.28, color=0.18, drift=0.03, glue=0.12, exciter=0.10,
                 transformer=0.08, softclip=0.12, tilt=0.02),
    "fx": dict(warmth=0.24, color=0.20, drift=0.04, glue=0.10, exciter=0.08,
               transformer=0.06, softclip=0.12, tilt=0.0),
}


_SDK_DEFAULTS = SynthPatch()


def apply_category_color(p: SynthPatch, category: str) -> SynthPatch:
    """Apply the category palette with deterministic per-patch spread.

    Fields a patch definition customized itself (anything not at the SDK
    default) are left untouched, so hand-tuned color choices always win.
    """
    theme = CATEGORY_COLOR[category]

    def maybe(attr: str, value: float) -> None:
        if getattr(p, attr) == getattr(_SDK_DEFAULTS, attr):
            setattr(p, attr, value)

    maybe("analog_warmth", _vary(theme["warmth"], p.name, "w"))
    maybe("analog_color", _vary(theme["color"], p.name, "c"))
    maybe("vintage_drift", _vary(theme["drift"], p.name, "d"))
    maybe("voice_slop", _vary(theme["drift"] * 0.6, p.name, "s"))
    maybe("phase_scatter", _vary(theme["drift"] * 0.8, p.name, "p"))
    maybe("wow_flutter", theme["drift"] * 0.2)
    maybe("output_glue", _vary(theme["glue"], p.name, "g"))
    maybe("hifi_exciter", _vary(theme["exciter"], p.name, "e"))
    maybe("output_transformer", _vary(theme["transformer"], p.name, "t"))
    maybe("output_soft_clip", _vary(theme["softclip"], p.name, "x"))
    maybe("tape_color", _vary(theme["color"] * 0.4, p.name, "k"))
    maybe("tone_tilt", theme["tilt"])
    return p


SINE, SQUARE, SAW, TRIANGLE, NOISE = (
    Waveform.SINE,
    Waveform.SQUARE,
    Waveform.SAW,
    Waveform.TRIANGLE,
    Waveform.NOISE,
)

# ---------------------------------------------------------------------------
# Bass
# ---------------------------------------------------------------------------
BASS = [
    patch("Sub Sine Bass", oscillator_a=SINE, sub_oscillator=0.5, cutoff=0.32,
          gain=0.8, amp_envelope=env(0.004, 0.18, 0.7, 0.12), portamento_time=0.03,
          portamento_legato=True, mono_mode=True, reverb_mix=0.04),
    patch("Reese Darkness", oscillator_a=SAW, oscillator_b=SAW, oscillator_mix=0.5,
          detune_cents=22.0, unison_voices=4, unison_detune_cents=10.0, cutoff=0.3,
          drive=0.45, gain=0.55, amp_envelope=env(0.004, 0.2, 0.75, 0.15),
          chorus_ensemble=0.25, reverb_mix=0.06),
    patch("Acid 303", oscillator_a=SAW, cutoff=0.2, resonance=0.65,
          filter_envelope_amount=0.7, filter_envelope=env(0.001, 0.12, 0.1, 0.08),
          gain=0.6, amp_envelope=env(0.002, 0.14, 0.3, 0.08),
          portamento_time=0.04, portamento_legato=True, mono_mode=True, delay_mix=0.08),
    patch("Wobble Bass", oscillator_a=SAW, cutoff=0.25, resonance=0.4,
          filter_envelope_amount=0.45, lfo_rate=5.5, drive=0.5, gain=0.55,
          amp_envelope=env(0.004, 0.22, 0.8, 0.12), portamento_time=0.02,
          mono_mode=True),
    patch("Dark Pluck Bass", oscillator_a=SAW, oscillator_b=SQUARE, oscillator_mix=0.4,
          cutoff=0.4, gain=0.65, amp_envelope=env(0.002, 0.16, 0.15, 0.06),
          reverb_mix=0.06),
    patch("Analog Funk Bass", oscillator_a=SAW, pulse_width=0.4, pwm_depth=0.25,
          cutoff=0.45, resonance=0.3, gain=0.65,
          amp_envelope=env(0.003, 0.14, 0.5, 0.08), portamento_time=0.025,
          portamento_legato=True),
    patch("FM Punch Bass", oscillator_a=SINE, oscillator_b=SAW, oscillator_mix=0.2,
          fm_amount=0.55, fm_ratio=1.0, fm_decay=0.08, gain=0.65,
          mono_mode=True, amp_envelope=env(0.001, 0.12, 0.4, 0.06)),
    patch("Industrial Drive Bass", oscillator_a=SAW, drive=0.7, bit_crush=0.08,
          cutoff=0.5, gain=0.55, amp_envelope=env(0.002, 0.18, 0.6, 0.1),
          mono_mode=True, reverb_mix=0.05),
    patch("Deep 808 Bass", oscillator_a=SINE, pitch_envelope_semitones=12.0,
          pitch_envelope_decay=0.08, sub_oscillator=0.6, gain=0.8,
          amp_envelope=env(0.002, 0.4, 0.65, 0.25), portamento_time=0.05,
          portamento_legato=True, reverb_mix=0.03),
]

# ---------------------------------------------------------------------------
# Drums
# ---------------------------------------------------------------------------
DRUMS = [
    patch("Kick 808", oscillator_a=SINE, sub_oscillator=0.2,
          pitch_envelope_semitones=36.0, pitch_envelope_decay=0.06, click=0.04,
          transient_noise=0.02, cutoff=0.7, gain=0.85,
          amp_envelope=env(0.001, 0.35, 0.0, 0.08)),
    patch("Kick 909", oscillator_a=SINE, oscillator_b=TRIANGLE, oscillator_mix=0.15,
          pitch_envelope_semitones=42.0, pitch_envelope_decay=0.045, click=0.14,
          drive=0.35, cutoff=0.8, gain=0.9,
          amp_envelope=env(0.001, 0.28, 0.0, 0.05)),
    patch("Snare 909", oscillator_a=NOISE, oscillator_b=SQUARE, oscillator_mix=0.35,
          noise=0.65, noise_tone=0.7, pitch_envelope_semitones=9.0,
          pitch_envelope_decay=0.04, transient_noise=0.1, high_pass=0.1, gain=0.74,
          reverb_mix=0.14, reverb_decay=0.35, amp_envelope=env(0.001, 0.16, 0.0, 0.1)),
    patch("Clap 909", oscillator_a=NOISE, noise=0.8, noise_tone=0.6,
          transient_burst_count=4, transient_burst_spacing=0.012,
          transient_burst_decay=0.55, high_pass=0.25, chorus_mix=0.15, gain=0.70,
          reverb_mix=0.12, amp_envelope=env(0.001, 0.22, 0.0, 0.18)),
    patch("Hat Closed 808", oscillator_a=NOISE, oscillator_b=SQUARE, oscillator_mix=0.3,
          noise=0.9, noise_tone=0.85, high_pass=0.6, fm_amount=0.18, fm_ratio=5.2,
          fm_decay=0.03, gain=0.54, amp_envelope=env(0.001, 0.035, 0.0, 0.02)),
    patch("Hat Open 808", oscillator_a=NOISE, oscillator_b=SQUARE, oscillator_mix=0.3,
          noise=0.9, noise_tone=0.85, high_pass=0.55, fm_amount=0.18, fm_ratio=5.2,
          fm_decay=0.08, gain=0.56, amp_envelope=env(0.001, 0.28, 0.0, 0.12)),
    patch("Cymbal 808", oscillator_a=NOISE, noise=0.7, noise_tone=0.9,
          fm_amount=0.25, fm_ratio=5.0, fm_decay=0.9, high_pass=0.5, gain=0.5,
          amp_envelope=env(0.001, 0.55, 0.0, 0.3)),
    patch("Cowbell 808", oscillator_a=SQUARE, oscillator_b=SQUARE, oscillator_mix=0.5,
          fm_amount=0.5, fm_ratio=3.53, fm_decay=0.15, high_pass=0.3, gain=0.55,
          amp_envelope=env(0.001, 0.13, 0.0, 0.05)),
    patch("Rim Shot", oscillator_a=SQUARE, noise=0.25, high_pass=0.5, click=0.1,
          gain=0.62, amp_envelope=env(0.001, 0.03, 0.0, 0.02)),
    patch("Shaker", oscillator_a=NOISE, noise=0.85, noise_tone=0.8, high_pass=0.65,
          gain=0.46, amp_envelope=env(0.008, 0.07, 0.0, 0.04)),
    patch("Tom 808 Low", oscillator_a=SINE, pitch_envelope_semitones=14.0,
          pitch_envelope_decay=0.09, gain=0.7,
          amp_envelope=env(0.001, 0.32, 0.0, 0.12)),
    patch("Tom 808 High", oscillator_a=SINE, pitch_envelope_semitones=18.0,
          pitch_envelope_decay=0.07, gain=0.7,
          amp_envelope=env(0.001, 0.2, 0.0, 0.08)),
    patch("Zap Snare", oscillator_a=NOISE, noise=0.6, fm_amount=0.4, fm_ratio=7.0,
          high_pass=0.35, gain=0.5, amp_envelope=env(0.001, 0.12, 0.0, 0.06)),
]

# ---------------------------------------------------------------------------
# Keys
# ---------------------------------------------------------------------------
KEYS = [
    patch("Electric Piano", oscillator_a=SINE, oscillator_b=TRIANGLE, oscillator_mix=0.45,
          fm_amount=0.35, fm_ratio=3.0, fm_decay=0.45, velocity_to_fm=0.55,
          tremolo_depth=0.12, lfo_rate=4.5, velocity_to_decay=0.45, key_track_decay=0.35,
          chorus_mix=0.12, chorus_ensemble=0.2, reverb_mix=0.18, gain=0.6,
          amp_envelope=env(0.001, 0.9, 0.25, 0.3)),
    patch("Dark Organ", oscillator_a=SAW, oscillator_b=SQUARE, oscillator_mix=0.5,
          chorus_mix=0.2, chorus_ensemble=0.15, vibrato_cents=2.0, gain=0.55,
          reverb_mix=0.14, amp_envelope=env(0.005, 0.1, 0.9, 0.08)),
    patch("Clav Pluck", oscillator_a=SQUARE, pulse_width=0.35, pwm_depth=0.3,
          osc_b_ratio=2.0, osc_b_decay=0.06, high_pass=0.2, gain=0.6,
          amp_envelope=env(0.001, 0.12, 0.05, 0.04)),
    patch("Marimba", oscillator_a=SINE, pitch_envelope_semitones=2.0,
          pitch_envelope_decay=0.02, gain=0.7,
          amp_envelope=env(0.001, 0.3, 0.0, 0.1)),
    patch("Vibraphone", oscillator_a=SINE, tremolo_depth=0.45, lfo_rate=5.0,
          gain=0.55, amp_envelope=env(0.002, 1.1, 0.0, 0.5)),
    patch("Stab Brass", oscillator_a=SAW, unison_voices=3, unison_detune_cents=8.0,
          cutoff=0.55, filter_envelope_amount=0.3, gain=0.55,
          amp_envelope=env(0.008, 0.25, 0.7, 0.15)),
    patch("Celesta", oscillator_a=SINE, oscillator_b=TRIANGLE, oscillator_mix=0.3,
          fm_amount=0.2, fm_ratio=4.0, fm_decay=0.6, gain=0.55,
          amp_envelope=env(0.001, 0.9, 0.05, 0.4)),
    patch("Harpsi Pluck", oscillator_a=SAW, osc_b_ratio=2.0, osc_b_decay=0.12,
          key_track_decay=0.50, high_pass=0.3, gain=0.55,
          amp_envelope=env(0.001, 0.15, 0.0, 0.05)),
    patch("Ring Bell", oscillator_a=SINE, ring_mod=0.5, gain=0.5,
          amp_envelope=env(0.002, 1.3, 0.0, 0.6)),
    patch("Piano House", oscillator_a=SAW, oscillator_b=TRIANGLE, oscillator_mix=0.6,
          oscillator_c=SINE, oscillator_c_enabled=True, oscillator_c_mix=0.20,
          osc_c_ratio=4.0, osc_c_decay=0.08,
          chorus_mix=0.2, chorus_ensemble=0.3, reverb_mix=0.16, gain=0.6,
          amp_envelope=env(0.002, 0.35, 0.25, 0.12)),
    # Layered acoustic piano: slow triangle body + fast-decaying string-partial
    # layers (per-osc ratio/decay), hammer transient, key/velocity-scaled ring.
    patch("Acoustic Piano", oscillator_a=TRIANGLE, oscillator_b=SINE,
          osc_b_ratio=3.0, osc_b_decay=0.28, osc_b_level=0.9,
          oscillator_c=SINE, oscillator_c_enabled=True, oscillator_c_mix=0.28,
          osc_c_ratio=4.2, osc_c_decay=0.09, osc_c_level=0.8,
          oscillator_mix=0.30, unison_voices=2, unison_detune_cents=4.0,
          cutoff=0.60, resonance=0.06, filter_keytrack=0.80,
          filter_envelope_amount=0.20, filter_envelope=env(0.001, 0.30, 0.10, 0.20),
          amp_envelope=env(0.001, 2.60, 0.0, 0.25),
          key_track_decay=0.60, velocity_to_decay=0.50, velocity_to_amp=0.92,
          velocity_to_filter=0.45, velocity_curve=3,
          click=0.05, transient_noise=0.10, transient_decay=0.010, low_punch=0.18,
          analog_color=0.10, analog_warmth=0.30, vintage_drift=0.05,
          reverb_mix=0.20, reverb_size=0.75, reverb_decay=0.70, gain=0.62),
]

# ---------------------------------------------------------------------------
# Lead
# ---------------------------------------------------------------------------
LEAD = [
    patch("Saw District Lead", oscillator_a=SAW, unison_voices=3,
          unison_detune_cents=12.0, cutoff=0.6, drive=0.2, gain=0.6,
          amp_envelope=env(0.005, 0.15, 0.75, 0.12)),
    patch("Sync Scream Lead", oscillator_a=SAW, hard_sync=0.5,
          filter_envelope_amount=0.3, cutoff=0.45, resonance=0.35, gain=0.6,
          amp_envelope=env(0.004, 0.18, 0.7, 0.1)),
    patch("Square Mono Lead", oscillator_a=SQUARE, pwm_depth=0.2, cutoff=0.5,
          resonance=0.2, vibrato_cents=5.0, gain=0.6,
          amp_envelope=env(0.004, 0.12, 0.8, 0.1), portamento_time=0.035,
          portamento_legato=True, delay_mix=0.1),
    patch("Fifth Rave Lead", oscillator_a=SAW, oscillator_b=SQUARE, oscillator_mix=0.5,
          detune_cents=700.0, cutoff=0.6, gain=0.55,
          amp_envelope=env(0.003, 0.16, 0.6, 0.1)),
    patch("Dark Mono Lead", oscillator_a=SAW, sub_oscillator=0.3, cutoff=0.4,
          vibrato_cents=6.0, gain=0.6, amp_envelope=env(0.006, 0.2, 0.8, 0.15),
          portamento_time=0.03, portamento_legato=True, delay_mix=0.08),
    patch("Pluck Lead", oscillator_a=SAW, cutoff=0.6, gain=0.6,
          amp_envelope=env(0.001, 0.18, 0.1, 0.06)),
    patch("FM Digital Lead", oscillator_a=SAW, fm_amount=0.6, fm_ratio=2.5,
          gain=0.6, amp_envelope=env(0.002, 0.2, 0.6, 0.12)),
    patch("Rave Hoover", oscillator_a=SAW, unison_voices=5, unison_detune_cents=18.0,
          chorus_mix=0.3, drive=0.3, cutoff=0.55, gain=0.5,
          amp_envelope=env(0.004, 0.2, 0.7, 0.14)),
    patch("Porta Solo Lead", oscillator_a=SAW, vibrato_cents=8.0, gain=0.6,
          amp_envelope=env(0.04, 0.15, 0.85, 0.15), portamento_time=0.07,
          portamento_legato=True, delay_mix=0.14, delay_feedback=0.35,
          reverb_mix=0.16),
    patch("Octave Lead", oscillator_a=SAW, sub_oscillator=0.5, ring_mod=0.1,
          cutoff=0.55, gain=0.6, amp_envelope=env(0.004, 0.16, 0.7, 0.12)),
    patch("Acid Lead", oscillator_a=SAW, cutoff=0.3, resonance=0.5,
          filter_envelope_amount=0.5, gain=0.6,
          amp_envelope=env(0.003, 0.2, 0.5, 0.1), portamento_time=0.04,
          portamento_legato=True),
]

# ---------------------------------------------------------------------------
# Pads
# ---------------------------------------------------------------------------
PADS = [
    patch("Warm Analog Pad", oscillator_a=SAW, oscillator_b=TRIANGLE, oscillator_mix=0.5,
          unison_voices=5, unison_detune_cents=10.0, chorus_mix=0.4, chorus_depth=0.5,
          chorus_ensemble=0.55, cutoff=0.5, gain=0.45, reverb_mix=0.3, reverb_size=0.78,
          reverb_decay=0.75, amp_envelope=env(0.4, 0.5, 0.8, 0.7)),
    patch("Dark Glass Pad", oscillator_a=SINE, oscillator_b=SAW, oscillator_mix=0.3,
          fm_amount=0.15, cutoff=0.45, gain=0.5, chorus_ensemble=0.4,
          reverb_mix=0.32, reverb_size=0.8, amp_envelope=env(0.3, 0.4, 0.75, 0.6)),
    patch("Hollow Pad", oscillator_a=SQUARE, pwm_depth=0.5, cutoff=0.35, gain=0.5,
          chorus_ensemble=0.45, reverb_mix=0.28, reverb_size=0.75,
          amp_envelope=env(0.25, 0.4, 0.7, 0.5)),
    patch("Soundtrack Pad", oscillator_a=SAW, unison_voices=6,
          unison_detune_cents=12.0, stereo_spread=0.7, chorus_mix=0.5,
          chorus_ensemble=0.6, gain=0.4, reverb_mix=0.35, reverb_size=0.85,
          reverb_decay=0.8, amp_envelope=env(0.5, 0.6, 0.85, 0.9)),
    patch("Night Choir", oscillator_a=TRIANGLE, oscillator_b=SAW, oscillator_mix=0.4,
          unison_voices=6, unison_detune_cents=14.0, chorus_mix=0.45,
          chorus_ensemble=0.55, gain=0.4, reverb_mix=0.36, reverb_size=0.85,
          reverb_decay=0.8, amp_envelope=env(0.35, 0.45, 0.8, 0.8)),
    patch("Crystal Pad", oscillator_a=SINE, ring_mod=0.15, fm_amount=0.2,
          fm_ratio=3.0, gain=0.5, reverb_mix=0.3, reverb_size=0.8,
          amp_envelope=env(0.2, 0.5, 0.6, 1.0)),
    patch("Vintage PW Pad", oscillator_a=SQUARE, pwm_depth=0.4, lfo_rate=0.3,
          cutoff=0.4, gain=0.5, chorus_ensemble=0.5, reverb_mix=0.26,
          amp_envelope=env(0.2, 0.4, 0.75, 0.5)),
    patch("Shimmer Pad", oscillator_a=SAW, chorus_mix=0.6, chorus_depth=0.6,
          chorus_ensemble=0.65, gain=0.4, reverb_mix=0.4, reverb_size=0.9,
          reverb_decay=0.85, amp_envelope=env(0.6, 0.5, 0.8, 1.2)),
    patch("Low Drone Pad", oscillator_a=SAW, cutoff=0.2, sub_oscillator=0.4,
          gain=0.45, reverb_mix=0.22, reverb_size=0.7,
          amp_envelope=env(0.8, 0.6, 0.9, 1.0)),
]

# ---------------------------------------------------------------------------
# Strings
# ---------------------------------------------------------------------------
STRINGS = [
    patch("Pizzicato Strings", oscillator_a=SAW, oscillator_b=TRIANGLE,
          oscillator_mix=0.3, unison_voices=3, cutoff=0.55, gain=0.55,
          amp_envelope=env(0.001, 0.18, 0.0, 0.08)),
    patch("Tremolo Strings", oscillator_a=SAW, unison_voices=4, tremolo_depth=0.5,
          lfo_rate=7.0, gain=0.5, amp_envelope=env(0.1, 0.3, 0.75, 0.4)),
    patch("Marcato Strings", oscillator_a=SAW, unison_voices=4, cutoff=0.5,
          gain=0.55, amp_envelope=env(0.02, 0.4, 0.5, 0.25)),
    patch("Octave Strings", oscillator_a=SAW, sub_oscillator=0.4, unison_voices=4,
          gain=0.5, amp_envelope=env(0.08, 0.25, 0.75, 0.5)),
    patch("Chamber Strings", oscillator_a=SAW, oscillator_b=TRIANGLE,
          oscillator_mix=0.5, unison_voices=3, chorus_mix=0.25, chorus_ensemble=0.5,
          gain=0.5, reverb_mix=0.24, reverb_size=0.78,
          amp_envelope=env(0.15, 0.3, 0.7, 0.45)),
    patch("Dark Ensemble", oscillator_a=SAW, cutoff=0.35, unison_voices=5,
          vibrato_cents=3.0, chorus_ensemble=0.5, gain=0.45, reverb_mix=0.26,
          reverb_size=0.8, amp_envelope=env(0.25, 0.4, 0.8, 0.6)),
    patch("Solo Violin Synth", oscillator_a=SAW, vibrato_cents=6.0, chorus_mix=0.15,
          chorus_ensemble=0.3, gain=0.55, reverb_mix=0.2,
          amp_envelope=env(0.08, 0.2, 0.85, 0.3)),
]

# ---------------------------------------------------------------------------
# Arps
# ---------------------------------------------------------------------------
ARPS = [
    patch("Pluck Arp", oscillator_a=SAW, unison_voices=2, unison_detune_cents=6.0,
          cutoff=0.65, gain=0.6, amp_envelope=env(0.001, 0.12, 0.05, 0.04)),
    patch("Acid Arp", oscillator_a=SAW, cutoff=0.3, resonance=0.5,
          filter_envelope_amount=0.5, gain=0.6,
          amp_envelope=env(0.001, 0.15, 0.1, 0.05)),
    patch("Digital Arp", oscillator_a=SQUARE, fm_amount=0.3, fm_ratio=2.0,
          gain=0.55, amp_envelope=env(0.001, 0.1, 0.05, 0.04)),
    patch("Soft Sequence", oscillator_a=TRIANGLE, chorus_mix=0.2, chorus_ensemble=0.2,
          gain=0.6, delay_mix=0.1, amp_envelope=env(0.003, 0.2, 0.15, 0.08)),
    patch("Rave Arp", oscillator_a=SAW, unison_voices=3, unison_detune_cents=12.0,
          cutoff=0.6, gain=0.55, amp_envelope=env(0.001, 0.14, 0.1, 0.05)),
    patch("Dark Sequence", oscillator_a=SAW, cutoff=0.35, sub_oscillator=0.2,
          gain=0.6, amp_envelope=env(0.002, 0.18, 0.2, 0.06)),
    patch("Bounce Arp", oscillator_a=SQUARE, pwm_depth=0.3, gain=0.6,
          amp_envelope=env(0.001, 0.08, 0.05, 0.03)),
]

# ---------------------------------------------------------------------------
# FX
# ---------------------------------------------------------------------------
FX = [
    patch("Noise Riser", oscillator_a=NOISE, noise=0.9, high_pass=0.3,
          chorus_mix=0.3, chorus_ensemble=0.3, gain=0.4, reverb_mix=0.3,
          reverb_size=0.85, amp_envelope=env(1.5, 0.5, 1.0, 1.0)),
    patch("Down Sweep", oscillator_a=SAW, pitch_envelope_semitones=-24.0,
          pitch_envelope_decay=0.8, cutoff=0.6, gain=0.55,
          amp_envelope=env(0.001, 0.8, 0.0, 0.3)),
    patch("Impact Hit", oscillator_a=SINE, pitch_envelope_semitones=36.0,
          pitch_envelope_decay=0.1, noise=0.3, gain=0.8,
          amp_envelope=env(0.001, 0.6, 0.0, 0.3)),
    patch("Zap Laser", oscillator_a=SQUARE, pitch_envelope_semitones=-36.0,
          pitch_envelope_decay=0.15, gain=0.55,
          amp_envelope=env(0.001, 0.15, 0.0, 0.05)),
    patch("Siren", oscillator_a=SINE, vibrato_cents=100.0, lfo_rate=0.6,
          gain=0.55, amp_envelope=env(0.3, 0.2, 1.0, 0.5)),
    patch("Wind", oscillator_a=NOISE, noise=0.8, noise_tone=0.4, high_pass=0.15,
          chorus_mix=0.4, gain=0.35, amp_envelope=env(1.0, 0.6, 0.8, 1.0)),
    patch("Metallic Hit", oscillator_a=SQUARE, ring_mod=0.6, noise=0.3,
          gain=0.5, amp_envelope=env(0.001, 0.4, 0.0, 0.2)),
    patch("Sub Drop", oscillator_a=SINE, pitch_envelope_semitones=-18.0,
          pitch_envelope_decay=0.5, gain=0.8,
          amp_envelope=env(0.001, 0.5, 0.0, 0.2)),
    patch("Radio Static", oscillator_a=NOISE, noise=0.9, bit_crush=0.2,
          sample_rate_reduction=0.3, gain=0.35,
          amp_envelope=env(0.005, 0.3, 0.7, 0.3)),
    patch("Alarm Pulse", oscillator_a=SQUARE, tremolo_depth=0.6, lfo_rate=8.0,
          gain=0.5, amp_envelope=env(0.005, 0.2, 0.7, 0.2)),
]

# Category folder -> (first free index, patches). Indices continue after the
# renamed C++ demo exports that already live in each folder.
LIBRARY = {
    "bass": (4, BASS),
    "drums": (8, DRUMS),
    "keys": (3, KEYS),
    "lead": (2, LEAD),
    "pads": (4, PADS),
    "strings": (4, STRINGS),
    "arps": (2, ARPS),
    "fx": (1, FX),
}


def generate() -> int:
    written = 0
    for folder, (first_index, patches) in LIBRARY.items():
        target_dir = FACTORY_DIR / folder
        target_dir.mkdir(parents=True, exist_ok=True)
        for offset, item in enumerate(patches):
            filename = f"{first_index + offset:02d}_{slug(item.name)}.arachnopatch"
            at.save_patch(apply_category_color(item, folder), target_dir / filename)
            written += 1
    print(f"wrote {written} factory patches under {FACTORY_DIR}")
    return 0


def check() -> int:
    failures = 0
    total = 0
    for path in sorted(FACTORY_DIR.rglob("*.arachnopatch")):
        total += 1
        try:
            at.load_patch(path)
        except Exception as error:  # noqa: BLE001 - report every bad file
            failures += 1
            print(f"FAIL {path}: {error}")
    print(f"checked {total} patches, {failures} failures")
    return 1 if failures else 0


def main() -> int:
    if "--check" in sys.argv:
        return check()
    return generate()


if __name__ == "__main__":
    sys.exit(main())
