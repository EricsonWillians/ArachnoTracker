# AGENTS.md

This file defines repository-specific working rules for coding agents (Codex or similar).

## Scope

These rules apply to the entire repository unless a deeper `AGENTS.md` overrides them.

## Primary Engineering Rules

1. Preserve behavior first.
   - Prefer incremental, scoped changes over broad rewrites.
   - Do not silently change file formats, CLI contracts, or keyboard shortcuts.

2. Respect module boundaries.
   - `src/core`: tracker/domain model (song, pattern, note, step semantics).
   - `src/audio`: synthesis, realtime playback, rendering, runtime backends.
   - `src/ui`: session/actions/view-model orchestration.
   - `src/ui/gui`: frontend rendering/event dispatch/interaction plumbing.
   - `src/utils`: I/O, diagnostics, import/export, integration helpers.

3. Keep realtime paths deterministic and safe.
   - Avoid allocations in tight realtime loops when possible.
   - Prefer fixed-order reductions for deterministic audio summing.
   - If parallelizing audio paths, preserve stable summation order.

4. Follow existing naming patterns.
   - GUI operational files follow `Gui*Ops.cpp` with paired headers in `include/ui/gui`.
   - Keep feature logic grouped by operation type (draw/event/input/context factory/lifecycle).

5. Documentation is part of the change.
   - Update relevant README/docs when introducing a new subsystem contract, mode, or command.

## Build and Verification Baseline

Run these after meaningful C++ changes:

```bash
cmake -S . -B build
cmake --build build -j8
timeout 20s ./build/arachno_smoke_tests; echo EXIT:$?
```

Notes:
- In this repository, `arachno_smoke_tests` can intentionally run until timeout under `timeout`; record the observed `EXIT` value used by the current baseline.
- For Python SDK changes:

```bash
PYTHONPATH=python python3 -m unittest discover -s python/tests
```

## CLI and UX Guardrails

- Keep `--help` output in sync with implemented flags and command behavior.
- Do not remove existing shortcuts/commands without an explicit migration path.
- New telemetry or tuning UI should remain readable in DOS and high-contrast themes.

## Performance Change Expectations

When changing synth/runtime performance behavior:

- Capture before/after evidence with a reproducible command, e.g.:

```bash
/usr/bin/time -f 'real=%e user=%U sys=%S cpu=%P maxrss=%M' \
  ./build/ArachnoTracker --render <input.arachno> /tmp/out.wav
```

- Prefer measurements and deterministic behavior claims over intuition.

## What to Avoid

- No destructive git operations (`reset --hard`, checkout overwrite) unless explicitly requested.
- No broad formatting-only churn mixed with logic changes.
- No speculative refactors in unrelated modules while fixing a targeted issue.

