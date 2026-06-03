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
- `SynthPatch`: native synthesizer preset with oscillator, modulation, filter, envelope, drive, lo-fi, unison, chorus, and percussion-transient controls.
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

Useful darkwave/EBM presets are available in `at.presets`:

- `low_saw()`
- `bright_twin()`
- `soft_wide()`
- `ebm_kick()`
- `gated_snare()`
- `metal_hat()`

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
