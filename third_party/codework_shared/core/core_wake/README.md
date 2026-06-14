# core_wake

Cross-thread wake abstraction for kernel wait/signal behavior.

## Scope
- Condvar backend for headless/runtime default
- External backend hooks for adapter-driven integration
- Timeout and infinite-wait support
- Pending-signal counter semantics for queued pre-signals

## Contract
- `core_wake` owns wait/signal bridging only. It does not own scheduler policy, worker/job execution, app shutdown sequencing, SDL/event-framework semantics, or kernel idle strategy.
- `core_wake_init_cond(...)` provides the default pthread mutex/condvar backend for headless and runtime use.
- `core_wake_init_external(...)` installs host-owned signal/wait callbacks for adapter-driven environments.
- Cond-backend waits consume exactly one pending signal per successful wake. Pre-signals accumulate until consumed, and overflow is rejected instead of wrapping.
- Finite cond-backend waits use `pthread_cond_timedwait(...)` with `CLOCK_REALTIME`, so timeout behavior follows the platform realtime clock rather than a monotonic scheduler surface.
- `core_wake_shutdown(...)` transitions the object back to an inert uninitialized state. Callers must not race shutdown against active waiters or reuse the object without a fresh init.

## Dependencies
- None
- Shared SemVer policy: `shared/docs/VERSIONING.md`

## Status
- Wake boundary truth-locked with lifecycle and pending-signal hardening (`v1.0.2`).
- `v1.0.2` adds invalid-arg, post-shutdown, external-failure, and repeated pre-signal coverage, and rejects cond-backend pending-signal overflow instead of wrapping.
