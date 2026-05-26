# ArachnoTracker

ArachnoTracker is a Linux-first tracker workstation for darkwave / EBM-oriented composition:
pattern sequencing, a deep native synth engine, MIDI import/export, Python scripting, and audio rendering.

The project is in active development and already usable for real composition and export workflows.

## Highlights

- Pattern-based tracker engine with per-step note, velocity, gate, microtiming, probability, retrig, and automation.
- Native polyphonic synthesizer with four oscillators, FM, unison, sync, ring, PWM, filter envelopes, modulation, drive, lo-fi controls, transient shaping, and per-voice stereo processing.
- Patch lifecycle: create, clone, rename, import/export `.arachnopatch`, replace in-project patches.
- Realtime playback session model with transport state, loop/playhead tracking, follow behavior, and audition paths.
- Native WAV export plus MP3/OGG via `ffmpeg`/`avconv`, stem export, and MIDI export.
- MIDI import pipeline with track/instrument mapping and normalization logic.
- GUI workbench (X11) plus shell GUI fallback for headless environments.
- Python SDK for scripted composition, patch generation, and project transforms.

## Current Runtime Status

- Works well for many projects and moderate track counts.
- Heavy sessions (30+ dense tracks with complex patches/effects) can still show realtime lag/artifacts depending on CPU/audio path.
- Recent optimization passes improved synth hot-path behavior, render chunking, and UI redraw stability, but realtime backend work is still in progress.

## Build

```bash
cmake -S . -B build
cmake --build build
```

Requirements:
- C++17 compiler
- CMake
- Optional for MP3/OGG export: `ffmpeg` or `avconv`

## Quick Start

Launch default UI entrypoint:

```bash
./build/ArachnoTracker
```

Frontend modes:

```bash
./build/ArachnoTracker --gui [project.arachno]
./build/ArachnoTracker --gui-window [project.arachno]
./build/ArachnoTracker --gui-shell [project.arachno]
```

If no X11 display is available, the app falls back to GUI shell.

## Composition Flow (GUI)

1. Create or load a project (`NEW` / `LOAD`).
2. Select/arm instrument and octave.
3. Enter notes with tracker keys or piano widgets.
4. Build patterns, then arrange with order slots.
5. Switch between pattern-only playback and full-song playback.
6. Shape sounds in the synth designer (`PATCH` button).
7. Export mixdown/stems/MIDI.

Core note input keys:
- `Z S X D C V G B H N J M`
- `Q 2 W 3 E R 5 T 6 Y 7 U`

Core control keys:
- `Arrows`: cursor navigation
- `Shift+Arrows`: range selection
- `Ctrl+C/X/V`: clipboard
- `Space`: play/stop
- `F5`: play song
- `F6`: play pattern
- `Ctrl+Space`: preview step
- `Ctrl+0..8`: set octave
- `[` / `]`: cycle instrument

Theme toggle:
- `Ctrl+H` (`MS-DOS` and `HIGH CONTRAST`)

## Synth + Patch Workflow

The synth designer window supports:
- Oscillator A/B/C/D waveform selection and enable/disable
- Modulation and FX pages
- Audition from keyboard and clickable piano keys
- Patch import as new, patch load into selected instrument, and patch export

Patch CLI helpers:

```bash
./build/ArachnoTracker --export-patch demo.arachno 1 lead.arachnopatch
./build/ArachnoTracker --import-patch demo.arachno out.arachno lead.arachnopatch NewLead
./build/ArachnoTracker --replace-patch demo.arachno out.arachno 1 lead.arachnopatch NewLead
```

## MIDI Workflow

Import MIDI directly:

```bash
./build/ArachnoTracker --import-midi input.mid output.arachno [rows-per-beat] [pattern-rows]
```

Export MIDI:

```bash
./build/ArachnoTracker --export-midi project.arachno output.mid
```

Inspect import-ready track mappings from UI diagnostics/sidebar after import.

## Rendering and Export

Render project:

```bash
./build/ArachnoTracker --render project.arachno output.wav
./build/ArachnoTracker --render project.arachno output.mp3
./build/ArachnoTracker --render project.arachno output.ogg
./build/ArachnoTracker --render-stems project.arachno stems wav
```

Render demo templates:

```bash
./build/ArachnoTracker --list-demo-templates
./build/ArachnoTracker --demo demo.wav
./build/ArachnoTracker --demo demo_night.ogg night_drive
./build/ArachnoTracker --write-demo demo_project.arachno night_drive
```

Install encoder dependency (Ubuntu/Debian):

```bash
sudo apt-get install ffmpeg
```

## Python Scripting SDK

Generate/edit projects with pure-stdlib Python support:

```bash
PYTHONPATH=python python3 examples/scripts/python_ebm_sketch.py
./build/ArachnoTracker --validate python_ebm_sketch.arachno
./build/ArachnoTracker --render python_ebm_sketch.arachno python_ebm_sketch.wav
```

Module CLI:

```bash
PYTHONPATH=python python3 -m arachnotracker new-ebm community_starter.arachno --title "Community Starter"
PYTHONPATH=python python3 -m arachnotracker patch-plugin examples/scripts/industrial_patch_plugin.py industrial_bass.arachnopatch --name IndustrialBass
```

Detailed API docs:
- [docs/PYTHON_API.md](docs/PYTHON_API.md)

## CLI Reference

Show canonical usage:

```bash
./build/ArachnoTracker --help
```

Useful inspection commands:

```bash
./build/ArachnoTracker --project-info project.arachno
./build/ArachnoTracker --validate project.arachno
./build/ArachnoTracker --arrangement project.arachno
./build/ArachnoTracker --instruments project.arachno
./build/ArachnoTracker --stats project.arachno
./build/ArachnoTracker --actions
./build/ArachnoTracker --shortcuts
./build/ArachnoTracker --palette
```

Command editing:

```bash
./build/ArachnoTracker --edit in.arachno out.arachno "move 4 1" "inst 2" "note C4 0.8"
./build/ArachnoTracker --edit-file in.arachno out.arachno commands.txt
./build/ArachnoTracker --interactive in.arachno out.arachno
```

## Project Format

- `.arachno`: versioned, line-oriented project format (diff-friendly).
- Stores song metadata, timing, tracks, instruments/patches, patterns, order, and step-level data.
- `.arachnopatch`: portable native synth patch format.

## Performance Notes (Important)

For dense sessions, realtime behavior depends heavily on patch complexity and output backend path.

Recommended today:
- Use `--render` (offline) for final-quality bounce when realtime is stressed.
- Reduce extreme unison/chorus/transient-heavy usage on many simultaneous voices.
- Prefer 48k sample rate for realtime preview unless you need higher.
- Keep MP3/OGG export for final pass, not realtime monitoring.

Realtime backend modernization is on the active roadmap to improve heavy-project stability without quality loss.

## Testing

```bash
ctest --test-dir build --output-on-failure
```

## Example Projects

See curated templates in:
- [examples/projects/README.md](examples/projects/README.md)

For GUI/TUI integration, inspect the canonical action registry:

```bash
./build/ArachnoTracker --actions
./build/ArachnoTracker --shortcuts
./build/ArachnoTracker --palette note
```

Examples:

```bash
./build/ArachnoTracker --edit demo.arachno generated.arachno \
  "fill-scale 1 0 12 4 C4 minor 1 0.75 0.8" \
  "euclid 0 0 16 5 C2 0 0.9 0.5" \
  "move 0 1" \
  "param cutoff 0.95" \
  "param vibrato 6"
```

Percussion examples:

```bash
./build/ArachnoTracker --edit demo.arachno drumfills.arachno \
  "move 14 1" \
  "probability 0.65" \
  "retrig 3 0.12 0.72"
```

Supported synth automation names include `mix`, `detune`, `pulse_width`, `pwm`, `fm`, `fm_ratio`, `chorus`, `chorus_rate`, `chorus_depth`, `unison`, `unison_detune`, `spread`, `sub`, `noise`, `cutoff`, `resonance`, `filter_env`, `pitch_env`, `pitch_decay`, `click`, `transient_noise`, `transient_decay`, `lfo_rate`, `vibrato`, `tremolo`, `ring_mod`, `hard_sync`, `drive`, `bitcrush`, `sample_reduce`, `highpass`, `gain`, `pan`, `attack`, `decay`, `sustain`, `release`, and filter envelope fields such as `filter_attack`.

## Realtime Playback Contract

The GUI foundation now includes a `RealtimePlaybackSession` API for transport and preview workflows:

- `play`, `pause`, `stop`, and `seekRows` for transport controls.
- `setLoopRows` and `clearLoop` for pattern/selection loop playback.
- `snapshot` for GUI state: transport state, current row, order slot, pattern row, loop status, follow-cursor setting, preview activity, and sample rate.
- `previewInstrument`, `previewStep`, and `previewNote` for browser auditioning and focused pattern editing.
- `auditionPatch`, `auditionDrumPatch`, `auditionInstrument`, and `auditionStep` for structured audition requests/results with user-visible messages and errors.
- `renderAuditionClip` for deterministic short audio previews suitable for preset browsers and waveform thumbnails.
- `render` for block-based stereo audio generation suitable for a future Linux audio-device backend.

## Linux Audio Runtime Contract

The GUI can configure realtime output through `AudioRuntimeSession` before native device adapters are implemented:

- `defaultLinuxAudioDeviceCatalog` describes JACK, PipeWire, ALSA, and a headless dummy preview target.
- `validateAudioRuntimeSettings` normalizes backend, device, sample rate, buffer frames, and period count before starting audio.
- `estimateAudioLatencyMs` gives export/settings dialogs a deterministic latency preview.
- `renderBlock` bridges an active runtime into `RealtimePlaybackSession::render` and tracks processed frames.
- `AudioRuntimeHealth` exposes configured/active state, selected backend/device, latency, processed blocks, processed frames, underrun count, and last error.

The dummy backend is available for tests and non-device previews. JACK, PipeWire, and ALSA entries are represented now so the GUI contract is stable; real native adapters remain the later Linux studio integration step.

## Application Session State

The GUI can bind to `ApplicationSession` as the high-level project controller:

- `snapshot` returns project path, dirty state, last user-visible message, diagnostics, editor view model, playback state, and audio runtime health in one read-only structure.
- `applyEditorCommand` dispatches existing tracker commands and updates dirty state, diagnostics, playback song binding, and last-message/error state.
- `newProject`, `loadProjectFile`, `saveProjectFile`, and `saveProjectFileAs` provide non-throwing project lifecycle operations for dialogs and menus.
- `configureAudioRuntime`, `startAudioRuntime`, `renderAudioRuntimeBlock`, and `stopAudioRuntime` provide GUI-safe runtime lifecycle calls around the Linux audio contract.
- `previewCursorStep` connects the active editor cell to the realtime preview contract.

This gives the future GUI a stable state boundary before visual widgets are introduced.

## Settings Persistence

The GUI foundation includes a versioned `AppSettings` model and settings file IO:

- Recent projects are normalized with most-recent-first ordering and duplicate removal.
- Export defaults store directory, audio format, sample rate, and stem-render preference.
- UI layout preferences store grid row count, inspector/browser visibility, and follow-playback behavior.
- Audio runtime preferences store backend (`auto`/`pipewire`/`jack`/`alsa`/`dummy`), device id, sample rate, buffer frames, periods, and realtime flags.
- Shortcut overrides merge with the default editor shortcut map and reuse conflict validation.
- `ApplicationSession` can load, save, expose, and apply settings while keeping playback follow-state synchronized.
- Named sync checkpoints (event sequence + task id) persist in settings for frontend resume flows.

The settings format is line-oriented and standard-library-only, matching the rest of the headless foundation.

## Structured Diagnostics

Project validation returns `ProjectDiagnostic` entries designed for both CLI and GUI use:

- `severity` separates warnings from blocking errors.
- `code` is stable for filtering, testing, localization, and targeted help.
- `location` can point to an order slot, pattern, row, track, or instrument.
- `formatDiagnostics` renders readable CLI output with counts, codes, locations, and messages.
- `ApplicationSession::snapshot` exposes diagnostics plus error/warning counts for status bars and diagnostics panels.

This keeps user-facing errors out of raw exception text and gives future views enough structure to highlight the affected project area.

## Script Integration Surface

The application core can now integrate with community scripts without hard-wiring process execution into the engine:

- `buildPythonNewProjectCommand` and `buildPythonPatchPluginCommand` produce structured commands a GUI can launch.
- `inspectScriptArtifact` identifies and validates generated `.arachno`, `.arachnopatch`, and `.arachno-edit` files.
- `ApplicationSession::importScriptArtifact` loads generated projects, imports generated patches as new instruments, or applies generated command files.
- Script command files skip comments and blank lines, making generated edit scripts easy to inspect and version.

The GUI can own process launching, progress UI, and script logs while the core owns safe artifact validation and import.

## File Compatibility

The application core includes file preflight checks for GUI load/import flows:

- `inspectTrackerFile` detects projects, patches, settings, and command files from their extension.
- Project, patch, and settings inspectors read version headers before attempting a full load.
- Reports include existence, readability, detected version, supported version, compatibility, loadability, warnings, and error text.
- Legacy/minimal project and patch files are covered by compatibility tests.

This lets a GUI show precise recovery messages before replacing the current session.

## Autosave And Recovery

The app core now exposes an autosave/recovery contract for crash-safe GUI sessions:

- `defaultRecoveryFileName` and `recoveryPathForProject` create deterministic recovery targets for saved and untitled projects.
- `inspectRecoveryFile` checks whether a recovery file exists and can be loaded before a GUI prompts the user.
- `saveRecoveryFile`, `loadRecoveryFile`, and `clearRecoveryFile` provide the file operations without requiring GUI code to know the project format.
- `ApplicationSession` can save, restore, and clear recovery snapshots while preserving dirty-state and user-message semantics.

This gives the future startup screen enough structure to offer restore/discard choices without risking the current project state.

## Project Lifecycle Planning

Before GUI widgets mutate the session, they can ask the core for lifecycle plans:

- `buildUnsavedChangesPrompt` returns localized-ready prompt data when the current session is dirty.
- `preflightOpenProject` combines project compatibility checks with recovery snapshot inspection.
- `planProjectLifecycleTransition` models new/open/close/quit/restore decisions, including save, discard, cancel, save-as-needed, and recovery-offer states.

This keeps dialog behavior deterministic and testable before toolkit-specific window code exists.

## Application Action Bridge

GUI menus, toolbars, shortcut handlers, and command palettes can use one action surface:

- `applicationActions` exposes project, transport, audio runtime, preview, and recovery actions.
- `buildApplicationActionPalette` combines application actions with editor command-palette entries.
- `buildAppActionSchema` describes required and optional action inputs such as paths, notes, rows, tracks, velocities, choices, and ranges.
- `validateAppActionParameters` returns field-level errors for missing, malformed, out-of-range, or invalid-choice inputs before execution.
- `buildCommandForAppAction` converts structured parameter maps into concrete tracker command text.
- `executeAppAction` dispatches concrete requests to `ApplicationSession`, including editor commands, playback, audio runtime control, save/open, and recovery actions.
- Audio runtime actions include filtered device listing (`backend`, `only_available`), configure/status/render-test/start/stop, and simulated-underrun diagnostics for GUI testing.
- Task actions include `task.cancel` (`task_id`, optional message) and `task.clear_finished` so GUI task panels can manage task lifecycle without direct manager calls.
- Session actions include `session.snapshot` (`grid_start_row`, `grid_row_count`) for full state sync, `session.events` (`since`, `drain`, `max_events`, `include_snapshot`, snapshot grid options) for incremental event sync, checkpoint actions (`session.checkpoint.save/advance/load/clear/list`) for resumable frontend cursors, and `session.sync` for one-call checkpoint-based delta/snapshot sync (including `create_if_missing` bootstrap).
- Export actions include `export.mixdown`, `export.stems` (`format`), and `export.midi` (`ticks_per_quarter`) with structured export result payloads.
- Script actions include `script.import` (`name_override`) with structured script artifact import payloads for project/patch/command-file flows.
- Results report project changes, editor-state changes, unsaved-change decisions, save-as requirements, recovery offers, and message severity (`info`/`warning`/`error`) for status bars.
- `renderApplicationActionResult` provides a structured debug rendering of action outcomes, including audio runtime, device catalogs, and render-test diagnostics.
- `serializeApplicationActionResult` provides a JSON-style payload for GUI/IPC adapters that need machine-readable action responses.
- `session.events` responses now include an `event_delta` summary (domain counts, truncation, returned/dropped totals) so GUI layers can route partial refreshes without diffing raw event arrays. When a limited event window truncates, the response can include a snapshot fallback to preserve UI consistency.
- `session.events` and `session.sync` responses include an `event_cursor` (`requested_since`, `recommended_since`, returned range, truncation/snapshot flags) so frontend polling loops can persist cursor state deterministically.
- `session.sync` wraps checkpoint lookup + event polling + optional snapshot fallback + checkpoint advancement, with `create_if_missing` support for first-run frontend bootstrap flows. Checkpoints carry project path/fingerprint guards so stale checkpoints can be detected and recovered via snapshot fallback.
- `session.sync` accepts `stale_policy` (`snapshot_fallback`/`error`/`ignore`) and reports `sync_checkpoint_update` status (`advanced`, `skipped_update_disabled`, `skipped_unsafe_delta`, `skipped_stale_policy`, `failed`) so GUI sync loops can safely decide when to commit cursors.
- `session.sync` also returns `suggested_sync_checkpoint` with `safe_to_commit`, allowing frontends to run with `update_checkpoint=false` and commit checkpoints later using explicit guidance instead of heuristics.
- `session.checkpoint.advance` supports optimistic compare-and-swap guards (`expected_event_sequence`, `expected_task_id`) so multi-loop or multi-client GUI flows can commit checkpoint updates without stomping newer cursors.
- `session.sync` and checkpoint load flows expose explicit compatibility statuses (`ok`, `missing`, `stale_path`, `stale_fingerprint`) so GUI recovery UX can branch without parsing free-form error strings.

Actions that require a path or concrete command text stay visible in the palette, but execution returns structured errors until the GUI supplies the missing input.

## Application Task Tracking

Long-running GUI workflows can use `AppTaskManager` for predictable progress state:

- Tasks have stable IDs, kind, title, detail, state, progress, message/error text, and output file lists.
- States cover running, succeeded, failed, and canceled work.
- `ApplicationSession::snapshot` includes all tasks plus active-task counts for status bars and task panels.
- Export workflows expose progress callbacks, and `exportProjectWithTask` maps those callbacks into task progress while preserving the existing structured `ExportResult`.
- Audio runtime actions (`audio.runtime.configure`, `audio.runtime.start`, `audio.runtime.stop`) also emit `AppTaskKind::Audio` tasks so GUI task panels can report runtime status consistently.

`AppEventLog` and `ApplicationSession::eventsSince` provide a GUI-friendly change stream for project, editor, settings, playback, audio runtime, message, diagnostics, script, recovery, and task events. A GUI can poll or drain events after applying actions instead of rebuilding assumptions about what changed.

`AppAsyncTaskRunner` adds a cancellable background-job contract on top of the same task snapshots. Jobs receive an `AppAsyncJobContext` for progress updates, output files, cancellation checks, success, failure, and user-requested cancellation. This gives export dialogs, script runners, plugin scanners, and recovery flows one progress/status contract without blocking the GUI thread.

`ApplicationSession::cancelTask` and `ApplicationSession::clearFinishedTasks` provide a GUI-safe session boundary for task list controls without exposing toolkit code to direct task-manager mutation.

## Python Composition SDK

Create projects directly from Python:

```python
import arachnotracker as at

song = at.Song(title="Python EBM Sketch", bpm=132)
drums = song.add_track("Drums")
kick = song.add_instrument(at.presets.ebm_kick())

pattern = at.Pattern("Opening", rows=16, tracks=len(song.tracks))
step = pattern.step(0, drums)
step.midi = at.note_name_to_midi("C2")
step.instrument = kick
step.velocity = 1.0

song.add_pattern(pattern)
song.order = [0]
at.save_project(song, "python_song.arachno")
```

Use higher-level composition helpers when you do not want to place every step manually:

```python
import arachnotracker as at

song = at.darkwave_ebm_starter("Community Starter")
pattern = song.patterns[0]

at.generative.fill_euclidean(
    pattern,
    track=0,
    start_row=1,
    steps=16,
    pulses=5,
    note="C5",
    instrument=2,
    velocity=0.35,
    gate=0.12,
    probability=0.7,
)

at.save_project(song, "community_starter.arachno")
```

Write patch-generator plugins as ordinary Python modules:

```python
from arachnotracker import SynthPatch, Waveform

def create_patch(name="Plugin Bass"):
    return SynthPatch(name=name, oscillator_a=Waveform.SAW, drive=0.45)
```

Load that plugin from a script:

```python
patch = at.load_patch_plugin("examples/scripts/industrial_patch_plugin.py")
at.save_patch(patch, "industrial_bass.arachnopatch")
```

Transform existing projects and patches:

```python
song = at.load_project("community_starter.arachno")
song.bpm = 136
for pattern in song.patterns:
    for step in pattern.steps.values():
        if step.midi is not None:
            step.velocity *= 0.92
at.save_project(song, "community_starter_136.arachno")

patch = at.load_patch("industrial_bass.arachnopatch")
patch.chorus_mix = 0.16
at.save_patch(patch, "industrial_bass_wide.arachnopatch")
```

This is a patch-generation plugin layer for composition and preset design. Native Linux DSP plugin hosting such as LV2 or CLAP is still a later milestone.

See [docs/PYTHON_API.md](docs/PYTHON_API.md) for the full scripting API, including project models, synth patch fields, generative helpers, patch plugins, and command-script generation.

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

## Next Big Pieces

To become a truly powerful compositional tool, the next layers should be:

- A full-screen keyboard-first tracker TUI/GUI on top of the editor session.
- Deeper scripting bindings for arrangement transforms, humanization, batch project migration, and community extension packs.
- MIDI input/output and controller mapping.
- Native JACK/PipeWire/ALSA adapters behind the existing Linux audio runtime contract.
- Plugin hosting, especially LV2 and CLAP.
- More synthesis: modulation matrix, additional filter models, wavetable import, sampler zones, drum lanes, macros, and performance controls.
