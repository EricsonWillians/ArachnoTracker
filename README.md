# ArachnoTracker

ArachnoTracker is becoming a Linux-first compositional tracker: pattern-based sequencing, a built-in synthesizer, and offline export for finished audio.

The current codebase is a headless foundation for the engine. It renders a demo song from tracker patterns into stereo audio and exports WAV natively, with MP3 and OGG export through common Linux encoders.

## What Works Now

- Value-based song model with tracks, instruments, patterns, rows, steps, note gates, microtiming, velocity, gain, and pan.
- Built-in polyphonic synthesizer with sine, square, saw, triangle, noise, dual oscillator mix, detune, pulse width/PWM, FM phase modulation, unison/spread, per-voice chorus, sub oscillator, noise, low-pass/high-pass shaping, resonance, filter envelope, pitch envelope, percussion transient click/noise, LFO vibrato/tremolo, ring modulation, hard sync, drive, bitcrush/sample-rate reduction, ADSR envelope, and per-patch pan/gain.
- Offline renderer that schedules pattern events into sample buffers.
- Realtime playback/preview session contract for GUI transport controls, loop playback, seeking, follow-cursor state, instrument auditioning, step preview, and block rendering.
- Linux audio runtime contract for backend/device selection, buffer validation, latency estimates, render-block bridging, and underrun health reporting.
- Dedicated audition requests/results for patch, drum, instrument, and step previews, plus deterministic preview clip rendering for future browsers and waveform thumbnails.
- Versioned `.arachno` project save/load.
- GUI-facing application session with project path, dirty state, editor view model, playback snapshot, diagnostics, last-message reporting, load/save operations, and command dispatch.
- Versioned application settings for recent projects, export defaults, UI layout preferences, follow-playback behavior, shortcut overrides, and Linux audio runtime preferences.
- Structured project diagnostics with severity, stable codes, target locations, formatted CLI output, and GUI-ready error/warning counts.
- File compatibility inspection for projects, patches, settings, and command files before GUI load/import operations.
- Autosave/recovery helpers for deterministic recovery paths, loadable recovery inspection, snapshot saving, restoring, and cleanup.
- Project lifecycle planning for unsaved-change prompts, open-file preflight, recovery offers, save-as requirements, and close/quit decisions.
- Application action bridge for project, transport, recovery, and editor actions with typed execution results and GUI palette entries.
- Structured action parameter schemas for GUI forms, command-palette prompts, typed editor command construction, and path-required actions.
- Application task tracking for long-running export, render, script, recovery, and plugin-scan workflows with progress, outputs, and errors.
- GUI-facing session event stream for project/editor/settings/playback/message/task changes.
- Cancellable async task runner for background render, export, script, recovery, and plugin workflows.
- Command-based pattern editing suitable for terminal workflows and future TUI/GUI integration.
- Discoverable editor action registry for future GUI menus, command palettes, toolbar actions, and shortcut defaults.
- Shortcut registry with normalization, lookup, and conflict detection for customizable keymaps.
- Command palette model that combines actions, shortcuts, search filtering, and enabled/disabled state.
- Structured pattern grid snapshots with cursor and selection metadata for future GUI/TUI rendering.
- Read-only editor view model for GUI sidebars, order lists, track strips, instrument browsers, status bars, and active pattern grids.
- Inspector-ready active step, selection, and clipboard summaries for GUI property panels and paste previews.
- Non-throwing editor command results for GUI-friendly error display and project dirty-state tracking.
- Native 16-bit stereo WAV export.
- MP3 and OGG export when `ffmpeg` or `avconv` is installed.
- GUI-facing export workflows with structured requests/results for mixdowns, stems, and MIDI.
- Per-track stem export for downstream mixing and arrangement workflows.
- Standard MIDI file export for DAWs, hardware sequencers, and external Linux synth chains.
- Python composition SDK for generating, loading, and transforming `.arachno` projects, `.arachnopatch` files, command scripts, and patch-generator plugins.
- Script integration surface for Python SDK command construction, generated project/patch/command-file inspection, and GUI-safe artifact import.
- CTest smoke tests for tracker timing, audio rendering, and WAV export.

## Building

```bash
cmake -S . -B build
cmake --build build
```

The current engine has no mandatory third-party dependency beyond a C++17 compiler and CMake.

Run the GUI (tries X11 window first, falls back to shell if unavailable):

```bash
./build/ArachnoTracker
```

Force a specific frontend:

```bash
./build/ArachnoTracker --gui-window demo.arachno
./build/ArachnoTracker --gui-shell demo.arachno
```

Window GUI tracker shortcuts:

- `Z S X D C V G B H N J M` for chromatic note entry in the armed octave.
- `Q 2 W 3 E R 5 T 6 Y 7 U` for the next octave row.
- `[` / `]` cycle armed instrument, `Alt+0..9` select instrument directly.
- `-` / `+` change octave, `,` / `.` change note velocity.
- `Ctrl+0..8` sets octave instantly.
- Sidebar has clickable octave selector (`0..8`, `-`, `+`) and a clickable piano keyboard that inserts notes at cursor.
- `Return` inserts the currently selected sidebar/piano note at cursor.
- `Tab` toggles step-advance after note entry.
- `Arrows` move the cursor, `Backspace`/`Delete` clear step.
- `Shift+Arrows` expands a rectangular selection from the anchor point.
- `Ctrl+C` / `Ctrl+X` / `Ctrl+V` copy-cut-paste selection, `Ctrl+A` selects whole active pattern.
- `Space` play/stop, `Shift+Space` pause, `Ctrl+Space` preview cursor step.
- Mouse: left-click moves cursor, left-drag creates selection, middle-click previews the pointed step.
- Mouse: `Alt+left-drag` paints notes with current armed instrument/velocity.
- Mouse: wheel scrolls rows, `Ctrl+wheel` resizes active pattern row count.
- Mouse: click transport controls, order slots, track header controls (select/mute/solo), instrument list, and pattern-row controls.
- Top bar includes quick `NEW`, `OPEN`, `SAVE`, and `EXPORT` actions.
- Theme system includes `MS-DOS` and `HIGH CONTRAST` modes (toggle with `Ctrl+H`).
- Mouse: drag vertical/horizontal split bars to resize tracker grid and side panels.
- Live playback now streams to Linux audio using `aplay` when available (float output first, S16 fallback).

Python scripting uses only the Python standard library:

```bash
PYTHONPATH=python python3 examples/scripts/python_ebm_sketch.py
./build/ArachnoTracker --validate python_ebm_sketch.arachno
./build/ArachnoTracker --render python_ebm_sketch.arachno python_ebm_sketch.wav
```

The SDK also has a small module CLI:

```bash
PYTHONPATH=python python3 -m arachnotracker new-ebm community_starter.arachno --title "Community Starter"
PYTHONPATH=python python3 -m arachnotracker patch-plugin examples/scripts/industrial_patch_plugin.py industrial_bass.arachnopatch --name IndustrialBass
```

## Exporting Audio

Render the built-in demo song:

```bash
./build/ArachnoTracker --demo demo.wav
./build/ArachnoTracker --demo demo.mp3
./build/ArachnoTracker --demo demo.ogg
./build/ArachnoTracker --list-demo-templates
./build/ArachnoTracker --demo demo_factory.wav factory_pulse
./build/ArachnoTracker --demo demo_night.ogg night_drive
```

WAV export is built into ArachnoTracker. MP3 and OGG export use `ffmpeg` first, then `avconv` as a fallback:

```bash
sudo apt-get install ffmpeg
```

The application core also exposes `ExportRequest` / `ExportPreflight` / `ExportResult` workflows for future GUI export dialogs. Supported targets are mixdown audio, per-track stems, and MIDI, with structured output file metadata, expected output paths, total work estimates, progress callbacks, and non-throwing error reporting.

Inspect the demo project:

```bash
./build/ArachnoTracker --info
```

## Project Files

Create a reusable project file:

```bash
./build/ArachnoTracker --write-demo demo.arachno
./build/ArachnoTracker --write-demo factory_pulse.arachno factory_pulse
```

Load curated examples directly:

```bash
./build/ArachnoTracker --gui examples/projects/darkwave_foundation.arachno
./build/ArachnoTracker --gui examples/projects/factory_pulse.arachno
./build/ArachnoTracker --gui examples/projects/night_drive.arachno
./build/ArachnoTracker --gui examples/projects/ebm_percussion_lab.arachno
./build/ArachnoTracker --gui examples/projects/cinematic_darkwave_builder.arachno
./build/ArachnoTracker --gui examples/projects/composition_starter_blank.arachno
```

Project pack details live in [examples/projects/README.md](examples/projects/README.md).

Inspect and render a saved project:

```bash
./build/ArachnoTracker --project-info demo.arachno
./build/ArachnoTracker --validate demo.arachno
./build/ArachnoTracker --arrangement demo.arachno
./build/ArachnoTracker --instruments demo.arachno
./build/ArachnoTracker --stats demo.arachno
./build/ArachnoTracker --actions
./build/ArachnoTracker --shortcuts
./build/ArachnoTracker --palette paste
./build/ArachnoTracker --gui demo.arachno
./build/ArachnoTracker --export-patch demo.arachno 1 bright.arachnopatch
./build/ArachnoTracker --import-patch demo.arachno with-patch.arachno bright.arachnopatch BrightLead
./build/ArachnoTracker --show demo.arachno 0 0 32
./build/ArachnoTracker --render demo.arachno demo.wav
./build/ArachnoTracker --render-stems demo.arachno stems wav
./build/ArachnoTracker --export-midi demo.arachno demo.mid
```

The project format is line-oriented, versioned, and diff-friendly. It stores title, tempo, sample rate, tracks, instruments, darkwave/EBM-oriented synth patch parameters, patterns, steps, order list, gates, velocities, microtiming, and automation values.

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

Start the session-driven GUI workbench shell (action palette, schemas, sync/events, transport):

```bash
./build/ArachnoTracker --gui demo.arachno
```

Supported editor commands:

- `pattern N`
- `move ROW TRACK`
- `up [N]`, `down [N]`, `left [N]`, `right [N]`
- `select ROW TRACK ROWS TRACKS`
- `copy`
- `cut`
- `paste [ROW] [TRACK]`
- `clear-selection`
- `undo`
- `redo`
- `note C4 [VELOCITY]`
- `inst N`
- `gate ROWS`
- `probability VALUE` or `probability clear`
- `retrig COUNT [SPACING_ROWS] [VELOCITY_DECAY]`
- `transpose SEMITONES`
- `transpose SEMITONES track`
- `tempo BPM`
- `rows-per-beat N`
- `title TEXT`
- `author TEXT`
- `description TEXT`
- `notes TEXT`
- `new-pattern NAME ROWS [TRACKS]`
- `clone-pattern [NAME]`
- `delete-pattern [PATTERN]`
- `pattern-name NAME`
- `append-order [PATTERN]`
- `insert-order INDEX [PATTERN]`
- `remove-order INDEX`
- `set-order PATTERN...`
- `new-track NAME`
- `duplicate-track SRC [NAME]`
- `delete-track TRACK`
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
- `instrument-wave INST A|B|C|D sine|square|saw|triangle|noise`
- `instrument-param INST NAME VALUE`
- `fill-scale TRACK START COUNT STRIDE ROOT SCALE INST [VELOCITY] [GATE]`
- `euclid TRACK START STEPS PULSES ROOT INST [VELOCITY] [GATE]`
- `param NAME VALUE`
- `param-clear [NAME|*]`
- `view` in interactive mode
- `instruments` in interactive mode
- `clear` or `rest`
- `write` and `quit` in interactive mode

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
