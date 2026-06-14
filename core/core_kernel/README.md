# core_kernel

Execution runtime orchestrator for shared scheduling/concurrency primitives.

## Scope
- Policy object for idle/budget/thread settings
- Module lifecycle registration hooks
- Ordered tick phases:
  - event drain
  - worker message drain
  - due timer fire
  - budgeted jobs
  - update + render hint
  - idle policy
- Timer notification bridge (`core_kernel_notify_timer`)

## Contract
- `core_kernel` owns shared runtime loop ordering and module lifecycle policy only. It does not own renderer/UI behavior, platform event adapters, worker payload cleanup, app shutdown sequencing, trace capture, or simulation cadence.
- Dependency pointers passed at init are borrowed host-owned services. `core_kernel` does not create, destroy, drain, or otherwise take ownership of the scheduler, jobs queue, wake object, or optional event/worker queues.
- Module hooks run synchronously on the caller thread during registration, tick, timer notification, and shutdown.
- `core_kernel_tick(...)` runs this fixed phase order: event queue drain, worker message drain, due timer fire, budgeted jobs, module updates, render-hint aggregation, then idle policy.
- `last_tick_work_units` counts drained events, drained worker messages, fired timers, and executed jobs only. Module updates and render-hint checks are not included in that count.
- Due timers currently fire with a fixed `max_fires = 1024` per tick; that is a current implementation contract, not a policy field.
- `frame_cap_hz`, `worker_thread_count`, and `coalesce_input_events` are currently stored policy fields only. They are not active behavior knobs yet.
- Invalid idle-mode values normalize to `CORE_KERNEL_IDLE_BLOCK`.
- Shutdown is best-effort module hook notification only. It does not drain queues, cancel timers/jobs, stop workers, or signal wake objects.

## Dependencies
- `core_time`
- `core_sched`
- `core_queue`
- `core_jobs`
- `core_wake`

## Status
- Runtime-orchestration boundary truth-locked with lifecycle and idle-policy hardening (`v1.0.1`).
