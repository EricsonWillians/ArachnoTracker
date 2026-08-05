#!/usr/bin/env python3
"""Generate ArachnoTracker's goth example projects (Darkwave / EBM / Industrial).

Composes the six showcase ``.arachno`` projects in ``examples/projects/`` from
the curated factory patch library. Run from the repository root:

    PYTHONPATH=python python3 examples/scripts/generate_example_projects.py

The arrangements are hand-composed (progressions, motifs, drum maps) with
generative helpers only where repetition is musical (euclidean percussion,
arp walks). Everything renders with the native engine — no external plugins.
"""

from __future__ import annotations

from pathlib import Path

from arachnotracker.generative import euclidean_hits, place_note
from arachnotracker.io import load_patch, save_project
from arachnotracker.model import Pattern, Song
from arachnotracker.notes import note_name_to_midi

ROOT = Path(__file__).resolve().parent.parent.parent
FACTORY = ROOT / "patches" / "factory"
OUT = ROOT / "examples" / "projects"

ROWS = 64  # 4 bars of 16 steps


def patch(rel: str) -> object:
    return load_patch(FACTORY / rel)


def pattern(song: Song, name: str) -> Pattern:
    p = Pattern(name=name, rows=ROWS, tracks=len(song.tracks))
    song.add_pattern(p)
    return p


def note_off(pattern_obj: Pattern, row: int, track: int) -> None:
    pattern_obj.step(row, track).note_off = True


# ---------------------------------------------------------------------------
# 1) Darkwave Foundation — D minor, 118 BPM
# ---------------------------------------------------------------------------

def darkwave_foundation() -> Song:
    song = Song(
        title="Darkwave Foundation",
        author="ArachnoTracker",
        description="Cathedral-dark darkwave: driving minor bass, choir pads, 808/909 kit, mono lead with glide.",
        notes="D minor | i-VI-III-VII | showcase for mono basses, choir pads and portamento leads.",
        bpm=118.0,
    )
    t_kick = song.add_track("Kick 909", volume=1.1)
    t_snare = song.add_track("Gated Snare", volume=0.95, pan=0.05)
    t_hat = song.add_track("Hats", volume=0.7, pan=-0.15)
    t_bass = song.add_track("Dark Bass", volume=0.9)
    t_pad = song.add_track("Night Choir", volume=0.5, pan=0.2)
    t_arp = song.add_track("Dark Arp", volume=0.55, pan=-0.25)
    t_lead = song.add_track("Mono Lead", volume=0.7, pan=0.1)
    t_str = song.add_track("Dark Ensemble", volume=0.45, pan=-0.1)

    k909 = patch("drums/09_kick_909.arachnopatch"); k909.gain *= 1.15
    s909 = patch("drums/10_snare_909.arachnopatch"); s909.gain *= 1.25
    h808c = patch("drums/12_hat_closed_808.arachnopatch"); h808c.gain *= 1.25
    h808o = patch("drums/13_hat_open_808.arachnopatch"); h808o.gain *= 1.25
    i_kick = song.add_instrument(k909)
    i_snare = song.add_instrument(s909)
    i_hat_c = song.add_instrument(h808c)
    i_hat_o = song.add_instrument(h808o)
    bass = patch("bass/08_dark_pluck_bass.arachnopatch")
    bass.mono_mode = True
    bass.portamento_time = 0.03
    i_bass = song.add_instrument(bass)
    i_pad = song.add_instrument(patch("pads/08_night_choir.arachnopatch"))
    i_arp = song.add_instrument(patch("arps/07_dark_sequence.arachnopatch"))
    lead = patch("lead/06_dark_mono_lead.arachnopatch")
    lead.mono_mode = True
    lead.portamento_time = 0.05
    lead.portamento_legato = True
    i_lead = song.add_instrument(lead)
    i_str = song.add_instrument(patch("strings/09_dark_ensemble.arachnopatch"))

    D = note_name_to_midi("D2")
    # i - VI - III - VII in D natural minor: Dm, Bb, F, C (roots as midi)
    prog_roots = [note_name_to_midi(n) for n in ("D2", "Bb1", "F2", "C2")]
    minor = [0, 2, 3, 5, 7, 8, 10]

    def chord_tones(root: int) -> list[int]:
        return [root + 12, root + 15, root + 19, root + 22]  # minor triad + 7th color

    def drums(p: Pattern, *, fills: bool = False, open_hats: bool = False) -> None:
        for row in range(0, ROWS, 4):  # four-on-the-floor
            place_note(p, row, t_kick, "D1", i_kick, velocity=1.0 if row % 16 == 0 else 0.92, gate=2.5)
        for row in range(8, ROWS, 16):  # snare on 3 of each bar
            place_note(p, row, t_snare, "D2", i_snare, velocity=0.95, gate=2.5)
        for row in range(0, ROWS, 2):  # 8th hats
            if row % 8 == 6 and open_hats:
                place_note(p, row, t_hat, "F#2", i_hat_o, velocity=0.7, gate=3.0)
            else:
                vel = 0.78 if row % 4 == 2 else 0.55
                place_note(p, row, t_hat, "F#2", i_hat_c, velocity=vel, gate=1.0, probability=0.95)
        if fills:
            for i, r in enumerate(range(56, 64, 2)):  # last-bar snare build
                place_note(p, r, t_snare, "D2", i_snare, velocity=0.6 + i * 0.06, gate=1.2,
                           retrigger_count=2, retrigger_spacing_rows=0.5)

    def bass_line(p: Pattern, *, energetic: bool) -> None:
        for bar in range(4):
            root = prog_roots[bar]
            base = bar * 16
            if energetic:
                seq = [0, 0, 12, 0, 0, 7, 0, 12, 0, 0, 10, 0, 7, 0, 12, 0]
            else:
                seq = [0, None, 0, None, 0, None, 7, None, 0, None, 0, None, 10, None, 7, None]
            for i, semi in enumerate(seq):
                if semi is None:
                    continue
                place_note(p, base + i, t_bass, root + semi, i_bass,
                           velocity=0.9 if i % 4 == 0 else 0.75, gate=1.8)

    def pads(p: Pattern, track: int, inst: int) -> None:
        for bar in range(4):
            tones = chord_tones(prog_roots[bar])
            for j, m in enumerate(tones):
                place_note(p, bar * 16, track, m + 12, inst, velocity=0.6 - j * 0.04, gate=15.5)

    def arp_line(p: Pattern) -> None:
        for bar in range(4):
            tones = chord_tones(prog_roots[bar]) + [prog_roots[bar] + 24]
            walk = [0, 1, 2, 3, 4, 3, 2, 1, 0, 2, 1, 3, 2, 4, 3, 1]
            for i, w in enumerate(walk):
                place_note(p, bar * 16 + i, t_arp, tones[w] + 12, i_arp,
                           velocity=0.55 if i % 2 else 0.72, gate=0.95)

    def lead_motif(p: Pattern) -> None:
        # Call-and-answer minor motif over bars 2-4 (D5 area), legato slides.
        motif = [
            (16, "D5", 3.0), (20, "F5", 1.5), (22, "E5", 1.5),
            (24, "C5", 3.0), (28, "D5", 3.0),
            (32, "A4", 2.0), (34, "C5", 2.0), (36, "D5", 2.0), (38, "F5", 2.0),
            (40, "G5", 4.0), (44, "F5", 2.0), (46, "E5", 2.0),
            (48, "D5", 6.0),
        ]
        for row, n, g in motif:
            place_note(p, row, t_lead, n, i_lead, velocity=0.85, gate=g)
        note_off(p, 56, t_lead)

    def strings_hold(p: Pattern) -> None:
        for bar in range(4):
            tones = chord_tones(prog_roots[bar])
            for j, m in enumerate(tones[:3]):
                place_note(p, bar * 16, t_str, m + 24, i_str, velocity=0.5 - j * 0.05, gate=15.5)

    # --- Arrangement ---
    p_intro = pattern(song, "Intro — pads + arp")
    pads(p_intro, t_pad, i_pad)
    arp_line(p_intro)

    p_verse = pattern(song, "Verse — full groove")
    drums(p_verse)
    bass_line(p_verse, energetic=False)
    pads(p_verse, t_pad, i_pad)
    arp_line(p_verse)

    p_verse2 = pattern(song, "Verse 2 — energetic bass")
    drums(p_verse2, open_hats=True)
    bass_line(p_verse2, energetic=True)
    pads(p_verse2, t_pad, i_pad)
    arp_line(p_verse2)

    p_chorus = pattern(song, "Chorus — lead motif")
    drums(p_chorus, fills=True, open_hats=True)
    bass_line(p_chorus, energetic=True)
    pads(p_chorus, t_pad, i_pad)
    lead_motif(p_chorus)
    strings_hold(p_chorus)

    p_break = pattern(song, "Break — strings + choir")
    strings_hold(p_break)
    pads(p_break, t_pad, i_pad)
    # heartbeat kick every bar
    for bar in range(4):
        place_note(p_break, bar * 16, t_kick, "D1", i_kick, velocity=0.85, gate=2.5)
        place_note(p_break, bar * 16 + 10, t_bass, prog_roots[bar], i_bass, velocity=0.7, gate=2.0)

    song.order = [p_intro_idx for p_intro_idx in (
        0, 0, 1, 1, 2, 3, 3, 2, 3, 4, 1, 2, 3, 3
    )]
    return song


# ---------------------------------------------------------------------------
# 2) Factory Pulse — EBM, A minor, 130 BPM
# ---------------------------------------------------------------------------

def factory_pulse() -> Song:
    song = Song(
        title="Factory Pulse",
        author="ArachnoTracker",
        description="Relentless EBM body music: sequenced mono bass, iron kick, gated snare, metal percussion.",
        notes="A minor | machine funk | probability hats, retrig snares, mono sequencer bass.",
        bpm=130.0,
    )
    t_kick = song.add_track("Iron Kick", volume=1.25)
    t_snare = song.add_track("Steel Snare", volume=1.05, pan=0.04)
    t_hat = song.add_track("Razor Hats", volume=0.8, pan=-0.18)
    t_rim = song.add_track("Anvil Rim", volume=0.9, pan=0.3)
    t_bass = song.add_track("Drive Bass", volume=1.0)
    t_stab = song.add_track("Body Stabs", volume=0.9, pan=-0.1)
    t_seq = song.add_track("Acid Sequence", volume=0.65, pan=0.22)
    t_fx = song.add_track("Factory FX", volume=0.75, pan=0.0)

    iron = patch("drums/01_ebm_iron_kick.arachnopatch"); iron.gain *= 1.25
    steel = patch("drums/02_steel_snare.arachnopatch"); steel.gain *= 1.3
    razor = patch("drums/03_ebm_razor_hat.arachnopatch"); razor.gain *= 1.35
    anvil = patch("drums/07_anvil_rim.arachnopatch"); anvil.gain *= 1.5
    i_kick = song.add_instrument(iron)
    i_snare = song.add_instrument(steel)
    i_hat = song.add_instrument(razor)
    i_rim = song.add_instrument(anvil)
    bass = patch("bass/11_industrial_drive_bass.arachnopatch")
    bass.mono_mode = True
    i_bass = song.add_instrument(bass)
    stab_p = patch("keys/01_body_stab.arachnopatch"); stab_p.gain *= 1.35
    acid_p = patch("arps/03_acid_arp.arachnopatch"); acid_p.gain *= 1.2
    i_stab = song.add_instrument(stab_p)
    i_seq = song.add_instrument(acid_p)
    i_riser = song.add_instrument(patch("fx/01_noise_riser.arachnopatch"))
    i_impact = song.add_instrument(patch("fx/03_impact_hit.arachnopatch"))

    # EBM bass sequence in A minor: root-pulse with 6th/7th motion
    bass_seq = [0, 0, None, 0, None, 0, 0, None, 0, None, 0, 0, None, 0, None, 0]
    bass_notes = [0, 0, None, 12, None, 0, 10, None, 0, None, 7, 0, None, 10, None, 12]

    def groove(p: Pattern, *, variation: int = 0) -> None:
        for row in range(0, ROWS, 4):
            place_note(p, row, t_kick, "A1", i_kick, velocity=1.0, gate=2.5)
        # off-beat ghost kick
        for row in range(14, ROWS, 16):
            place_note(p, row, t_kick, "A1", i_kick, velocity=0.55, gate=1.5, probability=0.8)
        for row in range(8, ROWS, 16):
            place_note(p, row, t_snare, "D2", i_snare, velocity=0.95, gate=2.5)
        # 16th razor hats with accents + probability
        for row in range(ROWS):
            if row % 4 == 0:
                continue
            vel = 0.85 if row % 4 == 2 else 0.6
            prob = 0.9 if row % 2 else 0.7
            place_note(p, row, t_hat, "F#2", i_hat, velocity=vel, gate=1.0, probability=prob)
        # anvil rim: euclidean 5-in-16 metallic answer
        for row, hit in enumerate(euclidean_hits(16, 5) * 4):
            if hit:
                place_note(p, row, t_rim, "G2", i_rim, velocity=0.85, gate=1.5, probability=0.85)
        # mono bass sequence
        for bar in range(4):
            root = note_name_to_midi("A1") + [0, 0, -2, 3][bar]  # A A G C
            for i, semi in enumerate(bass_notes):
                if semi is None or bass_seq[i] is None:
                    continue
                v = 0.95 if i % 8 == 0 else 0.8
                if variation and bar == 3 and i >= 12:
                    place_note(p, bar * 16 + i, t_bass, root + semi + 12, i_bass, velocity=v, gate=1.0,
                               retrigger_count=2, retrigger_spacing_rows=0.25)
                else:
                    place_note(p, bar * 16 + i, t_bass, root + semi, i_bass, velocity=v, gate=1.4)

    def stabs(p: Pattern, *, dense: bool) -> None:
        hits = [4, 12, 20, 28, 36, 44, 52, 60] if dense else [4, 20, 36, 52]
        chord = ["A3", "C4", "E4"]
        for r in hits:
            for j, n in enumerate(chord):
                place_note(p, r, t_stab, n, i_stab, velocity=0.9 - j * 0.08, gate=2.0)

    def acid(p: Pattern) -> None:
        walk = [0, 3, 7, 10, 12, 10, 7, 3, 0, 3, 7, 10, 15, 12, 10, 7]
        for bar in range(4):
            root = note_name_to_midi("A3") + [0, 0, -2, 3][bar]
            for i, semi in enumerate(walk):
                place_note(p, bar * 16 + i, t_seq, root + semi, i_seq,
                           velocity=0.65 if i % 2 else 0.8, gate=0.9, probability=0.92)

    def fx_sweeps(p: Pattern, *, impact: bool) -> None:
        if impact:
            place_note(p, 0, t_fx, "A2", i_impact, velocity=0.9, gate=6.0)
        place_note(p, 48, t_fx, "A4", i_riser, velocity=0.55, gate=15.0)

    p_groove = pattern(song, "Main groove")
    groove(p_groove)
    stabs(p_groove, dense=False)

    p_full = pattern(song, "Full assault")
    groove(p_full, variation=1)
    stabs(p_full, dense=True)
    acid(p_full)

    p_break = pattern(song, "Breakdown")
    for r in range(0, ROWS, 8):
        place_note(p_break, r, t_kick, "A1", i_kick, velocity=0.9, gate=0.4)
    for bar in range(4):
        root = note_name_to_midi("A1") + [0, 0, -2, 3][bar]
        place_note(p_break, bar * 16, t_bass, root, i_bass, velocity=0.85, gate=6.0)
        place_note(p_break, bar * 16 + 8, t_bass, root + 7, i_bass, velocity=0.7, gate=4.0)
    stabs(p_break, dense=False)
    fx_sweeps(p_break, impact=False)

    p_intro = pattern(song, "Intro build")
    for r in range(0, ROWS, 4):
        place_note(p_intro, r, t_kick, "A1", i_kick, velocity=0.7 + r / ROWS * 0.3, gate=2.5)
    for row in range(0, ROWS, 2):
        place_note(p_intro, row, t_hat, "F#2", i_hat, velocity=0.3 + row / ROWS * 0.4, gate=1.0)
    acid(p_intro)
    fx_sweeps(p_intro, impact=True)

    song.order = [3, 0, 0, 1, 2, 2, 0, 1, 1, 2, 1, 1, 2]
    return song


# ---------------------------------------------------------------------------
# 3) Night Drive — melodic dark synthwave, F# minor, 96 BPM
# ---------------------------------------------------------------------------

def night_drive() -> Song:
    song = Song(
        title="Night Drive",
        author="ArachnoTracker",
        description="Rain-on-chrome melodic darkwave: FM keys, warm pads, 808 kit, gliding solo lead.",
        notes="F# minor | i-VI-iv-V | slow-burn night music with portamento lead and FM bells.",
        bpm=96.0,
    )
    t_kick = song.add_track("808 Kick", volume=0.9)
    t_snare = song.add_track("808 Snare", volume=0.7, pan=0.05)
    t_hat = song.add_track("808 Hats", volume=0.5, pan=-0.2)
    t_bass = song.add_track("Sub Bass", volume=0.85)
    t_keys = song.add_track("FM Keys", volume=0.6, pan=-0.15)
    t_pad = song.add_track("Warm Pad", volume=0.5, pan=0.2)
    t_bell = song.add_track("Glass Bell", volume=0.45, pan=0.3)
    t_lead = song.add_track("Solo Lead", volume=0.7, pan=0.05)

    i_kick = song.add_instrument(patch("drums/08_kick_808.arachnopatch"))
    i_snare = song.add_instrument(patch("drums/10_snare_909.arachnopatch"))
    i_hat_c = song.add_instrument(patch("drums/12_hat_closed_808.arachnopatch"))
    i_hat_o = song.add_instrument(patch("drums/13_hat_open_808.arachnopatch"))
    bass = patch("bass/04_sub_sine_bass.arachnopatch")
    bass.mono_mode = True
    bass.portamento_time = 0.06
    bass.portamento_legato = True
    i_bass = song.add_instrument(bass)
    i_keys = song.add_instrument(patch("keys/03_electric_piano.arachnopatch"))
    i_pad = song.add_instrument(patch("pads/04_warm_analog_pad.arachnopatch"))
    i_bell = song.add_instrument(patch("keys/02_fm_glass_bell.arachnopatch"))
    i_lead = song.add_instrument(patch("lead/10_porta_solo_lead.arachnopatch"))

    # F#m: i VI iv V -> F#m, D, Bm, C#
    roots = [note_name_to_midi(n) for n in ("F#2", "D2", "B1", "C#2")]
    chords = [
        ["F#3", "A3", "C#4"],
        ["D3", "F#3", "A3"],
        ["B2", "D3", "F#3"],
        ["C#3", "F3", "G#3"],
    ]

    def drums(p: Pattern, *, busy: bool) -> None:
        for bar in range(4):
            base = bar * 16
            place_note(p, base, t_kick, "F#1", i_kick, velocity=1.0, gate=3.0)
            place_note(p, base + 7, t_kick, "F#1", i_kick, velocity=0.6, gate=2.0, probability=0.8)
            if busy:
                place_note(p, base + 10, t_kick, "F#1", i_kick, velocity=0.75, gate=2.5)
            place_note(p, base + 8, t_snare, "D2", i_snare, velocity=0.8, gate=2.5)
        for row in range(0, ROWS, 2):
            if row % 8 == 6:
                place_note(p, row, t_hat, "F#2", i_hat_o, velocity=0.55, gate=3.0, probability=0.85)
            else:
                place_note(p, row, t_hat, "F#2", i_hat_c, velocity=0.5 if row % 4 == 2 else 0.35,
                           gate=1.0, probability=0.9)

    def bass_line(p: Pattern) -> None:
        seq = [(0, 0, 3.5), (6, 0, 1.5), (8, 7, 1.5), (12, 0, 3.0)]
        for bar in range(4):
            root = roots[bar] - 12
            for off, semi, g in seq:
                place_note(p, bar * 16 + off, t_bass, root + semi, i_bass, velocity=0.9, gate=g)

    def keys_chords(p: Pattern) -> None:
        for bar in range(4):
            for j, n in enumerate(chords[bar]):
                place_note(p, bar * 16 + (j % 2), t_keys, n, i_keys,
                           velocity=0.65 - j * 0.05, gate=13.0)

    def pad_wash(p: Pattern) -> None:
        for bar in range(4):
            for j, n in enumerate(chords[bar]):
                place_note(p, bar * 16, t_pad, n, i_pad, velocity=0.45 - j * 0.03, gate=15.5)

    def bells(p: Pattern) -> None:
        melody = [
            (4, "F#5"), (12, "C#5"), (20, "A4"), (28, "F#5"),
            (36, "B4"), (44, "D5"), (52, "G#4"), (60, "C#5"),
        ]
        for row, n in melody:
            place_note(p, row, t_bell, n, i_bell, velocity=0.5, gate=6.0, probability=0.9)

    def solo(p: Pattern) -> None:
        line = [
            (0, "F#4", 2.5), (4, "A4", 1.5), (6, "C#5", 2.0),
            (8, "E5", 3.5), (12, "D5", 2.0), (14, "C#5", 2.0),
            (16, "B4", 3.0), (20, "A4", 1.5), (22, "B4", 1.5),
            (24, "C#5", 4.0), (28, "G#4", 2.0), (30, "A4", 2.0),
            (32, "F#4", 5.0),
        ]
        for row, n, g in line:
            place_note(p, row, t_lead, n, i_lead, velocity=0.85, gate=g)
        note_off(p, 40, t_lead)

    p_intro = pattern(song, "Intro — keys + pad")
    keys_chords(p_intro)
    pad_wash(p_intro)
    bells(p_intro)

    p_verse = pattern(song, "Verse — groove")
    drums(p_verse, busy=False)
    bass_line(p_verse)
    keys_chords(p_verse)
    pad_wash(p_verse)

    p_chorus = pattern(song, "Chorus — solo")
    drums(p_chorus, busy=True)
    bass_line(p_chorus)
    pad_wash(p_chorus)
    bells(p_chorus)
    solo(p_chorus)

    p_out = pattern(song, "Outro — bells in the rain")
    keys_chords(p_out)
    pad_wash(p_out)
    bells(p_out)
    for bar in range(4):
        place_note(p_out, bar * 16, t_kick, "F#1", i_kick, velocity=0.7, gate=3.0)

    song.order = [0, 1, 1, 2, 2, 1, 2, 2, 0, 3]
    return song


# ---------------------------------------------------------------------------
# 4) EBM Percussion Lab — drum study, 126 BPM
# ---------------------------------------------------------------------------

def ebm_percussion_lab() -> Song:
    song = Song(
        title="EBM Percussion Lab",
        author="ArachnoTracker",
        description="Drum-first laboratory: layered 808/909/metal kit, euclidean rhythms, retrigs and probability.",
        notes="Study project: swap instruments per track, mutate probabilities, learn the drum engine.",
        bpm=126.0,
    )
    t_kick = song.add_track("Kick 909", volume=1.1)
    t_kick2 = song.add_track("Kick 808", volume=0.8)
    t_snare = song.add_track("Snare 909", volume=1.0, pan=0.03)
    t_clap = song.add_track("Clap 909", volume=0.75, pan=0.2)
    t_hatc = song.add_track("Closed Hat", volume=0.7, pan=-0.2)
    t_hato = song.add_track("Open Hat", volume=0.65, pan=-0.3)
    t_tom = song.add_track("Toms", volume=0.65, pan=0.1)
    t_rim = song.add_track("Rim + Cowbell", volume=0.7, pan=0.3)
    t_shaker = song.add_track("Shaker + Ride", volume=0.6, pan=-0.1)
    t_zap = song.add_track("Zap Snare", volume=0.55, pan=0.0)

    i_k909 = song.add_instrument(patch("drums/09_kick_909.arachnopatch"))
    i_k808 = song.add_instrument(patch("drums/08_kick_808.arachnopatch"))
    i_s909 = song.add_instrument(patch("drums/10_snare_909.arachnopatch"))
    i_clap = song.add_instrument(patch("drums/11_clap_909.arachnopatch"))
    i_hatc = song.add_instrument(patch("drums/12_hat_closed_808.arachnopatch"))
    i_hato = song.add_instrument(patch("drums/13_hat_open_808.arachnopatch"))
    i_toml = song.add_instrument(patch("drums/18_tom_808_low.arachnopatch"))
    i_tomh = song.add_instrument(patch("drums/19_tom_808_high.arachnopatch"))
    i_rim = song.add_instrument(patch("drums/16_rim_shot.arachnopatch"))
    i_cow = song.add_instrument(patch("drums/15_cowbell_808.arachnopatch"))
    i_shaker = song.add_instrument(patch("drums/17_shaker.arachnopatch"))
    i_ride = song.add_instrument(patch("drums/14_cymbal_808.arachnopatch"))
    i_zap = song.add_instrument(patch("drums/20_zap_snare.arachnopatch"))

    def kit(p: Pattern, *, level: int) -> None:
        # four-on-floor 909 with 808 layering on beat 1
        for row in range(0, ROWS, 4):
            place_note(p, row, t_kick, "C2", i_k909, velocity=1.0, gate=2.5)
            if level >= 1 and row % 16 == 0:
                place_note(p, row, t_kick2, "C1", i_k808, velocity=0.7, gate=3.0)
        for row in range(8, ROWS, 16):
            place_note(p, row, t_snare, "D2", i_s909, velocity=0.95, gate=2.5)
            if level >= 2:
                place_note(p, row + 1, t_clap, "D#2", i_clap, velocity=0.5, gate=2.0, probability=0.8)
        # closed hats 16ths with velocity swing
        for row in range(ROWS):
            if row % 4 == 0:
                continue
            vel = (0.75 if row % 4 == 2 else 0.5) if row % 2 == 0 else 0.35
            place_note(p, row, t_hatc, "F#2", i_hatc, velocity=vel, gate=1.0, probability=0.92)
        # open hat offbeats
        for row in range(6, ROWS, 8):
            place_note(p, row, t_hato, "A#2", i_hato, velocity=0.6, gate=3.0, probability=0.9)
        # shaker euclidean 7/16
        if level >= 1:
            for row, hit in enumerate(euclidean_hits(16, 7) * 4):
                if hit and row % 2:
                    place_note(p, row, t_shaker, "C#3", i_shaker, velocity=0.4, gate=0.75, probability=0.85)
        # rim shot euclidean 3/8, cowbell on 3/16 offset
        if level >= 2:
            for row, hit in enumerate(euclidean_hits(8, 3) * 8):
                if hit:
                    place_note(p, row, t_rim, "E2", i_rim, velocity=0.55, gate=1.5, probability=0.8)
            for row in range(4, ROWS, 16):
                place_note(p, row, t_rim, "G#2", i_cow, velocity=0.5, gate=1.5, probability=0.7)

    def tom_fill(p: Pattern, start: int = 48) -> None:
        fill = [(0, i_tomh, "C3"), (2, i_tomh, "D3"), (4, i_toml, "A2"), (6, i_toml, "G2"),
                (8, i_tomh, "C3"), (10, i_toml, "A2"), (12, i_toml, "F2"), (14, i_toml, "E2")]
        for i, (off, inst, n) in enumerate(fill):
            place_note(p, start + off, t_tom, n, inst, velocity=0.65 + i * 0.04, gate=2.0)

    def zap_accents(p: Pattern) -> None:
        for row in (24, 56):
            place_note(p, row, t_zap, "C3", i_zap, velocity=0.7, gate=2.5)

    p_basic = pattern(song, "Level 1 — core kit")
    kit(p_basic, level=0)

    p_layered = pattern(song, "Level 2 — layered")
    kit(p_layered, level=1)

    p_full = pattern(song, "Level 3 — full lab")
    kit(p_full, level=2)
    zap_accents(p_full)

    p_fill = pattern(song, "Tom fills + retrigs")
    kit(p_fill, level=2)
    tom_fill(p_fill, 48)
    place_note(p_fill, 62, t_snare, "D2", i_s909, velocity=0.9, gate=1.2,
               retrigger_count=4, retrigger_spacing_rows=0.25, retrigger_velocity_decay=0.8)

    p_ride = pattern(song, "Ride groove")
    kit(p_ride, level=1)
    for row in range(0, ROWS, 4):
        place_note(p_ride, row, t_shaker, "F#3", i_ride, velocity=0.5 if row % 8 else 0.65,
                   gate=3.0, probability=0.9)

    song.order = [0, 0, 1, 1, 2, 3, 2, 4, 2, 3, 1, 2]
    return song


# ---------------------------------------------------------------------------
# 5) Cinematic Darkwave Builder — C minor, 72 BPM
# ---------------------------------------------------------------------------

def cinematic_darkwave_builder() -> Song:
    song = Song(
        title="Cinematic Darkwave Builder",
        author="ArachnoTracker",
        description="Slow-burn gothic score: cathedral choir, drones, chamber strings, bells, impact percussion.",
        notes="C minor | i-VI-iv-VI | long-form tension template — intros, builds, breakdowns, climaxes.",
        bpm=72.0,
    )
    t_drone = song.add_track("Low Drone", volume=0.7)
    t_choir = song.add_track("Cathedral Choir", volume=0.55, pan=0.15)
    t_strings = song.add_track("Chamber Strings", volume=0.55, pan=-0.15)
    t_bell = song.add_track("Glass Bells", volume=0.5, pan=0.25)
    t_pad = song.add_track("Soundtrack Pad", volume=0.45, pan=-0.25)
    t_perc = song.add_track("Impacts", volume=0.8)
    t_tom = song.add_track("Ritual Toms", volume=0.7, pan=0.1)
    t_fx = song.add_track("Atmosphere FX", volume=0.5, pan=0.0)
    t_violin = song.add_track("Solo Violin", volume=0.6, pan=0.05)

    drone = patch("pads/12_low_drone_pad.arachnopatch")
    i_drone = song.add_instrument(drone)
    i_choir = song.add_instrument(patch("pads/03_cathedral_choir.arachnopatch"))
    i_strings = song.add_instrument(patch("strings/08_chamber_strings.arachnopatch"))
    i_bell = song.add_instrument(patch("keys/02_fm_glass_bell.arachnopatch"))
    i_pad = song.add_instrument(patch("pads/07_soundtrack_pad.arachnopatch"))
    i_impact = song.add_instrument(patch("fx/03_impact_hit.arachnopatch"))
    i_tom = song.add_instrument(patch("drums/05_tunnel_assault_tom.arachnopatch"))
    i_wind = song.add_instrument(patch("fx/06_wind.arachnopatch"))
    i_sweep = song.add_instrument(patch("fx/02_down_sweep.arachnopatch"))
    violin = patch("strings/10_solo_violin_synth.arachnopatch")
    violin.mono_mode = True
    violin.portamento_time = 0.08
    violin.portamento_legato = True
    i_violin = song.add_instrument(violin)

    # Cm: i - VI - iv - VI -> Cm, Ab, Fm, Ab
    roots = [note_name_to_midi(n) for n in ("C2", "Ab1", "F1", "Ab1")]
    chords = [
        ["C3", "Eb3", "G3"],
        ["Ab2", "C3", "Eb3"],
        ["F2", "Ab2", "C3"],
        ["Ab2", "C3", "Eb3"],
    ]

    def drone_bed(p: Pattern) -> None:
        for bar in range(4):
            place_note(p, bar * 16, t_drone, roots[bar] - 12, i_drone, velocity=0.75, gate=15.8)
            place_note(p, bar * 16, t_drone, roots[bar] - 5, i_drone, velocity=0.5, gate=15.8)

    def choir_pad(p: Pattern, track: int, inst: int, vel: float) -> None:
        for bar in range(4):
            for j, n in enumerate(chords[bar]):
                place_note(p, bar * 16, track, n, inst, velocity=vel - j * 0.04, gate=15.6)

    def string_lines(p: Pattern) -> None:
        # Slow counter-melody: C4-B3-C4-Eb4 | C4-Eb4-F4-Eb4 ...
        line = [
            (0, "C4", 6.0), (8, "B3", 3.0), (12, "C4", 3.5),
            (16, "Eb4", 6.0), (24, "C4", 6.5),
            (32, "F4", 6.0), (40, "Eb4", 3.0), (44, "D4", 3.0),
            (48, "Eb4", 7.0), (56, "C4", 6.0),
        ]
        for row, n, g in line:
            place_note(p, row, t_strings, n, i_strings, velocity=0.7, gate=g)
        note_off(p, 62, t_strings)

    def bell_motif(p: Pattern) -> None:
        bells_seq = [(0, "C5"), (6, "G4"), (16, "Eb5"), (22, "C5"), (32, "F5"), (38, "Eb5"), (48, "G5"), (54, "Eb5")]
        for row, n in bells_seq:
            place_note(p, row, t_bell, n, i_bell, velocity=0.5, gate=4.0, probability=0.92)

    def ritual_drums(p: Pattern, *, intense: bool) -> None:
        for bar in range(4):
            base = bar * 16
            place_note(p, base, t_perc, "C2", i_impact, velocity=0.85, gate=8.0)
            place_note(p, base + 8, t_tom, "C2", i_tom, velocity=0.8, gate=4.0)
            if intense:
                place_note(p, base + 4, t_tom, "E2", i_tom, velocity=0.6, gate=2.5, probability=0.85)
                place_note(p, base + 10, t_tom, "G1", i_tom, velocity=0.65, gate=2.5)
                place_note(p, base + 14, t_tom, "E2", i_tom, velocity=0.7, gate=2.0,
                           retrigger_count=2, retrigger_spacing_rows=0.5)

    def atmosphere(p: Pattern, *, sweep_end: bool = True) -> None:
        place_note(p, 0, t_fx, "C3", i_wind, velocity=0.45, gate=30.0)
        place_note(p, 32, t_fx, "C3", i_wind, velocity=0.4, gate=30.0)
        if sweep_end:
            place_note(p, 56, t_fx, "C4", i_sweep, velocity=0.55, gate=6.0)

    def violin_solo(p: Pattern) -> None:
        aria = [
            (0, "G4", 3.0), (4, "Ab4", 2.0), (6, "G4", 2.0),
            (8, "Eb4", 4.0), (12, "F4", 4.0),
            (16, "C5", 5.0), (22, "Bb4", 2.0), (24, "Ab4", 3.0), (28, "G4", 4.0),
            (32, "F4", 3.0), (36, "Eb4", 2.0), (38, "F4", 2.0), (40, "G4", 6.0),
            (48, "Ab4", 5.0), (54, "G4", 3.0), (56, "Eb4", 6.0),
        ]
        for row, n, g in aria:
            place_note(p, row, t_violin, n, i_violin, velocity=0.8, gate=g)
        note_off(p, 63, t_violin)

    p_atmo = pattern(song, "Atmosphere — drone + wind")
    drone_bed(p_atmo)
    atmosphere(p_atmo, sweep_end=False)

    p_choir = pattern(song, "Choir enters")
    drone_bed(p_choir)
    choir_pad(p_choir, t_choir, i_choir, 0.6)
    bell_motif(p_choir)
    atmosphere(p_choir)

    p_strings = pattern(song, "String movement")
    drone_bed(p_strings)
    choir_pad(p_strings, t_choir, i_choir, 0.55)
    string_lines(p_strings)
    bell_motif(p_strings)

    p_build = pattern(song, "Ritual build")
    drone_bed(p_build)
    choir_pad(p_build, t_pad, i_pad, 0.55)
    string_lines(p_build)
    ritual_drums(p_build, intense=False)
    atmosphere(p_build)

    p_climax = pattern(song, "Climax — violin aria")
    drone_bed(p_climax)
    choir_pad(p_climax, t_choir, i_choir, 0.65)
    ritual_drums(p_climax, intense=True)
    violin_solo(p_climax)

    p_fall = pattern(song, "Aftermath")
    drone_bed(p_fall)
    choir_pad(p_fall, t_choir, i_choir, 0.4)
    bell_motif(p_fall)
    atmosphere(p_fall, sweep_end=False)

    song.order = [0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 2, 4, 5, 5]
    return song


# ---------------------------------------------------------------------------
# 6) Rust Belt Industrial — G minor, 140 BPM
# ---------------------------------------------------------------------------

def rust_belt_industrial() -> Song:
    song = Song(
        title="Rust Belt Industrial",
        author="ArachnoTracker",
        description="Aggro-industrial machine music: crusher bass, metal strikes, sync-scream stabs, static.",
        notes="G minor | distorted machine funk | bit-crushed percussion, metallic hits, alarm FX.",
        bpm=140.0,
    )
    t_kick = song.add_track("Crusher Kick", volume=1.25)
    t_snare = song.add_track("Industrial Clap", volume=1.05, pan=0.05)
    t_hat = song.add_track("Metal Hats", volume=0.75, pan=-0.2)
    t_strike = song.add_track("Metal Strikes", volume=0.85, pan=0.25)
    t_bass = song.add_track("Crusher Bass", volume=1.0)
    t_stab = song.add_track("Sync Stabs", volume=0.85, pan=-0.1)
    t_lead = song.add_track("Scream Lead", volume=0.75, pan=0.15)
    t_fx = song.add_track("Static + Alarm", volume=0.6, pan=0.0)

    iron2 = patch("drums/01_ebm_iron_kick.arachnopatch"); iron2.gain *= 1.25
    zap = patch("drums/20_zap_snare.arachnopatch"); zap.gain *= 1.35
    razor2 = patch("drums/03_ebm_razor_hat.arachnopatch"); razor2.gain *= 1.35
    mhit = patch("fx/07_metallic_hit.arachnopatch"); mhit.gain *= 1.3
    i_kick = song.add_instrument(iron2)
    i_snare = song.add_instrument(zap)
    i_hat = song.add_instrument(razor2)
    i_strike = song.add_instrument(mhit)
    bass = patch("bass/03_reese_pressure_xl.arachnopatch")
    bass.mono_mode = True
    bass.portamento_time = 0.025
    i_bass = song.add_instrument(bass)
    stab2 = patch("keys/01_body_stab.arachnopatch"); stab2.gain *= 1.35
    scream_p = patch("lead/03_sync_scream_lead.arachnopatch"); scream_p.gain *= 1.2
    i_stab = song.add_instrument(stab2)
    i_lead = song.add_instrument(scream_p)
    i_static = song.add_instrument(patch("fx/09_radio_static.arachnopatch"))
    i_alarm = song.add_instrument(patch("fx/10_alarm_pulse.arachnopatch"))

    # G natural minor bass sequence: driving 8ths, b6 color
    bass_seq = [0, None, 0, 0, None, 0, None, 0, 0, None, 0, 8, None, 0, 10, None]

    def machine(p: Pattern, *, chaos: int = 0) -> None:
        for row in range(0, ROWS, 4):
            place_note(p, row, t_kick, "G1", i_kick, velocity=1.0, gate=2.5)
        for row in range(8, ROWS, 16):
            place_note(p, row, t_snare, "D2", i_snare, velocity=0.95, gate=2.5)
        for row in range(ROWS):
            if row % 2:
                place_note(p, row, t_hat, "F#2", i_hat, velocity=0.65 if row % 4 == 3 else 0.5,
                           gate=1.0, probability=0.9)
        # metallic strikes: harsh euclidean 5-in-16
        for row, hit in enumerate(euclidean_hits(16, 5) * 4):
            if hit:
                place_note(p, row, t_strike, "B2", i_strike, velocity=0.8, gate=2.0,
                           probability=0.75 + chaos * 0.1)
        for bar in range(4):
            root = note_name_to_midi("G1") + [0, 0, -4, 1][bar]  # G G Eb Ab
            for i, semi in enumerate(bass_seq):
                if semi is None:
                    continue
                place_note(p, bar * 16 + i, t_bass, root + semi, i_bass,
                           velocity=0.9 if i % 4 == 0 else 0.75, gate=1.4)

    def stabs(p: Pattern) -> None:
        stab_rows = [6, 14, 22, 30, 38, 46, 54, 62]
        chord = ["G3", "Bb3", "D4"]
        for r in stab_rows:
            for j, n in enumerate(chord):
                place_note(p, r, t_stab, n, i_stab, velocity=0.9 - j * 0.08, gate=2.0)

    def scream(p: Pattern) -> None:
        line = [
            (16, "G4", 1.5), (18, "Bb4", 1.5), (20, "D5", 2.0), (22, "C5", 2.0),
            (24, "Bb4", 3.0), (28, "G4", 3.0),
            (48, "F5", 2.0), (50, "D5", 2.0), (52, "Bb4", 3.0), (56, "G4", 6.0),
        ]
        for row, n, g in line:
            place_note(p, row, t_lead, n, i_lead, velocity=0.75, gate=g)
        note_off(p, 63, t_lead)

    def static_bed(p: Pattern, *, alarm: bool = False) -> None:
        place_note(p, 0, t_fx, "G3", i_static, velocity=0.3, gate=32.0, probability=0.95)
        place_note(p, 32, t_fx, "G3", i_static, velocity=0.28, gate=32.0, probability=0.95)
        if alarm:
            place_note(p, 48, t_fx, "G4", i_alarm, velocity=0.5, gate=12.0)

    p_machine = pattern(song, "Machine groove")
    machine(p_machine)
    stabs(p_machine)

    p_chaos = pattern(song, "Chaos groove")
    machine(p_chaos, chaos=1)
    stabs(p_chaos)
    static_bed(p_chaos, alarm=True)

    p_scream = pattern(song, "Scream section")
    machine(p_scream, chaos=1)
    stabs(p_scream)
    scream(p_scream)

    p_intro = pattern(song, "Power on")
    for r in range(0, ROWS, 8):
        place_note(p_intro, r, t_kick, "G1", i_kick, velocity=0.6 + r / ROWS * 0.4, gate=2.5)
    static_bed(p_intro, alarm=True)
    for bar in range(4):
        root = note_name_to_midi("G1") + [0, 0, -4, 1][bar]
        place_note(p_intro, bar * 16, t_bass, root, i_bass, velocity=0.8, gate=8.0)

    p_break = pattern(song, "Power down")
    for bar in range(4):
        root = note_name_to_midi("G1") + [0, 0, -4, 1][bar]
        place_note(p_break, bar * 16, t_bass, root, i_bass, velocity=0.85, gate=6.0)
        place_note(p_break, bar * 16 + 8, t_bass, root + 8, i_bass, velocity=0.6, gate=4.0)
    for row in range(8, ROWS, 16):
        place_note(p_break, row, t_snare, "D2", i_snare, velocity=0.85, gate=2.5)
    static_bed(p_break)

    song.order = [3, 0, 0, 1, 2, 4, 1, 2, 2, 0, 1, 2, 2, 4]
    return song


# ---------------------------------------------------------------------------

def main() -> None:
    builders = {
        "darkwave_foundation.arachno": darkwave_foundation,
        "factory_pulse.arachno": factory_pulse,
        "night_drive.arachno": night_drive,
        "ebm_percussion_lab.arachno": ebm_percussion_lab,
        "cinematic_darkwave_builder.arachno": cinematic_darkwave_builder,
        "rust_belt_industrial.arachno": rust_belt_industrial,
    }
    for filename, builder in builders.items():
        song = builder()
        path = OUT / filename
        save_project(song, path)
        notes = sum(len(p.steps) for p in song.patterns)
        print(f"{filename}: {len(song.tracks)} tracks, {len(song.instruments)} instruments, "
              f"{len(song.patterns)} patterns, {notes} steps, order={len(song.order)}")


if __name__ == "__main__":
    main()
