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

    @property
    def empty(self) -> bool:
        return self.midi is None and not self.automation


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
