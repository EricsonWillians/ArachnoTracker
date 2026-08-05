from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Dict, List, Optional


class Waveform(str, Enum):
    """Oscillator shapes supported by the native synthesizer."""

    SINE = "sine"
    SQUARE = "square"
    SAW = "saw"
    TRIANGLE = "triangle"
    NOISE = "noise"
    SUPERSAW = "supersaw"


@dataclass
class Envelope:
    """ADSR envelope in seconds plus normalized sustain level."""

    attack: float = 0.005
    decay: float = 0.08
    sustain: float = 0.72
    release: float = 0.18


@dataclass
class SynthPatch:
    """Serializable native synthesizer patch.

    Fields map directly to ArachnoTracker's built-in synth and patch files, so
    generated patches can be rendered without external plugin dependencies.
    """

    name: str = "Init"
    oscillator_a: Waveform = Waveform.SAW
    oscillator_b: Waveform = Waveform.SQUARE
    oscillator_mix: float = 0.35
    oscillator_b_enabled: bool = True
    detune_cents: float = 7.0
    pulse_width: float = 0.5
    pwm_depth: float = 0.0
    fm_amount: float = 0.0
    fm_ratio: float = 2.0
    chorus_mix: float = 0.0
    chorus_rate: float = 0.35
    chorus_depth: float = 0.25
    unison_voices: int = 1
    unison_detune_cents: float = 0.0
    stereo_spread: float = 0.0
    sub_oscillator: float = 0.18
    noise: float = 0.02
    noise_tone: float = 0.58
    cutoff: float = 0.72
    resonance: float = 0.12
    filter_envelope_amount: float = 0.18
    pitch_envelope_semitones: float = 0.0
    pitch_envelope_decay: float = 0.08
    lfo_rate: float = 5.5
    vibrato_cents: float = 0.0
    tremolo_depth: float = 0.0
    ring_mod: float = 0.0
    hard_sync: float = 0.0
    drive: float = 0.08
    bit_crush: float = 0.0
    sample_rate_reduction: float = 0.0
    high_pass: float = 0.0
    click: float = 0.0
    transient_shape: float = 0.0
    transient_noise: float = 0.0
    transient_pitch_semitones: float = 0.0
    transient_pitch_decay: float = 0.01
    transient_burst_count: int = 1
    transient_burst_spacing: float = 0.004
    transient_burst_decay: float = 0.7
    transient_tone: float = 0.72
    transient_decay: float = 0.012
    gain: float = 0.55
    pan: float = 0.0
    # Extended engine fields (emitted in the modern 148-value layout; defaults
    # mirror the C++ SynthPatch so unedited fields round-trip unchanged).
    filter_drive: float = 0.15
    filter_keytrack: float = 0.45
    chorus_ensemble: float = 0.0
    delay_mix: float = 0.06
    delay_time: float = 0.24
    delay_feedback: float = 0.36
    reverb_mix: float = 0.10
    reverb_size: float = 0.68
    reverb_damping: float = 0.44
    reverb_decay: float = 0.74
    portamento_time: float = 0.0
    portamento_legato: bool = False
    fm_decay: float = 0.0
    velocity_to_fm: float = 0.0
    mono_mode: bool = False
    # Oscillator C/D and per-oscillator shape controls
    oscillator_c: Waveform = Waveform.SINE
    oscillator_d: Waveform = Waveform.SINE
    oscillator_c_enabled: bool = False
    oscillator_d_enabled: bool = False
    oscillator_c_mix: float = 0.0
    oscillator_d_mix: float = 0.0
    detune_c_cents: float = -7.0
    detune_d_cents: float = 12.0
    osc_a_level: float = 1.0
    osc_b_level: float = 1.0
    osc_c_level: float = 1.0
    osc_d_level: float = 1.0
    osc_a_detune_cents: float = 0.0
    osc_b_detune_cents: float = 0.0
    osc_c_detune_cents: float = 0.0
    osc_d_detune_cents: float = 0.0
    osc_a_pulse_width: float = 0.5
    osc_b_pulse_width: float = 0.5
    osc_c_pulse_width: float = 0.5
    osc_d_pulse_width: float = 0.5
    osc_a_pwm_depth: float = 0.0
    osc_b_pwm_depth: float = 0.0
    osc_c_pwm_depth: float = 0.0
    osc_d_pwm_depth: float = 0.0
    osc_a_drive: float = 0.0
    osc_b_drive: float = 0.0
    osc_c_drive: float = 0.0
    osc_d_drive: float = 0.0
    # Layered timbre: per-osc frequency ratios and 1-pole decay layers
    osc_b_ratio: float = 1.0
    osc_c_ratio: float = 1.0
    osc_d_ratio: float = 1.0
    osc_b_decay: float = 0.0
    osc_c_decay: float = 0.0
    osc_d_decay: float = 0.0
    velocity_to_decay: float = 0.0
    key_track_decay: float = 0.0
    # Filter / FM / drive extras
    filter_mode: int = 0
    lfo_filter_depth: float = 0.0
    lfo_pan_depth: float = 0.0
    fm_feedback: float = 0.0
    fm_algorithm: int = 0
    fm_color: float = 0.5
    fm_spread: float = 0.0
    wavefold: float = 0.0
    comb_mix: float = 0.0
    comb_time: float = 0.08
    comb_feedback: float = 0.15
    # Delay extras
    delay_tone: float = 0.54
    delay_stereo: float = 0.34
    delay_mod_depth: float = 0.24
    delay_drive: float = 0.02
    delay_ducking: float = 0.0
    delay_diffusion: float = 0.24
    delay_wow: float = 0.22
    delay_crossfeed: float = 0.28
    # Reverb extras
    reverb_pre_delay: float = 0.06
    reverb_diffusion: float = 0.62
    reverb_width: float = 0.65
    reverb_shimmer: float = 0.08
    reverb_mod_depth: float = 0.08
    reverb_early_mix: float = 0.24
    reverb_tone: float = 0.54
    reverb_chorus: float = 0.20
    reverb_bloom: float = 0.26
    # Chorus extras
    chorus_feedback: float = 0.12
    chorus_delay: float = 0.42
    chorus_width: float = 0.55
    chorus_tone: float = 0.58
    chorus_jitter: float = 0.22
    chorus_saturation: float = 0.26
    # Color / character (defaults = previously hardcoded values)
    analog_color: float = 0.45
    vintage_drift: float = 0.35
    wow_flutter: float = 0.08
    tone_tilt: float = -0.05
    tape_color: float = 0.12
    air_boost: float = 0.18
    low_punch: float = 0.08
    analog_warmth: float = 0.62
    voice_slop: float = 0.28
    phase_scatter: float = 0.24
    unison_warp: float = 0.18
    unison_humanize: float = 0.22
    console_crosstalk: float = 0.06
    stereo_depth: float = 0.22
    hifi_exciter: float = 0.16
    output_transformer: float = 0.18
    output_soft_clip: float = 0.20
    output_glue: float = 0.22
    # Velocity expression
    velocity_to_amp: float = 0.85
    velocity_to_filter: float = 0.2
    velocity_to_attack: float = 0.15
    velocity_curve: int = 1
    filter_keytrack_resonance: float = 0.0
    filter_nonlinearity: float = 0.25
    amp_envelope_curve: int = 0
    filter_envelope_curve: int = 0
    amp_envelope: Envelope = field(default_factory=Envelope)
    filter_envelope: Envelope = field(default_factory=Envelope)


@dataclass
class Track:
    """Mixer lane metadata for one tracker track."""

    name: str = "Track"
    volume: float = 0.85
    pan: float = 0.0
    muted: bool = False
    solo: bool = False


@dataclass
class Step:
    """One pattern cell containing an optional note and per-step controls."""

    midi: Optional[int] = None
    velocity: float = 1.0
    instrument: int = 0
    gate: float = 0.88
    micro_offset_rows: float = 0.0
    probability: Optional[float] = None
    retrigger_count: int = 1
    retrigger_spacing_rows: float = 0.25
    retrigger_velocity_decay: float = 0.85
    automation: Dict[str, float] = field(default_factory=dict)
    # Note-off step: releases the last note started on this track (shown as "===").
    note_off: bool = False

    @property
    def empty(self) -> bool:
        return self.midi is None and not self.note_off and not self.automation


@dataclass
class Pattern:
    """Tracker pattern grid addressed by ``(row, track)`` coordinates."""

    name: str = "Pattern"
    rows: int = 64
    tracks: int = 8
    steps: Dict[tuple[int, int], Step] = field(default_factory=dict)

    def step(self, row: int, track: int) -> Step:
        """Return the editable step at ``row`` and ``track``, creating it if needed."""

        if row < 0 or row >= self.rows:
            raise IndexError("row is out of range")
        if track < 0 or track >= self.tracks:
            raise IndexError("track is out of range")
        key = (row, track)
        if key not in self.steps:
            self.steps[key] = Step()
        return self.steps[key]


@dataclass
class Song:
    """Top-level project model for writing native ``.arachno`` files."""

    title: str = "Untitled"
    author: str = ""
    description: str = ""
    notes: str = ""
    bpm: float = 138.0
    rows_per_beat: int = 4
    sample_rate: int = 48000
    tracks: List[Track] = field(default_factory=list)
    instruments: List[SynthPatch] = field(default_factory=list)
    patterns: List[Pattern] = field(default_factory=list)
    order: List[int] = field(default_factory=list)

    def add_track(self, name: str, *, volume: float = 0.85, pan: float = 0.0) -> int:
        """Append a track and return its index."""

        self.tracks.append(Track(name=name, volume=volume, pan=pan))
        return len(self.tracks) - 1

    def add_instrument(self, patch: SynthPatch) -> int:
        """Append a synth patch and return its instrument index."""

        self.instruments.append(patch)
        return len(self.instruments) - 1

    def add_pattern(self, pattern: Pattern) -> int:
        """Append a pattern and return its index."""

        self.patterns.append(pattern)
        return len(self.patterns) - 1
