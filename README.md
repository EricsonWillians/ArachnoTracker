# ArachnoTracker

ArachnoTracker is a Linux-first tracker workstation for darkwave/EBM production.
It combines:

- Pattern-based composition
- A native synthesizer engine
- MIDI import/export
- Realtime audition and playback
- Offline render (WAV, MP3, OGG)
- Python composition SDK

## Build

Requirements:

- CMake (3.15+)
- C++17 compiler
- X11 development libraries
- `pthread` support
- Optional: ALSA development libraries (`libasound`) for native ALSA integration
- Optional: `ffmpeg` or `avconv` for MP3/OGG export

Build commands:

```bash
cmake -S . -B build
cmake --build build -j8
```

## Run

Default frontend entrypoint:

```bash
./build/ArachnoTracker
```

Frontend selection:

```bash
./build/ArachnoTracker --gui [project.arachno]
./build/ArachnoTracker --gui-window [project.arachno]
./build/ArachnoTracker --gui-shell [project.arachno]
```

Helpful CLI:

```bash
./build/ArachnoTracker --help
./build/ArachnoTracker --project-info project.arachno
./build/ArachnoTracker --validate project.arachno
./build/ArachnoTracker --actions
./build/ArachnoTracker --shortcuts
```

## Render and Exchange

Render full mix:

```bash
./build/ArachnoTracker --render project.arachno output.wav
./build/ArachnoTracker --render project.arachno output.mp3
./build/ArachnoTracker --render project.arachno output.ogg
```

Render stems:

```bash
./build/ArachnoTracker --render-stems project.arachno stems wav
```

MIDI:

```bash
./build/ArachnoTracker --import-midi input.mid output.arachno [rows-per-beat] [pattern-rows]
./build/ArachnoTracker --export-midi project.arachno output.mid
```

Patch I/O:

```bash
./build/ArachnoTracker --export-patch project.arachno 1 patch.arachnopatch
./build/ArachnoTracker --import-patch in.arachno out.arachno patch.arachnopatch [name]
./build/ArachnoTracker --replace-patch in.arachno out.arachno 1 patch.arachnopatch [name]
```

## Typical Composition Flow

1. Create/load project.
2. Build instruments/patches.
3. Program patterns and per-cell effects.
4. Arrange order list for full song structure.
5. Audition pattern vs full song playback.
6. Export final mix/stems/MIDI.

## Tests

```bash
ctest --test-dir build --output-on-failure
PYTHONPATH=python python3 -m unittest discover -s python/tests
```

## Documentation Map

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
- [docs/LESSONS_LEARNED.md](docs/LESSONS_LEARNED.md)
- [src/audio/README.md](src/audio/README.md)
- [src/core/README.md](src/core/README.md)
- [src/ui/README.md](src/ui/README.md)
- [src/ui/gui/README.md](src/ui/gui/README.md)
- [python/arachnotracker/README.md](python/arachnotracker/README.md)
- [docs/PYTHON_API.md](docs/PYTHON_API.md)
