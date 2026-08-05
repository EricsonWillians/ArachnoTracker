# AGENTS.md

This file defines repository-specific working rules and project context for coding agents (Codex or similar). It is the single source of truth for how this project is built, tested, organized, and modified.

## Project Overview

**ArachnoTracker** is a Linux-first tracker workstation for darkwave/EBM production. It is written in C++17 and uses CMake as its build system. The project provides:

- Pattern-based composition with per-step effects, probability, retriggering, and note-off steps (`===`) that release sustained notes through their ADSR release phase. A legato input mode (sidebar `LEGATO` toggle, `L` key, or `legato [on|off|toggle]` editor command) fills the entered step's gate at edit time so notes sustain until the next note or `===` on the same track; sustained notes draw a slim marker bar in the grid. GUI octave numbers follow the `midiNoteName` convention (60 = C4) everywhere (sidebar piano, computer-keyboard entry, synth designer keyboard).
- A native synthesizer engine with 4 oscillators, FM, ring mod, hard sync, unison, filters, envelopes, LFO, chorus, delay, reverb, and bit crushing.
- Full ADSR (amp + filter) envelopes per patch; sustain holds for as long as a key is held, with true note-off from computer keyboard and external MIDI controllers (ALSA input, global audition).
- A raw X11 GUI frontend (no Qt/GTK).
- Realtime playback and offline rendering to WAV (native), MP3, and OGG (via `ffmpeg`/`avconv`).
- MIDI import/export.
- A 98-patch factory library organized by category (`patches/factory/<category>/`).
- A 128-preset curated General MIDI bank (`patches/gm/`, generated from `src/utils/GmPresetBank.cpp`) that voices imported `.mid` files with era-authentic 80s/90s sounds (FM EPs, Juno ensemble strings/pads, analog mono basses with portamento).
- Portamento/glide per patch (`portamentoTime`, `portamentoLegato`), mono mode per patch (`monoMode`: new note steals same instrument+channel voices — classic monosynth), an FM depth envelope (`fmDecay` seconds + `velocityToFm` DX7-style dynamics), per-oscillator frequency ratios and 1-pole decay layers (`oscBRatio`/`oscCRatio`/`oscDRatio` × note freq; `oscBDecay`/`oscCDecay`/`oscDDecay` seconds, 0 = constant — piano/EP clang-over-body layering; FM operator phases derive from the same oscillator phases, so ratios shift FM tuning), amp-decay dynamics (`velocityToDecay`, `keyTrackDecay` — harder strikes and lower notes ring longer), true 1:1 filter keyboard tracking (`filterKeytrack` = 1.0 moves cutoff one octave per octave of pitch — pre-2026-08 builds tracked ~0.45:1), SVF notch/peak filter modes (`filterMode` 3/4), a Freeverb-class comb/allpass reverb (per-voice networks offline; one shared send bus in realtime — per-voice reverb was ~50% of realtime DSP cost) with proper FX tail ring-out, a master-bus "smile" EQ (low lift/mid dip/high cut), and an offline-render full-quality mode (`Synthesizer::setOfflineRendering`, used by `AudioEngine`) that disables all load-adaptive degradation outside realtime playback. Realtime adaptive quality is driven by measured DSP load only (never raw voice counts), so dense arrangements keep full quality on healthy machines.
- A Python composition SDK for generative and batch editing workflows.

All code lives under a single `arachno` C++ namespace. The project is roughly:

- ~34,500 lines of C++ source in `src/`
- ~9,400 lines of C++ headers in `include/`
- ~2,200 lines of Python SDK in `python/arachnotracker/`

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
cmake -S . -B build          # defaults to Release (-O3 -march=native) since 2026-08;
cmake --build build -j8      #   the DSP engine cannot sustain realtime in an -O0 build
```

- Override with `-DCMAKE_BUILD_TYPE=Debug` for debugging.
- Disable native codegen for portable binaries with `-DARACHNO_NATIVE_OPT=OFF`.
- No fast-math anywhere: IEEE semantics preserved, offline renders stay deterministic.
- The smoke-test target is always compiled with `-UNDEBUG` (the suite uses raw
  `assert()`, including a few side effects inside assert expressions; building it
  with NDEBUG both silences checks and deadlocks the async-task test).

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
>
> **Baseline (2026-08):** the full suite passes (`EXIT:0`). The previously known
> `testProjectRoundTrip`/`testPatchRoundTrip` failures were fixed by removing the
> non-idempotent load-time `applyCompetitionPresetQuality` polish from
> `PatchIO`/`ProjectIO` (load paths are now transparent; polish is creation-time only).
> - Parallel-bus offline rendering of the full demo song is not bit-deterministic across runs (pre-existing); single-bus renders (< 96 events) are deterministic.

For Python SDK changes:

```bash
PYTHONPATH=python python3 -m unittest discover -s python/tests
```

### Real-world MIDI soak test

`tests/midi_soak_test.py` recursively finds every `.mid`/`.midi` under a root
(default `~/Music`), imports each to a project, renders it to WAV, and validates
the result (non-silent, no clipped samples, no DC offset, sane peak/RMS, optional
deterministic re-render). It is an on-demand harness (depends on local files and
renders full songs, so it is slow), not part of the smoke suites:

```bash
python3 tests/midi_soak_test.py --verbose --jobs 5 --timeout 1800 --keep --out /tmp/midi_soak
# subset: --match "Winter Widow|Never-Gonna"; faster: --no-determinism-check
```

Current baseline: all 31 files under `~/Music` pass (0 clipped samples, peaks
0.32–0.93, no silence/DC).

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
| `src/utils/` | I/O, diagnostics, import/export: `ProjectIO`, `PatchIO`, `MidiImporter`/`Exporter`, `GmPresetBank` (curated 128-preset GM bank), `ExportWorkflow`, `AutoSave`, `ProjectDiagnostics`, `ScriptIntegration`, `FileCompatibility`. |
| `include/` | Public headers mirroring `src/` structure. |
| `python/arachnotracker/` | Python SDK: `model.py`, `io.py`, `presets.py`, `templates.py`, `generative.py`, `plugins.py`, `commands.py`. |
| `tests/` | C++ smoke tests (`test_audio.cpp`, `test_tracker.cpp`). Uses raw `assert()`, no external test framework. |
| `python/tests/` | Python SDK tests (`test_python_sdk.py`). |
| `examples/projects/` | 7 goth showcase `.arachno` projects (darkwave/EBM/industrial/cinematic) + blank canvas; regenerated by `examples/scripts/generate_example_projects.py`. |
| `examples/scripts/` | Python example scripts. |
| `patches/factory/` | 98 factory `.arachnopatch` presets in category subfolders (`bass/`, `drums/`, `keys/`, `lead/`, `pads/`, `strings/`, `arps/`, `fx/`); see `patches/factory/README.md`. Generated entries come from `examples/scripts/generate_factory_patches.py`. |
| `patches/gm/` | 128 curated GM presets (see `patches/gm/README.md`); regenerate with `./build/ArachnoTracker --write-gm-presets patches/gm`. |

Performance notes:

- DSP hot paths use a deterministic Padé `fastTanh` instead of libm `std::tanh`
  (`src/audio/Synthesizer.cpp`); keep new per-sample math libm-free where possible.
  The same rule produced `fastSin01`/`fastCos01` (7th-order polynomial,
  |err| < 2e-4) for every per-sample oscillator/FM/FX-LFO sine — libm `std::sin`
  is control-rate only. The band-limited triangle is a mip-mapped wavetable
  (6 bands × 2048, built at static init with the original additive formula);
  the 2026-08 profiler run showed the old per-sample additive triangle at ~17%
  of total render time (up to 31 libm sin calls per sample per unison voice).
  SVF filter coefficients (g/k) are control-block stepped — never recompute
  `envelopeFor`/`exp2`/`tan` per sample in the voice loop. Combined effect on
  the reference box: example-project renders got 2.1–2.7× faster
  (darkwave_foundation 44.5s → 16.6s, cinematic 58.0s → 22.8s).
- **Every saturator/waveshaper in the voice chain must be transparent at zero
  drive and level-compensated when driven** (`oscDriveShape`, filter-input sat,
  the `asymmetricSaturation` call site). The 2026-08 "everything sounds fuzzy"
  bug was `asymmetricSaturation(sample, 0, 0)` degenerating to a full-scale
  `tanh` on every oscillator sample. When adding coloration stages, verify a
  clean sine still measures < 1% THD with all patch color fields at zero.
  The same rule applies to the master bus: the old unconditional `1.5x-0.5x³`
  output cubic (~2% pure 3rd-harmonic on every render) is now a linear region +
  knee soft-clip, and the master glue `tanh` only blends in on hot buses
  (smoothed peak follower ≥ 0.70). `testLayeredTimbreDsp` guards the < 1% THD
  clean-sine contract end to end.
- GUI audio: a dedicated producer thread (`GuiAudioProducerOps.cpp`) renders
  blocks into the thread-safe output queue (ALSA device buffer adaptive:
  20 ms default, self-escalating 20→48→96 ms on measured xrun streaks —
  PipeWire Bluetooth pulls are bursty and starve a small ring; override with
  `ARACHNO_ALSA_LATENCY_MS`, reset on performance-mode change — queue
  prime ~15 ms, queue cap ~100 ms in `GuiAudioRuntime.cpp`); the GUI thread
  only owns output open/close. Synchronization: ONE shared recursive mutex
  (`RealtimePlaybackSession::apiMutex` == `ApplicationSession::audioStateMutex()`)
  held for block renders and (briefly) for transport/audition/song mutations;
  `GuiAudioRuntime::audioLifecycleMutex_` for output open/write/close. Never
  hold either across waits, redraws, or view-model builds (`snapshot()` is
  deliberately unguarded — playback state self-locks; read-vs-read of the song
  needs no guard). Voice stealing is swap-and-pop — never `voices_.erase()`
  (memmoves ~300 KB per trailing Voice). Load-feedback smoothing is asymmetric
  (fast attack, slow release) so adaptive shedding engages within ~2 blocks
  without tier thrash. The top-panel telemetry shows producer mode
  (P-THR/P-GUI) and queue-health counters (s/c/x = starvation/congestion/xrun).
  Performance modes (AUTO/LIVE/BALANCED/HEAVY/CUSTOM) reshape queue/block
  geometry only — they never touch engine quality.

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
   - Adaptive quality/pressure systems (Synthesizer tiers, RealtimePlayback pressure)
     must be driven ONLY by measured signals (wall-clock DSP load, underrun risk) —
     never by raw track/voice/event counts. Degrade timbral detail first, note
     lengths/gates last. Notes are never dropped at the first pressure stage, and
     voice culling/stealing must respect (instrument, channel) ownership (see
     docs/LESSONS_LEARNED.md #6).

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
  - `.arachno` step lines end with a tolerant trailing-field chain: probability/retrigger fields followed by a note-off flag (`0|1`). Readers must keep ignoring trailing tokens they do not know; writers append new fields at the end without a version bump.
  - `.arachnopatch` `params` lines are positional (currently 159 values; `portamentoTime`/`portamentoLegato` at 146-147, `fmDecay`/`velocityToFm`/`monoMode` at 148-150, `oscBRatio`/`oscCRatio`/`oscDRatio`/`oscBDecay`/`oscCDecay`/`oscDDecay`/`velocityToDecay`/`keyTrackDecay` at 151-158). Readers gate new tails with `params.size() >= N`; writers append at the end without a version bump. `oscillators` lines carry 4 waveform names (readers tolerate 2).
  - Load paths (`PatchIO::loadPatch`, `ProjectIO` instrument lines) must stay transparent (save→load→save = identity). `applyCompetitionPresetQuality` is a creation-time polish only (demo songs, MIDI import) and must not be re-applied on load.
- `AppSettings` file format (`arachno_settings`, currently version 5 with `last_patch_dir` for patch-browser folder memory): new keys append at the end before `end_settings`, gated by version, so older files keep loading.
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
