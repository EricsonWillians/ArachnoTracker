from __future__ import annotations

from . import generative as gen
from .model import Pattern, Song
from . import presets


def darkwave_ebm_starter(title: str = "Python EBM Starter", *, bpm: float = 132.0) -> Song:
    song = Song(
        title=title,
        author="ArachnoTracker Python SDK",
        description="Generated darkwave/EBM starter project.",
        bpm=bpm,
        rows_per_beat=4,
    )

    drums = song.add_track("Drums", volume=0.95)
    bass = song.add_track("Bass", volume=0.88)
    lead = song.add_track("Lead", volume=0.7, pan=-0.12)
    pad = song.add_track("Pad", volume=0.52, pan=0.18)

    kick = song.add_instrument(presets.ebm_kick())
    snare = song.add_instrument(presets.gated_snare())
    hat = song.add_instrument(presets.metal_hat())
    bass_patch = song.add_instrument(presets.low_saw())
    lead_patch = song.add_instrument(presets.bright_twin())
    pad_patch = song.add_instrument(presets.soft_wide())

    pattern = Pattern("Python Opening", rows=64, tracks=len(song.tracks))

    for row in range(0, 64, 4):
        gen.place_note(
            pattern,
            row,
            drums,
            "C2",
            kick,
            velocity=1.0 if row % 16 == 0 else 0.82,
            gate=0.32,
        )

    for row in range(8, 64, 16):
        gen.place_note(
            pattern,
            row,
            drums,
            "E2",
            snare,
            velocity=0.82,
            gate=0.55,
            retrigger_count=2,
            retrigger_spacing_rows=0.18,
            retrigger_velocity_decay=0.55,
        )

    for row in range(2, 64, 4):
        gen.place_note(
            pattern,
            row,
            drums,
            "C5",
            hat,
            velocity=0.46 if row % 8 == 2 else 0.34,
            gate=0.18,
            probability=0.62 if row % 16 == 14 else None,
            retrigger_count=3 if row % 16 == 14 else 1,
            retrigger_spacing_rows=0.12,
            retrigger_velocity_decay=0.72,
        )

    bassline = ["C2", "C2", "G2", "F2", "C2", "C3", "G2", "A#1"]
    for index, note in enumerate(bassline):
        step = gen.place_note(pattern, index * 8, bass, note, bass_patch, velocity=0.9, gate=0.72)
        step.automation["cutoff"] = 0.34 + 0.08 * (index % 2)

    gen.fill_scale(
        pattern,
        lead,
        0,
        11,
        6,
        "C4",
        "minor",
        lead_patch,
        degrees=[0, 2, 4, 6, 7, 6, 4, 2, 3, 4, 7],
        velocity=0.68,
        gate=0.58,
    )

    for row, note in ((0, "C3"), (1, "G3"), (32, "F2"), (33, "C3")):
        gen.place_note(pattern, row, pad, note, pad_patch, velocity=0.46, gate=3.4)

    song.add_pattern(pattern)
    song.order = [0, 0]
    return song
