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

## Dependencies
- `core_time`
- `core_sched`
- `core_queue`
- `core_jobs`
- `core_workers`
- `core_wake`

## Status
- Baseline runtime-orchestration contract finalized with headless harness coverage (`v1.0.0`).
