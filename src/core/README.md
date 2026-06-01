# src/core

## Ownership

This directory owns tracker/domain semantics.

It is the source of truth for:

- Song/pattern/step data structures
- Musical timing abstractions
- Note and instrument domain behavior
- Step-level effect semantics that influence synthesis intent

## File Map

- `Tracker.cpp`
  - High-level song construction helpers and tracker-level utilities.

- `Pattern.cpp`, `PatternRow.cpp`, `PatternStep.cpp`
  - Pattern grid representation and step access.

- `Note.cpp`
  - MIDI<->frequency and note-name conversions.

- `Instrument.cpp`
  - Synth patch defaults/parameter interpretation and helper behavior.

- `StepEffects.cpp`
  - Per-step effect mapping into synthesis state.

- `Effect.cpp`
  - Effect primitives and shared effect structures.

## Change Rules

1. Treat this layer as deterministic business logic.
2. Keep UI and backend concerns out of core models.
3. Any project format or editor command behavior change must remain backward-compatible or be explicitly migrated.
4. Keep note/timing math explicit and testable.

## Typical Dependencies

- Upstream users: `src/ui`, `src/audio`, `src/utils`, Python I/O compatibility layers.
- Avoid depending on frontend or platform-specific code from here.

