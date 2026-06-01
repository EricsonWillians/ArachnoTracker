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

