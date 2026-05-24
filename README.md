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
./build/ArachnoTracker --render demo.arachno demo.wav
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
- `clear` or `rest`
- `write` and `quit` in interactive mode

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
