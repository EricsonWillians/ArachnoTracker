# Lessons Learned

This document stores recurring corrections only.

## 1) Keep realtime loops allocation-free and math-aware

Recurring correction:
- Avoid adding allocations or expensive transcendental work inside sample loops unless absolutely required.

Applied repeatedly:
- Precompute control values per block where possible.
- Use deterministic/fixed-order summing and bounded complexity scaling under heavy polyphony.

## 2) Determinism matters for parallel audio paths

Recurring correction:
- Parallel rendering must preserve deterministic output ordering.

Applied repeatedly:
- Use fixed bus partitioning and fixed-order merge.
- Do not let thread completion order define final summation order.

## 3) GUI complexity must be modularized early

Recurring correction:
- Large monolithic GUI files degrade velocity and correctness.

Applied repeatedly:
- Split behavior by operation category (`Draw`, `Event`, `ContextFactory`, `Lifecycle`, `Adapter`, `Bindings`).
- Keep file naming explicit and discoverable.

## 4) Telemetry must be actionable, not decorative

Recurring correction:
- Performance indicators that do not map to decisions are noise.

Applied repeatedly:
- Surface explicit DSP load %, underrun risk, and limiter/headroom metrics.
- Prefer simple meters and stable labels over overloaded text.

## 5) Preserve user workflows while improving internals

Recurring correction:
- Do not break existing CLI/shortcut/project contracts while optimizing internals.

Applied repeatedly:
- Keep old flags/actions working.
- Update documentation alongside behavior changes.


## 6) Count-based "safety" limits are audible bugs, not safety

Recurring correction:
- Hard caps keyed to raw track/voice/event counts (voice cull caps, unison
  budgets like `24 / activeVoiceCount`, per-frame event quotas, same-patch-name
  voice limits, spatial-FX cutoffs at N voices) silently delete or thin out
  audio exactly when arrangements get dense — users describe it as "tracks
  nullify each other" or "tracks stop playing when others start". They fire on
  healthy machines where no protection is needed.

Applied repeatedly:
- All adaptive degradation must be gated on measured signals (wall-clock DSP
  load, underrun risk) only; idle/healthy systems run full quality.
- Degradation order: timbral detail (unison/FX) → low-priority event thinning →
  envelope tightening → gate clamping (emergencies only). Notes are never
  dropped at the first pressure stage.
- Culling/stealing scopes must respect ownership: same-midi and mono-mode
  stealing per (instrument, channel); patch-name caps per instrument and only
  under load — imported MIDI lanes legitimately share patch names across tracks.
- Polyphony normalization (`voiceNorm`) must stay gentle; the limiter/auto-trim
  already react to real loudness, and offline bus renders (few voices per bus)
  otherwise sound much louder than single-bus realtime playback of the same mix.

## 7) Per-voice heavy FX does not scale — use shared buses in realtime

Recurring correction:
- Running a full effect network (e.g. a Freeverb reverb: 8 combs + 4 allpasses
  x 2 channels) inside every voice costs ~50% of the whole DSP budget at
  moderate polyphony and makes dense realtime playback underrun no matter how
  large the audio buffer is. Buffer size only absorbs jitter; it cannot fix
  sustained over-budget rendering.

Applied repeatedly:
- Measure per-feature cost before optimizing (A/B load probes with individual
  FX stages disabled) — the bottleneck was reverb alone; chorus/delay/unison
  were negligible.
- Realtime uses one shared send bus with send-weighted, block-smoothed
  parameters; offline rendering keeps per-voice networks for full-quality
  per-patch tails (rendered files bit-identical, verified with `cmp`).
- Voice lifetime must not include tails the voice no longer owns (realtime
  voices die after their delay tail; the shared reverb rings on its own).
- Idle-skip shared FX processing when both input and tail are silent.

## 8) Audio rendering must not share a thread with the UI — and never memmove big voices

Recurring correction:
- Rendering audio synchronously on the GUI thread hard-stutters on dense songs:
  redraws, event storms, and snapshot rebuilds preempt block rendering, the
  output queue drains, and conceal/re-prime cycles produce audible gaps. Buffer
  geometry (performance modes) cannot fix producer starvation.

Applied:
- A dedicated producer thread (`GuiAudioProducer`) renders blocks into the
  thread-safe output queue; the GUI keeps only lifecycle (open/close) work.
- Synchronization is ONE shared recursive mutex
  (`RealtimePlaybackSession::apiMutex` / `ApplicationSession::audioStateMutex()`):
  producer block renders vs. transport/audition/editor/project mutations. No
  nesting with third-party locks; never hold across waits.
- Related realtime-path fix: `Synthesizer` voice stealing used `voices_.erase()`,
  memmoving trailing ~300 KB `Voice` objects (embedded FX buffers) per steal —
  tens of MB per dense note burst. Steals now swap-and-pop (voice order was
  already unstable by design: the post-render cull used swap-and-pop).
- Lock discipline for the shared audio-state mutex: it exists to serialize
  block renders against SHORT mutations (transport, audition, editor commands).
  Never hold it across view-model builds, redraws, file I/O, or sleeps — a
  full `snapshot()` under the lock (tens of ms every 240 ms) stalled the
  producer exactly like the old GUI-thread rendering did. Playback/transport
  state has its own self-locking snapshot path; read-vs-read of the song needs
  no guard. Load-feedback smoothing is asymmetric (fast attack, slow release)
  so shedding engages within ~2 blocks without tier thrash.

## 9) GUI poll paths must be lock-free, not just "not long-held"

Recurring correction:
- Even with a producer thread and a well-disciplined short-hold audio mutex,
  any GUI *poll* path that takes that mutex still stutters the interface: the
  producer holds `apiMutex_` for an entire block render (tens of ms on dense
  blocks), so a 33 ms playhead poll, an action refresh, or a MIDI-poll
  `snapshot()` queues behind it and the UI/audition feels stuck in bursts —
  indistinguishable from audio underruns in user reports. Pipeline sims can be
  perfectly healthy (queue full, zero xruns) while this is happening, because
  the defect is interface latency, not audio starvation.

Applied:
- `RealtimePlaybackSession::snapshot()` is fully lock-free: transport state,
  playhead row, and follow-cursor are atomics; the loop range uses a tiny side
  mutex (never nested with `apiMutex_`); the row map + `secondsPerRow` are a
  published immutable `shared_ptr` lookup swapped in `rebuildRowMap`; synth
  telemetry is a display-only copy (a momentarily torn field is cosmetically
  harmless).
- Rule of thumb: anything the GUI calls on a timer (playhead poll, telemetry,
  MIDI poll) must complete in microseconds without touching `apiMutex_`.
  Verify with a churn harness that hammers `playback().snapshot()` at 33 ms
  during a producer render — worst latency should stay ~0.01 ms.

## 10) Output-writer hysteresis is a stutter amplifier — and poll loops must be O(1)

Recurring correction:
- The ALSA writer thread used aggressive hysteresis: unprime when the queue
  dipped below startThreshold/3 (~28 ms in heavy mode), then stay silent until
  the queue refilled to the full start threshold (~85 ms). One deep producer
  block or one PipeWire hiccup flipped the loop into repeating 85 ms silence
  gaps — RANDOM in time (load/queue-phase dependent, not song-position
  dependent), which no offline render or healthy-queue sim reproduces.
- Concealment waited 8×12 ms before acting, but the device buffer is ~20 ms —
  it always arrived after the xrun it existed to prevent.
- Separately, `syncArmedInstrumentFromSession` ran a FULL `session.snapshot()`
  (O(song) countContent + full grid rebuild) every main-loop iteration while
  the loop hot-spun during streaming — pegging the GUI thread and starving
  keyboard/MIDI note dispatch.

Applied:
- Writer policy: once primed, stream ANY queued audio (>= 1 frame); unprime
  only on true emptiness; re-prime threshold capped at 2 chunks; conceal on the
  FIRST starved timeout (~12 ms < 20 ms device buffer, so the device never
  runs dry during producer hiccups).
- `tuneAlsaQueueForLoad` start-prime capped at 1024 frames (~21 ms) — the
  heavy-mode 85 ms start prime made every live note late.
- Producer per-block frames capped (2048 starved / 1024 normal) so one render
  call's audio-mutex hold stays bounded for note-on latency.
- `syncArmedInstrumentFromSession` reads cursor/active-step directly (O(1));
  main loop sleeps 4 ms during streaming when the producer thread owns audio
  instead of yield-spinning.
- Diagnostic: `ARACHNO_SPIKE_LOG=1 ./build/ArachnoTracker --gui` logs producer
  render spikes, GUI phase spikes (midi-poll/refresh/draw) with monotonic
  timestamps (include/SpikeLog.h). Random-stutter reports must come with a
  spike log and the s/c/x counters, not just a description.

## 11) Prove the output stream before blaming the engine — check the sink

Recurring correction:
- "Random stutter" reports survived every app-side fix because the defect was
  never in the app: the user's default sink was BLUETOOTH
  (PipeWire bluez_output), whose RF/encoder hiccups produce exactly that
  symptom, position-independent, invisible to app-side queue telemetry.

Applied (diagnostic method that settled it):
- Drive the real GUI under Xephyr (XTest key injection), `ARACHNO_SPIKE_LOG=1`.
- `pactl load-module module-null-sink` + set-default-sink, then
  `parecord -d <null>.monitor` during full-song playback: 98 s through the
  reported "bad" section showed ZERO gaps/repeats/spikes — the app stream is
  gapless, so the stutter is introduced downstream (BT leg).
- NOTE: `pw-record --target <bt>.monitor` silently records zeros on some
  PipeWire/BT nodes; use `parecord -d <source-name>` and sanity-check peak > 0.
- Mitigation implemented: the ALSA device buffer self-escalates 20→48→96 ms on
  a measured xrun streak (3 in a row) via the existing unhealthy→reopen path,
  giving PipeWire's bursty BT pulls a bigger ring to drain. Wired sinks never
  escalate (no xruns, no latency cost). `ARACHNO_ALSA_LATENCY_MS` pins a fixed
  value; changing performance mode resets to 20 ms; the top panel shows
  `AL<n>ms` when escalated.
