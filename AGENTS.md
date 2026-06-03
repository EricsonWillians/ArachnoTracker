# AGENTS.md

This file defines repository-specific working rules and project context for coding agents (Codex or similar). It is the single source of truth for how this project is built, tested, organized, and modified.

## Project Overview

**ArachnoTracker** is a Linux-first tracker workstation for darkwave/EBM production. It is written in C++17 and uses CMake as its build system. The project provides:

- Pattern-based composition with per-step effects, probability, and retriggering.
- A native synthesizer engine with 4 oscillators, FM, ring mod, hard sync, unison, filters, envelopes, LFO, chorus, delay, reverb, and bit crushing.
- A raw X11 GUI frontend (no Qt/GTK).
- Realtime playback and offline rendering to WAV (native), MP3, and OGG (via `ffmpeg`/`avconv`).
- MIDI import/export.
- A Python composition SDK for generative and batch editing workflows.

All code lives under a single `arachno` C++ namespace. The project is roughly:

- ~34,500 lines of C++ source in `src/`
- ~9,400 lines of C++ headers in `include/`
- ~1,300 lines of Python SDK in `python/arachnotracker/`

## Technology Stack

| Layer | Technology |
|-------|-----------|
| Language | C++17 |
| Build system | CMake (3.15+) |
| GUI | X11 (raw Xlib) |
| Audio backends | ALSA (optional), PipeWire/JACK/Dummy (modeled) |
| Threading | `std::thread`, pthread |
| SIMD | SSE2/SSE3 (denormal flush) |
| Python SDK | Pure standard library (dataclasses, pathlib, json, unittest) |
| Export | Native WAV; MP3/OGG via external `ffmpeg`/`avconv` |

## Build and Test Commands

### Requirements

- CMake 3.15+
- C++17 compiler (GCC or Clang)
- X11 development libraries
- `pthread` support
- Optional: ALSA development libraries (`libasound`)
- Optional: `ffmpeg` or `avconv` for MP3/OGG export

### Build

```bash
cmake -S . -B build
cmake --build build -j8
```

### Run

```bash
./build/ArachnoTracker              # starts the GUI shell
./build/ArachnoTracker --gui [project.arachno]
./build/ArachnoTracker --help
```

### Tests

After meaningful C++ changes:

```bash
cmake -S . -B build
cmake --build build -j8
timeout 20s ./build/arachno_smoke_tests; echo EXIT:$?
```

> **Note:** `arachno_smoke_tests` can intentionally run until timeout under `timeout`; record the observed `EXIT` value used by the current baseline.

For Python SDK changes:

```bash
PYTHONPATH=python python3 -m unittest discover -s python/tests
```

Also available via CTest:

```bash
ctest --test-dir build --output-on-failure
```

## Code Organization

The project is organized into strict module boundaries:

| Directory | Purpose |
|-----------|---------|
| `src/core/` | Tracker domain model: `Song`, `Pattern`, `PatternStep`, `Note`, `Instrument`, `StepEffects`, timing/grid rules. |
| `src/audio/` | Synthesis, realtime playback, offline rendering, runtime backends. `Synthesizer.cpp` is the native voice engine. `AudioEngine.cpp` handles offline mixdown/stems. `RealtimePlayback.cpp` owns the transport state machine. |
| `src/ui/` | Application orchestration: `ApplicationSession` (central coordinator), `AppActions`, `EditorActions`/`Shortcuts`/`CommandPalette`, `EditorViewModel`, `AppSettings`/`Task`/`Event`/`Async`, `ProjectLifecycle`. |
| `src/ui/gui/` | GUI frontend behavior (~90 `Gui*Ops.cpp` files). Raw X11 draw/event/input dispatch. `GuiWindow.cpp` is the composition root. |
| `src/cli/` | Command-line parsing/rendering (currently minimal; CLI logic lives in `src/main.cpp`). |
| `src/utils/` | I/O, diagnostics, import/export: `ProjectIO`, `PatchIO`, `MidiImporter`/`Exporter`, `ExportWorkflow`, `AutoSave`, `ProjectDiagnostics`, `ScriptIntegration`, `FileCompatibility`. |
| `include/` | Public headers mirroring `src/` structure. |
| `python/arachnotracker/` | Python SDK: `model.py`, `io.py`, `presets.py`, `templates.py`, `generative.py`, `plugins.py`, `commands.py`. |
| `tests/` | C++ smoke tests (`test_audio.cpp`, `test_tracker.cpp`). Uses raw `assert()`, no external test framework. |
| `python/tests/` | Python SDK tests (`test_python_sdk.py`). |
| `examples/projects/` | 6 starter `.arachno` projects. |
| `examples/scripts/` | Python example scripts. |
| `patches/factory/` | 17 factory `.arachnopatch` presets. |

### Key Files

- `src/main.cpp` — CLI entry point. Dispatches to GUI, render, MIDI import/export, patch I/O, project info/validation, interactive editor, and edit-file processing.
- `src/audio/Synthesizer.cpp` (~115,000 chars) — Native synth voice engine.
- `src/audio/AudioEngine.cpp` — Offline rendering pipeline.
- `src/audio/RealtimePlayback.cpp` — Transport, audition, row/event scheduling.
- `src/core/Tracker.cpp` — Song construction, demo songs, template builders.
- `src/ui/ApplicationSession.cpp` — Central orchestrator for project lifecycle, playback, settings, diagnostics, snapshots.
- `src/utils/ProjectIO.cpp` — Native `.arachno` format read/write (text-based, versioned).
- `src/utils/PatchIO.cpp` — Native `.arachnopatch` format read/write.

## Naming and File Conventions

- GUI operational files follow `Gui<Area><Operation>Ops.cpp` with paired headers in `include/ui/gui`.
- Common operation suffixes:
  - `DrawOps` — drawing/layout/text rendering
  - `EventOps` — event handling (key/mouse/window)
  - `InputOps` — input preprocessing and gesture routing
  - `MotionOps` / `WheelOps` — pointer movement and wheel behavior
  - `ClickOps` — click dispatch and button interactions
  - `ContextFactoryOps` — construct typed context bundles for operations
  - `InteractionAdapterOps` — bridge between composed contexts and handlers
  - `LifecycleOps` — window/resource init/shutdown flows
  - `BindingsOps` — wrap mutable `GuiWindow` state as callable interfaces
  - `RenderAdapterOps` / `RunLoopAdapterOps` — high-level orchestration adapters
- Keep feature logic grouped by operation type (draw/event/input/context factory/lifecycle).

## Primary Engineering Rules

1. **Preserve behavior first.**
   - Prefer incremental, scoped changes over broad rewrites.
   - Do not silently change file formats, CLI contracts, or keyboard shortcuts.

2. **Respect module boundaries.**
   - `src/core` owns domain models and must not depend on UI or platform code.
   - `src/audio` owns DSP and realtime paths; keep it deterministic.
   - `src/ui` owns application orchestration; GUI should interact through action/session contracts.
   - `src/ui/gui` owns frontend rendering and input dispatch only.
   - `src/utils` owns I/O and integration helpers.

3. **Keep realtime paths deterministic and safe.**
   - Avoid allocations in tight realtime loops when possible.
   - Prefer fixed-order reductions for deterministic audio summing.
   - If parallelizing audio paths, preserve stable summation order.
   - Precompute control values per block where possible.

4. **GUI complexity must be modularized.**
   - Do not add behavior directly to `GuiWindow.cpp`; use operation modules.
   - If a feature touches multiple pathways, add typed context structures in headers, keep behavior in operation modules, and wire through adapter/factory functions.

5. **Documentation is part of the change.**
   - Update relevant README/docs when introducing a new subsystem contract, mode, or command.
   - Keep `--help` output in sync with implemented flags and command behavior.

## Testing Strategy

- **C++ smoke tests** (`arachno_smoke_tests`): Cover audio rendering, tracker model, WAV export, stem rendering, pattern editing, project round-trip, MIDI import/export, patch I/O, shortcuts, command palette, diagnostics, and generative helpers. Uses raw `assert()`; no external test framework.
- **Python SDK tests** (`python/tests/test_python_sdk.py`): Cover note conversion, project round-trip, patch plugins, edit scripts, generative helpers, and module CLI.
- **No deployment scripts or CI configs** are present in the repository.

## Performance Change Expectations

When changing synth/runtime performance behavior:

- Capture before/after evidence with a reproducible command:

```bash
/usr/bin/time -f 'real=%e user=%U sys=%S cpu=%P maxrss=%M' \
  ./build/ArachnoTracker --render <input.arachno> /tmp/out.wav
```

- Prefer measurements and deterministic behavior claims over intuition.
- Any new metrics should be exposed through snapshot-friendly telemetry structures (e.g., `SynthRenderTelemetry`).

## CLI and UX Guardrails

- Do not remove existing shortcuts/commands without an explicit migration path.
- New telemetry or tuning UI should remain readable in DOS and high-contrast themes.
- The CLI is the primary non-GUI interface; all flags are implemented in `src/main.cpp`.

## Contracts Worth Protecting

- `.arachno` and `.arachnopatch` format compatibility.
- CLI command/flag behavior from `src/main.cpp`.
- Deterministic sequencing semantics (row timing, order traversal, retrig/probability behavior).
- Stable shortcut/action identifiers used by GUI and automation.
- `ApplicationSession::snapshot(...)` is the principal frontend state boundary.

## What to Avoid

- No destructive git operations (`reset --hard`, checkout overwrite) unless explicitly requested.
- No broad formatting-only churn mixed with logic changes.
- No speculative refactors in unrelated modules while fixing a targeted issue.
- No allocations in tight realtime audio loops.

## Documentation Map

- `README.md` — Human-facing quick start, build instructions, CLI usage.
- `docs/ARCHITECTURE.md` — Top-level project map, runtime layers, data flow, snapshot contract.
- `docs/PYTHON_API.md` — Full Python SDK documentation.
- `docs/LESSONS_LEARNED.md` — Recurring corrections (realtime allocation-free, deterministic parallel audio, GUI modularization, actionable telemetry, preserving workflows).
- `src/audio/README.md` — Audio module ownership, file map, change rules.
- `src/core/README.md` — Core module ownership, file map, change rules.
- `src/ui/README.md` — UI module ownership, key components, architecture rules.
- `src/ui/gui/README.md` — GUI naming conventions, coordination files, functional areas.
- `python/arachnotracker/README.md` — Python package purpose, module map, usage.
