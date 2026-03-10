# core_time

Canonical monotonic time contract for the shared execution layer.

## Scope
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

## Status
- Baseline implementation finalized for execution-core usage (`v1.0.0`).
