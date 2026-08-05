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
     - Per-voice FX: BBD chorus/ensemble, tape delay, and a Freeverb-class reverb
       (8 damped feedback combs + 4 allpasses per channel) whose tails ring out past
       note release (voices stay alive for the FX tail window). Reverb topology is
       mode-dependent: offline rendering runs the full per-voice networks (per-patch
       tails), while realtime playback routes each voice's send (output x reverbMix)
       into ONE shared Freeverb bus with send-weighted, block-smoothed parameters —
       per-voice reverb networks were ~50% of realtime DSP cost at high polyphony
       and pushed dense mixes into underruns. The shared bus keeps ringing after
       voices die, so realtime voices only stay alive for their per-voice delay tail.
     - Portamento/glide between successive notes per (instrument, channel), driven by
       patch `portamentoTime`/`portamentoLegato`; `monoMode` gives classic monosynth
       behavior (new note steals same instrument+channel voices); `fmDecay`/
       `velocityToFm` provide the DX7-style FM modulator envelope (metallic clang
       decays to warm body, velocity-scaled).
   - Realtime playback uses load-adaptive quality tiers driven only by measured DSP
     load (raw track/voice/event counts never downgrade quality on their own);
     offline rendering (`AudioEngine`) forces full quality via
     `Synthesizer::setOfflineRendering(true)` (no voice-limit reduction, no FX
     stripping, no wall-clock load feedback).
   - `RealtimePlaybackSession` has a matching measured-load-only pressure model
     with three stages (underrun-risk / DSP-load thresholds, risk calibrated to
     the synth's risk formula): High (0.45 / 62%)
     trims timbral detail only (unison, FX mixes) and never touches envelopes,
     gates, or note scheduling; Critical (0.78 / 74%) adds low-priority event
     thinning and moderate envelope tightening; Emergency (0.97 / 86%) is the
     only stage that aggressively drops events or shortens gates. Event quotas
     are generous safety valves (64 events/frame, 96/segment at rest) so dense
     imported chords never lose notes. Inside the synth, unison-stack shedding
     and spatial-FX (delay/reverb) gating are likewise keyed to measured load
     only — raw voice-count budgets are forbidden because they made dense mixes
     collapse to thin single-oscillator sounds on idle machines.
   - The mix bus ends with a gentle "smile" EQ (one-pole split bands: low lift,
     mid dip, high cut) before the limiter stage.
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

## Realtime Threading Model

- A dedicated **producer thread** (`GuiAudioProducer`, `src/ui/gui/GuiAudioProducerOps.cpp`)
  renders blocks via `renderMainRealtimeAudioBlocks` into the thread-safe output
  ring queue; drain threads (`GuiAudioRuntime`, ALSA/aplay) memcpy the queue to
  the device. If producer startup fails, the GUI falls back to its old inline
  render loop (`processMainRealtimeAudio` still owns output open/close either
  way). Before 2026-08 all audio rendered synchronously on the GUI thread, so
  30 Hz full-window redraws starved playback on dense songs.
- **Single shared audio-state mutex** (`RealtimePlaybackSession::apiMutex`,
  exposed as `ApplicationSession::audioStateMutex()`, recursive): held by the
  producer for each block render, and briefly by transport/audition calls and
  editor/project mutations (`applyEditorCommand`, `replaceSong`,
  `importScriptPatch`). Never hold it across waits or redraws.
  `GuiAudioRuntime::audioLifecycleMutex_` serializes output open/write/close.
  Both snapshot paths are lock-free so GUI polling never queues behind a
  producer block render: `RealtimePlaybackSession::snapshot()` reads atomics +
  a published immutable row-map lookup, and `ApplicationSession::snapshot()`
  builds the view model without the audio mutex (song writes are GUI-thread
  only).
- Output-side robustness (`GuiAudioRuntime` ALSA writer): once primed the
  writer streams any queued audio (unprime only on true emptiness, re-prime
  capped at 2 chunks), concealment engages after ~12 ms of drought (before the
  ~20 ms device buffer runs dry), and the device buffer self-escalates
  20->48->96 ms on measured xrun streaks so bursty PipeWire/Bluetooth pulls
  stop underrunning the ring (`ARACHNO_ALSA_LATENCY_MS` pins a fixed value).
  `ARACHNO_SPIKE_LOG=1` logs producer/GUI-phase latency spikes with monotonic
  timestamps (include/SpikeLog.h).
- The AUTO/LIVE/BALANCED/HEAVY/CUSTOM performance modes reshape GUI-side
  queue/block geometry only (latency vs. robustness); they never touch engine
  quality, which is governed by measured DSP load.


## MIDI Import Voicing

- `MidiImporter` groups notes into lanes (track/channel/program), then voices each
  lane from the curated General MIDI preset bank (`src/utils/GmPresetBank.cpp`,
  `gmPresetForProgram`): 128 hand-tuned patches targeting 80s/90s synth character
  (FM EPs/bells, Juno ensemble strings/pads, portamento mono basses, sync leads).
  Channel-10 lanes are split per drum class (`drumClassForNote`: kick/snare/clap/
  hat/tom/cymbal/percussion) and voiced from `gmDrumClassPreset`.
- Lane-name heuristics ("bass"/"lead"/"pad"/"arp"/"fx") refine the preset. The
  bank presets are used as-is (no creation-time polish — its vintage/drift floors
  would smear the clean digital character); load paths are transparent too.
- The bank is exportable to `patches/gm/*.arachnopatch` via `--write-gm-presets`.

## Snapshot/State Flow

- GUI requests session snapshot via action/session bridge.
- `ApplicationSession::snapshot(...)` returns:
  - editor view-model
  - playback state (+ synth telemetry)
  - audio runtime health
  - message/task/diagnostic state

## Playback Feedback (GUI)

- The full snapshot refreshes at ~4 Hz during playback (heavy: rebuilds the
  editor view-model). Real-time playhead feedback runs on a cheap poll
  (`pollPlayheadFromWindowState`, `src/ui/gui/GuiSessionWindowOps.cpp`) at ~30 Hz
  from the main-loop tick: it fetches only the transport snapshot, patches it
  into the cached session snapshot, and applies follow-playback (pattern switch
  via `editor.navigation.pattern`, order-index sync, row follow via
  `ensureVisible`). It returns Unchanged/Redraw/FullRefresh so the tick only
  repaints (or fully refreshes) when something actually moved.
- The grid playhead is a full-row highlight plus a fractional scan line derived
  from `playback.position.absoluteRow` (`GuiMainGridDrawOps.cpp`); the playing
  order slot is highlighted in the top panel, and the status line shows
  `Ord n/N Pat p Row r` (1-based).
- The FOLLOW sidebar toggle flips a GUI-local flag mirrored into
  `AppSettings.layout.followPlayback` and persisted to the settings file by the
  tick watcher in `GuiWindow.cpp`.

## Contracts Worth Protecting

- `.arachno` and `.arachnopatch` format compatibility.
- CLI command/flag behavior from `src/main.cpp`.
- Deterministic sequencing semantics (row timing, order traversal, retrig/probability behavior).
- Stable shortcut/action identifiers used by GUI and automation.

