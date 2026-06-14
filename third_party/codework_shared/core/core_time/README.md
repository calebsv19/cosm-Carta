# core_time

Canonical monotonic time contract for the shared execution layer.

## Scope (v1.0.1)
- Monotonic timestamp read (`core_time_now_ns`)
- Duration helpers (diff/add/compare)
- Saturating unit conversion helpers
- Injectable provider for deterministic tests
- Trace timestamp interop helpers (`core_time_to_trace_ns`, `core_time_from_trace_ns`)

## Dependencies
- None

## Platform
- macOS backend: `src/core_time_mac.c`
- POSIX backend: `src/core_time_posix.c`
- Windows backend stub: `src/core_time_win.c`

## Current Contract
- `core_time` owns monotonic timestamp measurement and simple duration arithmetic only.
- provider override is process-global mutable state:
  - useful for deterministic tests
  - callers must coordinate externally if they use it across concurrent contexts
- `core_time_now_ns()` returns `0` on provider or platform failure.
- `core_time_diff_ns()` returns an absolute difference only; callers must not infer ordering from it.
- `core_time_add_ns()` saturates at `UINT64_MAX`.
- `core_time_seconds_to_ns()` saturates at `UINT64_MAX` for oversized positive inputs and returns `0` for non-positive inputs.
- trace helpers are current v1 identity mappings.

## Boundaries
- no sleep or wake primitives
- no scheduler, job, worker, or kernel policy
- no simulation time scaling
- no wall-clock/calendar semantics
- no renderer pacing or profiling UI behavior

## Status
- baseline implementation finalized for execution-core usage with provider/platform/trace surfaces in place
