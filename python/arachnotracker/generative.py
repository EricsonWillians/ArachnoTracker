"""Composable helpers for algorithmic pattern generation."""

from __future__ import annotations

from typing import Iterable, List, Sequence

from .model import Pattern, Step
from .notes import note_name_to_midi

SCALES = {
    "major": [0, 2, 4, 5, 7, 9, 11],
    "minor": [0, 2, 3, 5, 7, 8, 10],
    "dorian": [0, 2, 3, 5, 7, 9, 10],
    "phrygian": [0, 1, 3, 5, 7, 8, 10],
    "lydian": [0, 2, 4, 6, 7, 9, 11],
    "mixolydian": [0, 2, 4, 5, 7, 9, 10],
    "locrian": [0, 1, 3, 5, 6, 8, 10],
    "pentatonic": [0, 2, 4, 7, 9],
    "minor-pentatonic": [0, 3, 5, 7, 10],
    "chromatic": list(range(12)),
}


def scale_degree(root_midi: int, scale: str, degree: int) -> int:
    """Return the MIDI note for a scale degree above ``root_midi``."""

    intervals = SCALES.get(scale.lower())
    if intervals is None:
        raise ValueError(f"unknown scale: {scale}")
    octave, index = divmod(degree, len(intervals))
    return root_midi + octave * 12 + intervals[index]


def scale_notes(root: str | int, scale: str, degrees: Iterable[int]) -> List[int]:
    """Convert scale degrees to MIDI notes from a root note name or MIDI value."""

    root_midi = note_name_to_midi(root) if isinstance(root, str) else root
    return [scale_degree(root_midi, scale, degree) for degree in degrees]


def euclidean_hits(steps: int, pulses: int) -> List[bool]:
    """Return a simple Euclidean rhythm mask with ``pulses`` distributed over ``steps``."""

    if steps <= 0:
        raise ValueError("steps must be positive")
    if pulses <= 0 or pulses > steps:
        raise ValueError("pulses must be between 1 and steps")
    return [((index * pulses) % steps) < pulses for index in range(steps)]


def place_note(
    pattern: Pattern,
    row: int,
    track: int,
    note: str | int,
    instrument: int,
    *,
    velocity: float = 1.0,
    gate: float = 0.88,
    probability: float | None = None,
    retrigger_count: int = 1,
    retrigger_spacing_rows: float = 0.25,
    retrigger_velocity_decay: float = 0.85,
) -> Step:
    """Place a note in a pattern and return the edited step."""

    step = pattern.step(row, track)
    step.midi = note_name_to_midi(note) if isinstance(note, str) else note
    step.velocity = velocity
    step.instrument = instrument
    step.gate = gate
    step.probability = probability
    step.retrigger_count = retrigger_count
    step.retrigger_spacing_rows = retrigger_spacing_rows
    step.retrigger_velocity_decay = retrigger_velocity_decay
    return step


def fill_scale(
    pattern: Pattern,
    track: int,
    start_row: int,
    count: int,
    stride: int,
    root: str | int,
    scale: str,
    instrument: int,
    *,
    degrees: Sequence[int] | None = None,
    velocity: float = 0.8,
    gate: float = 0.8,
) -> None:
    """Fill a melodic line by walking through scale degrees."""

    if count <= 0:
        raise ValueError("count must be positive")
    if stride <= 0:
        raise ValueError("stride must be positive")
    degree_list = list(range(count)) if degrees is None else list(degrees)
    if not degree_list:
        raise ValueError("degrees must not be empty")
    notes = scale_notes(root, scale, degree_list)
    for index in range(count):
        row = start_row + index * stride
        if row >= pattern.rows:
            break
        place_note(pattern, row, track, notes[index % len(notes)], instrument, velocity=velocity, gate=gate)


def fill_euclidean(
    pattern: Pattern,
    track: int,
    start_row: int,
    steps: int,
    pulses: int,
    note: str | int,
    instrument: int,
    *,
    velocity: float = 0.8,
    gate: float = 0.6,
    probability: float | None = None,
) -> None:
    """Place a repeated note using a Euclidean rhythm mask."""

    for offset, hit in enumerate(euclidean_hits(steps, pulses)):
        row = start_row + offset
        if row >= pattern.rows:
            break
        if hit:
            place_note(
                pattern,
                row,
                track,
                note,
                instrument,
                velocity=velocity,
                gate=gate,
                probability=probability,
            )
