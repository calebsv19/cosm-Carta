# core_wake

Cross-thread wake abstraction for kernel wait/signal behavior.

## Scope
- Condvar backend for headless/runtime default
- External backend hooks for adapter-driven integration
- Timeout and infinite-wait support
- Pending-signal counter semantics

## Dependencies
- None
- Shared SemVer policy: `shared/docs/VERSIONING.md`

## Status
- Baseline wake contract finalized with stress coverage (`v1.0.1`).
- `v1.0.1` includes a standards-safe infinite-timeout constant (`UINT32_MAX`) to eliminate pedantic enum-range warnings without API behavior changes.
