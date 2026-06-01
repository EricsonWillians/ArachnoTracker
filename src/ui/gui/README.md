# src/ui/gui

## Purpose

This directory contains GUI frontend behavior (X11 window + shell fallback), organized as operational modules.

## Naming Map

Most files follow:

- `Gui<Area><Operation>Ops.cpp`

Common operation suffixes:

- `DrawOps`: drawing/layout/text rendering helpers
- `EventOps`: event handling logic (key/mouse/window)
- `InputOps`: input preprocessing and gesture routing
- `MotionOps` / `WheelOps`: pointer movement and wheel behavior
- `ClickOps`: click dispatch and button interactions
- `ContextFactoryOps`: construct typed context bundles for operations
- `InteractionAdapterOps`: bridge between composed contexts and handlers
- `LifecycleOps`: window/resource init/shutdown flows
- `BindingsOps`: wrap mutable `GuiWindow` state as callable interfaces
- `RenderAdapterOps` / `RunLoopAdapterOps`: high-level orchestration adapters

## Primary Coordination Files

- `GuiWindow.cpp`
  - Main composition entry for the GUI runtime.
  - Owns frontend state objects and wires operation modules together.

- `GuiMainWindowRenderOps.cpp`
  - Main frame render orchestration.

- `GuiMainRunLoopOps.cpp`
  - Tick loop pacing, event polling, and runtime integration.

## Major Functional Areas

- Main tracker window: `GuiMain*`
- Synth/patch designer window: `GuiSynth*`
- Arrangement/order/pattern interactions: `GuiArrangement*`, `GuiGrid*`
- File/prompt/session overlays: `GuiFile*`, `GuiInlinePrompt*`, `GuiSession*`, `GuiUnsavedPrompt*`
- Audio output bridge/tuning: `GuiAudioRuntime.cpp`, `GuiMainRealtimeAudioOps.cpp`

## Practical Rule of Thumb

If a new feature touches multiple event/draw/input pathways:

1. Add or extend typed context structures in headers under `include/ui/gui`.
2. Keep behavior in operation modules (not in `GuiWindow.cpp` directly).
3. Wire the modules through adapter/factory functions.

This keeps `GuiWindow.cpp` as composition glue rather than a behavior monolith.

