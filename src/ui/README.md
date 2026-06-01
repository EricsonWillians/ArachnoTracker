# src/ui

## Ownership

This directory owns application orchestration between domain, audio, and frontends.

## Key Components

- `ApplicationSession.cpp`
  - Central state coordinator.
  - Project lifecycle, playback binding, diagnostics, snapshot assembly.

- `AppActions.cpp`
  - Structured action API for GUI/shell automation and tooling.

- `EditorActions.cpp`, `EditorShortcuts.cpp`, `EditorCommandPalette.cpp`
  - Editor command/shortcut contracts and command palette metadata.

- `EditorViewModel.cpp`
  - Snapshot/view model translation for frontends.

- `AppSettings.cpp`
  - Persistent settings loading/validation/serialization.

- `AppTask.cpp`, `AppEvent.cpp`, `AppAsync.cpp`
  - Task status, event log, and background operation support.

- `ProjectLifecycle.cpp`
  - Save/load/sync/recovery workflow helpers.

## Architecture Rules

1. `ApplicationSession` is the composition root for runtime state.
2. GUI should interact through action/session contracts, not bypass state layers.
3. Snapshot structure changes must be intentional and documented.
4. Keep CLI and GUI command semantics aligned with action behavior.

## Snapshot Contract

`ApplicationSession::snapshot(...)` is the principal frontend state boundary:

- Editor state
- Playback snapshot (including synth telemetry)
- Audio runtime health
- Diagnostics/messages/tasks

When adding new UI features, prefer extending this contract over introducing ad-hoc globals.

