# src/audio

## Ownership

This directory owns audio synthesis, playback, rendering, and runtime health models.

## Files

- `Synthesizer.cpp`
  - Native synth voice engine.
  - Oscillators, modulation, filters, transient shaping, stereo processing.
  - Adaptive quality tiers, SIMD-aware hotspots, render telemetry.

- `RealtimePlayback.cpp`
  - Transport state machine (`play/pause/stop/seek/loop`).
  - Row/event scheduling for realtime playback.
  - Audition entrypoints and playback snapshot generation.

- `AudioEngine.cpp`
  - Offline rendering pipeline for mixdown and stems.
  - Deterministic bus partitioning and fixed-order merge.

- `AudioRuntime.cpp`
  - Backend/device catalog and validation model.
  - Runtime configuration and health reporting (configured/active/underruns).

- `Mixer.cpp`
  - Reserved for dedicated mixer extraction (currently minimal).

## Change Rules

1. Keep realtime render deterministic and low-jitter.
2. Keep heavy-load behavior graceful (quality scaling, not hard failure).
3. Preserve rendered output compatibility where practical.
4. Any new metrics should be exposed through snapshot-friendly telemetry structures.

## Verification

Use:

```bash
cmake --build build -j8
timeout 20s ./build/arachno_smoke_tests; echo EXIT:$?
/usr/bin/time -f 'real=%e user=%U sys=%S cpu=%P maxrss=%M' \
  ./build/ArachnoTracker --render <input.arachno> /tmp/out.wav
```

