# Architecture

## Top-Level Project Map

- `src/core`: tracker/domain model and composition primitives.
- `src/audio`: synth DSP, realtime playback session, offline audio rendering, runtime backend model.
- `src/ui`: application session, action layer, view-model, settings/tasks/events.
- `src/ui/gui`: GUI rendering, input dispatch, interaction contexts, runtime bridge.
- `src/cli`: command-line front-end parsing/rendering.
- `src/utils`: project/patch/MIDI I/O, diagnostics, export workflow, scripting integration.
- `python/arachnotracker`: Python SDK for generating/editing `.arachno` projects and `.arachnopatch` files.
- `include`: public headers grouped by subsystem.
- `tests`: C++ and Python tests.

## Runtime Layers

1. **Domain (`core`)**
   - Owns `Song`, `Pattern`, `PatternStep`, `Track`, `Instrument`, timing/grid rules, and editor command semantics.

2. **Audio (`audio`)**
   - `Synthesizer`: native voice engine + modulation/effects.
   - `RealtimePlaybackSession`: transport, audition, row mapping, event scheduling.
   - `AudioEngine`: offline render/mix/stems.
   - `AudioRuntimeSession`: backend capability/config/health model.

3. **Application (`ui`)**
   - `ApplicationSession`: central orchestrator.
   - `AppActions`: structured action API.
   - `EditorViewModel`: GUI-facing snapshot data.
   - `AppTask`, `AppEvent`, `AppSettings`: lifecycle/status/preferences.

4. **Frontend (`ui/gui`)**
   - Draw + event operation files (`Gui*Ops.cpp`) compose the X11/shell GUI behavior.
   - `GuiWindow.cpp` coordinates bootstrap and lifecycle bindings.

## Audio Data Flow

1. Song/pattern data -> scheduled note events.
2. Events feed `Synthesizer::noteOn(...)`.
3. `Synthesizer::render(...)` fills block buffers.
4. Mix bus limiting/headroom shaping.
5. Realtime output (`GuiAudioRuntime`) and/or offline exporter.

## Snapshot/State Flow

- GUI requests session snapshot via action/session bridge.
- `ApplicationSession::snapshot(...)` returns:
  - editor view-model
  - playback state (+ synth telemetry)
  - audio runtime health
  - message/task/diagnostic state

## Contracts Worth Protecting

- `.arachno` and `.arachnopatch` format compatibility.
- CLI command/flag behavior from `src/main.cpp`.
- Deterministic sequencing semantics (row timing, order traversal, retrig/probability behavior).
- Stable shortcut/action identifiers used by GUI and automation.

