# ArachnoTracker

ArachnoTracker is becoming a Linux-first compositional tracker: pattern-based sequencing, a built-in synthesizer, and offline export for finished audio.

The current codebase is a headless foundation for the engine. It renders a demo song from tracker patterns into stereo audio and exports WAV natively, with MP3 and OGG export through common Linux encoders.

## What Works Now

- Value-based song model with tracks, instruments, patterns, rows, steps, note gates, microtiming, velocity, gain, and pan.
- Built-in polyphonic synthesizer with sine, square, saw, triangle, noise, dual oscillator mix, detune, sub oscillator, noise, low-pass shaping, filter envelope, LFO vibrato/tremolo, drive, ADSR envelope, and per-patch pan/gain.
- Offline renderer that schedules pattern events into sample buffers.
- Versioned `.arachno` project save/load.
- Command-based pattern editing suitable for terminal workflows and future TUI/GUI integration.
- Native 16-bit stereo WAV export.
- MP3 and OGG export when `ffmpeg` or `avconv` is installed.
- Per-track stem export for downstream mixing and arrangement workflows.
- Standard MIDI file export for DAWs, hardware sequencers, and external Linux synth chains.
- CTest smoke tests for tracker timing, audio rendering, and WAV export.

## Building

```bash
cmake -S . -B build
cmake --build build
```

The current engine has no mandatory third-party dependency beyond a C++17 compiler and CMake.

## Exporting Audio

Render the built-in demo song:

```bash
./build/ArachnoTracker --demo demo.wav
./build/ArachnoTracker --demo demo.mp3
./build/ArachnoTracker --demo demo.ogg
```

WAV export is built into ArachnoTracker. MP3 and OGG export use `ffmpeg` first, then `avconv` as a fallback:

```bash
sudo apt-get install ffmpeg
```

Inspect the demo project:

```bash
./build/ArachnoTracker --info
```

## Project Files

Create a reusable project file:

```bash
./build/ArachnoTracker --write-demo demo.arachno
```

Inspect and render a saved project:

```bash
./build/ArachnoTracker --project-info demo.arachno
./build/ArachnoTracker --validate demo.arachno
./build/ArachnoTracker --arrangement demo.arachno
./build/ArachnoTracker --instruments demo.arachno
./build/ArachnoTracker --export-patch demo.arachno 1 bright.arachnopatch
./build/ArachnoTracker --import-patch demo.arachno with-patch.arachno bright.arachnopatch BrightLead
./build/ArachnoTracker --show demo.arachno 0 0 32
./build/ArachnoTracker --render demo.arachno demo.wav
./build/ArachnoTracker --render-stems demo.arachno stems wav
./build/ArachnoTracker --export-midi demo.arachno demo.mid
```

The project format is line-oriented, versioned, and diff-friendly. It stores title, tempo, sample rate, tracks, instruments, synth patch parameters, patterns, steps, order list, gates, velocities, microtiming, and automation values.

## Command Editing

Apply tracker edits without opening a GUI:

```bash
./build/ArachnoTracker --edit demo.arachno edited.arachno \
  "move 4 1" \
  "inst 1" \
  "note D5 0.8" \
  "gate 1.25"
```

Apply repeatable edits from a command file:

```bash
./build/ArachnoTracker --edit-file demo.arachno generated.arachno commands.arachno-edit
```

Start a line-oriented terminal editing session:

```bash
./build/ArachnoTracker --interactive demo.arachno edited.arachno
```

Supported editor commands:

- `pattern N`
- `move ROW TRACK`
- `up [N]`, `down [N]`, `left [N]`, `right [N]`
- `note C4 [VELOCITY]`
- `inst N`
- `gate ROWS`
- `transpose SEMITONES`
- `transpose SEMITONES track`
- `tempo BPM`
- `rows-per-beat N`
- `new-pattern NAME ROWS [TRACKS]`
- `clone-pattern [NAME]`
- `pattern-name NAME`
- `append-order [PATTERN]`
- `set-order PATTERN...`
- `new-track NAME`
- `duplicate-track SRC [NAME]`
- `track-name TRACK NAME`
- `clear-track TRACK`
- `resize-pattern ROWS`
- `track-volume TRACK VALUE`
- `track-pan TRACK VALUE`
- `track-mute TRACK true|false`
- `track-solo TRACK true|false`
- `new-instrument NAME`
- `clone-instrument SRC [NAME]`
- `instrument-name INST NAME`
- `instrument-wave INST A|B sine|square|saw|triangle|noise`
- `instrument-param INST NAME VALUE`
- `fill-scale TRACK START COUNT STRIDE ROOT SCALE INST [VELOCITY] [GATE]`
- `euclid TRACK START STEPS PULSES ROOT INST [VELOCITY] [GATE]`
- `param NAME VALUE`
- `param-clear [NAME|*]`
- `view` in interactive mode
- `instruments` in interactive mode
- `clear` or `rest`
- `write` and `quit` in interactive mode

Examples:

```bash
./build/ArachnoTracker --edit demo.arachno generated.arachno \
  "fill-scale 1 0 12 4 C4 minor 1 0.75 0.8" \
  "euclid 0 0 16 5 C2 0 0.9 0.5" \
  "move 0 1" \
  "param cutoff 0.95" \
  "param vibrato 6"
```

Supported synth automation names include `mix`, `detune`, `sub`, `noise`, `cutoff`, `resonance`, `filter_env`, `lfo_rate`, `vibrato`, `tremolo`, `drive`, `gain`, `pan`, `attack`, `decay`, `sustain`, `release`, and filter envelope fields such as `filter_attack`.

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

## Next Big Pieces

To become a truly powerful compositional tool, the next layers should be:

- A full-screen keyboard-first tracker TUI/GUI on top of the editor session.
- Scripting bindings for generating and transforming patterns.
- MIDI input/output and controller mapping.
- JACK/PipeWire integration for Linux studio workflows.
- Plugin hosting, especially LV2 and CLAP.
- More synthesis: modulation matrix, filter envelopes, LFOs, wavetable import, sampler zones, and macros.
