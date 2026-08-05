# ArachnoTracker Python API

The Python SDK is a standard-library-only composition layer for creating,
loading, transforming, and exporting native ArachnoTracker project assets. It is
intended for community scripts, generative composition, batch editing, preset
packs, and patch-generator plugins.

Use it from the repository with:

```bash
PYTHONPATH=python python3 your_script.py
```

## Core Concepts

Import the public API as:

```python
import arachnotracker as at
```

The object graph mirrors the native project format:

- `Song`: project metadata, tracks, instruments, patterns, and arrangement order.
- `Track`: mixer lane name, volume, pan, mute, and solo state.
- `SynthPatch`: native synthesizer preset modeling the full engine surface — 4 oscillators with per-osc level/detune/pulse-width/PWM/drive, layered ratio/decay timbre, modulation, dual-filter modes, FM, envelopes, drive, lo-fi, unison, chorus/delay/reverb FX, color/character, velocity expression, and percussion-transient controls.
- `Pattern`: tracker grid addressed by `(row, track)`.
- `Step`: one grid cell with note, instrument, velocity, gate, probability, retriggering, microtiming, and automation.
- `Envelope`: ADSR values in seconds plus normalized sustain.
- `Waveform`: `SINE`, `SQUARE`, `SAW`, `TRIANGLE`, and `NOISE`.

## Project IO

Write a project:

```python
song = at.Song(title="Python Sketch", author="Me", bpm=132)
drums = song.add_track("Drums")
kick = song.add_instrument(at.presets.ebm_kick())

pattern = at.Pattern("Opening", rows=64, tracks=len(song.tracks))
at.generative.place_note(pattern, 0, drums, "C2", kick, velocity=1.0, gate=0.25)

song.add_pattern(pattern)
song.order = [0]
at.save_project(song, "python_sketch.arachno")
```

Load, transform, and save a project:

```python
song = at.load_project("python_sketch.arachno")
song.bpm = 136

for pattern in song.patterns:
    for step in pattern.steps.values():
        if step.midi is not None:
            step.velocity *= 0.9

at.save_project(song, "python_sketch_136.arachno")
```

Validate with the native binary:

```bash
./build/ArachnoTracker --validate python_sketch_136.arachno
./build/ArachnoTracker --render python_sketch_136.arachno python_sketch_136.wav
```

## Patch IO

Write a patch:

```python
patch = at.SynthPatch(
    name="Cold Bass",
    oscillator_a=at.Waveform.SAW,
    oscillator_b=at.Waveform.SQUARE,
    cutoff=0.34,
    drive=0.38,
    bit_crush=0.08,
)
at.save_patch(patch, "cold_bass.arachnopatch")
```

Read a patch:

```python
patch = at.load_patch("cold_bass.arachnopatch")
patch.chorus_mix = 0.12
at.save_patch(patch, "cold_bass_wide.arachnopatch")
```

### Extended patch fields

In addition to the core fields, `SynthPatch` exposes the extended engine
surface. Every field default mirrors the native C++ `SynthPatch` default, so
SDK patches load identically in the app:

- `filter_drive`, `filter_keytrack`
- `chorus_ensemble` (Juno-style multi-tap ensemble chorus)
- `delay_mix`, `delay_time`, `delay_feedback`
- `reverb_mix`, `reverb_size`, `reverb_damping`, `reverb_decay`
  (Freeverb-class comb/allpass reverb)
- `portamento_time` (seconds of pitch glide; `0` = off),
  `portamento_legato` (glide only between overlapping notes)
- `mono_mode` (classic monosynth: only one voice per instrument+channel)
- `fm_decay` (seconds for FM depth to decay to zero; `0` = constant) and
  `velocity_to_fm` (velocity→FM depth, DX7-style dynamics)
- `Waveform.SUPERSAW` oscillator shape

The full native parameter surface is modeled, grouped as:

- Oscillator C/D and per-oscillator shape controls: `oscillator_c`,
  `oscillator_d` (waveforms), `oscillator_c_enabled`, `oscillator_d_enabled`,
  `oscillator_c_mix`, `oscillator_d_mix`, `detune_c_cents`, `detune_d_cents`,
  `osc_a_level`..`osc_d_level`, `osc_a_detune_cents`..`osc_d_detune_cents`,
  `osc_a_pulse_width`..`osc_d_pulse_width`, `osc_a_pwm_depth`..`osc_d_pwm_depth`,
  `osc_a_drive`..`osc_d_drive`
- Layered timbre (per-osc frequency ratios and 1-pole decay layers):
  `osc_b_ratio`, `osc_c_ratio`, `osc_d_ratio` (default `1.0`),
  `osc_b_decay`, `osc_c_decay`, `osc_d_decay`, `velocity_to_decay`,
  `key_track_decay` (default `0.0`)
- Filter / FM / drive extras: `filter_mode`, `lfo_filter_depth`,
  `lfo_pan_depth`, `fm_feedback`, `fm_algorithm`, `fm_color`, `fm_spread`,
  `wavefold`, `comb_mix`, `comb_time`, `comb_feedback`
- Delay extras: `delay_tone`, `delay_stereo`, `delay_mod_depth`,
  `delay_drive`, `delay_ducking`, `delay_diffusion`, `delay_wow`,
  `delay_crossfeed`
- Reverb extras: `reverb_pre_delay`, `reverb_diffusion`, `reverb_width`,
  `reverb_shimmer`, `reverb_mod_depth`, `reverb_early_mix`, `reverb_tone`,
  `reverb_chorus`, `reverb_bloom`
- Chorus extras: `chorus_feedback`, `chorus_delay`, `chorus_width`,
  `chorus_tone`, `chorus_jitter`, `chorus_saturation`
- Color / character: `analog_color`, `vintage_drift`, `wow_flutter`,
  `tone_tilt`, `tape_color`, `air_boost`, `low_punch`, `analog_warmth`,
  `voice_slop`, `phase_scatter`, `unison_warp`, `unison_humanize`,
  `console_crosstalk`, `stereo_depth`, `hifi_exciter`, `output_transformer`,
  `output_soft_clip`, `output_glue`
- Velocity expression (`.arachnopatch` files only; the project instrument
  chain omits this block by design): `velocity_to_amp`, `velocity_to_filter`,
  `velocity_to_attack`, `velocity_curve`, `filter_keytrack_resonance`,
  `filter_nonlinearity`, `amp_envelope_curve`, `filter_envelope_curve`

`save_patch` writes the modern `arachno_patch 2` 159-value layout (the same
layout the C++ application writes, including all four oscillator names on the
`oscillators` line), and `load_patch` reads every historical layout (v1
legacy 32-value files, intermediate layouts, and any modern v2 tail length —
138-value early-v2 files through current 159-value files, including the
factory and `patches/gm/` libraries).

Useful darkwave/EBM presets are available in `at.presets`:

- `low_saw()`
- `bright_twin()`
- `soft_wide()`
- `ebm_kick()`
- `gated_snare()`
- `metal_hat()`

Classic synthesized strings (full ADSR, high sustain for held notes):

- `classic_strings()` — lush 80s ensemble strings
- `synth_strings_85()` — brighter mid-80s synth strings
- `analog_string_machine()` — Solina-style slow-swell string machine

## Pattern Editing

Create or access a step:

```python
step = pattern.step(row=12, track=1)
step.midi = at.note_name_to_midi("F2")
step.instrument = 0
step.velocity = 0.82
step.gate = 0.75
step.micro_offset_rows = -0.04
```

Complex percussion is modeled directly on steps:

```python
hat = pattern.step(14, 0)
hat.midi = at.note_name_to_midi("C5")
hat.instrument = 2
hat.velocity = 0.42
hat.gate = 0.1
hat.probability = 0.65
hat.retrigger_count = 3
hat.retrigger_spacing_rows = 0.12
hat.retrigger_velocity_decay = 0.72
```

A step can also be a note-off (shown as `===` in the GUI): it releases the last
note started on that track through its envelope release phase, which is how
long sustained notes (pads, strings) are ended early:

```python
pad_note = pattern.step(0, 1)
pad_note.midi = at.note_name_to_midi("C4")
pad_note.instrument = 3
pad_note.gate = 24.0  # rings for 24 rows unless a note-off cuts it
release_step = pattern.step(8, 1)
release_step.note_off = True
```

Per-step automation targets native synth parameters:

```python
step.automation["cutoff"] = 0.42
step.automation["drive"] = 0.5
step.automation["fm"] = 0.18
```

Common automation names include `mix`, `detune`, `pulse_width`, `pwm`,
`fm`, `fm_ratio`, `chorus`, `chorus_rate`, `chorus_depth`, `unison`,
`unison_detune`, `spread`, `sub`, `noise`, `cutoff`, `resonance`,
`filter_env`, `pitch_env`, `pitch_decay`, `click`, `transient_noise`,
`transient_decay`, `lfo_rate`, `vibrato`, `tremolo`, `ring_mod`, `hard_sync`,
`drive`, `bitcrush`, `sample_reduce`, `highpass`, `gain`, `pan`,
`chorus_feedback`, `chorus_delay`, `chorus_width`, `delay_mix`,
`delay_time`, `delay_feedback`, `delay_tone`, `reverb_mix`, `reverb_size`,
`reverb_damping`, `reverb_pre_delay`, `vintage_drift`, `wow_flutter`,
`attack`, `decay`, `sustain`, `release`, and filter envelope fields such as
`filter_attack`.

## Generative Helpers

Scale helpers:

```python
notes = at.generative.scale_notes("C3", "minor", [0, 2, 4, 6, 7])
```

Available scales are `major`, `minor`, `dorian`, `phrygian`, `lydian`,
`mixolydian`, `locrian`, `pentatonic`, `minor-pentatonic`, and `chromatic`.

Euclidean rhythms:

```python
at.generative.fill_euclidean(
    pattern,
    track=0,
    start_row=0,
    steps=16,
    pulses=5,
    note="C5",
    instrument=2,
    velocity=0.35,
    gate=0.12,
    probability=0.72,
)
```

Scale fills:

```python
at.generative.fill_scale(
    pattern,
    track=1,
    start_row=0,
    count=16,
    stride=4,
    root="C2",
    scale="minor",
    instrument=1,
    degrees=[0, 0, 3, 2, 0, 5, 3, 2],
)
```

## Templates

Create a complete darkwave/EBM starter:

```python
song = at.darkwave_ebm_starter("Community Starter", bpm=132)
at.save_project(song, "community_starter.arachno")
```

The template includes drums, bass, lead, pad, six native synth instruments, an
arrangement order, probability accents, retriggered percussion, and automation.

## Patch-Generator Plugins

A patch plugin is an ordinary Python module exposing a factory that returns
`SynthPatch`.

```python
from arachnotracker import SynthPatch, Waveform

def create_patch(name="Industrial Bass"):
    return SynthPatch(
        name=name,
        oscillator_a=Waveform.SAW,
        oscillator_b=Waveform.SQUARE,
        cutoff=0.32,
        drive=0.5,
        bit_crush=0.1,
    )
```

Load it from another script:

```python
patch = at.load_patch_plugin("industrial_bass.py", name="Factory Bass")
at.save_patch(patch, "factory_bass.arachnopatch")
```

Or run it through the module CLI:

```bash
PYTHONPATH=python python3 -m arachnotracker patch-plugin industrial_bass.py factory_bass.arachnopatch --name FactoryBass
```

This plugin layer is for composition and native preset generation. It does not
host native DSP plugin formats.

## Command Scripts

`EditScript` writes `.arachno-edit` files consumable by the native CLI:

```python
script = (
    at.EditScript()
    .move(14, 1)
    .probability(0.65)
    .retrigger(3, 0.12, 0.72)
    .automation("cutoff", 0.8)
)
script.write("fill.arachno-edit")
```

Apply it:

```bash
./build/ArachnoTracker --edit-file input.arachno output.arachno fill.arachno-edit
```

## Module CLI

Create a starter project:

```bash
PYTHONPATH=python python3 -m arachnotracker new-ebm starter.arachno --title "Community Starter" --bpm 132
```

Run a patch plugin:

```bash
PYTHONPATH=python python3 -m arachnotracker patch-plugin plugin.py output.arachnopatch --name PluginBass
```

## Stability Notes

The SDK intentionally uses plain dataclasses and standard-library modules so
community scripts remain easy to read, version, and share. Public imports from
`arachnotracker` are the preferred compatibility surface. Native file versions
are checked while loading, and missing older synth fields keep their dataclass
defaults where possible.
