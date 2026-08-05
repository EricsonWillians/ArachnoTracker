# General MIDI Preset Bank

128 curated `.arachnopatch` presets — one per General MIDI program — tuned for
professional 80s/90s synthesizer character:

- FM electric pianos and bells (DX7-style FM with **modulator decay envelopes**
  and velocity-scaled FM depth — the metallic clang dies into a warm body)
- Juno-style ensemble-chorus strings, brass sections, and lush pads
- Analog **mono-mode** basses and leads with portamento/glide (SH-101/TB-303/Moog
  character)
- 808/909-class drum voices per percussion class (click-snap kicks, gated-crack
  snares, metallic hats) with FM-decay cymbals
- Drawbar/rock/church organs, plucked guitars, reeds, flutes, synth FX

## Usage

- **MIDI import** (`--import-midi`) voices every imported track from this bank
  automatically: the dominant GM program of each track/lane selects the preset
  (see `src/utils/GmPresetBank.cpp`).
- The GUI patch file browser can load any entry directly as a starting point
  for the synth designer.

## Regenerating

The bank is generated from the built-in C++ preset table (full 148-parameter
fidelity, unlike SDK-generated files):

```bash
./build/ArachnoTracker --write-gm-presets patches/gm
```

Files are named `NNN_<gm_program_name>.arachnopatch` with the 0-based GM
program number.
