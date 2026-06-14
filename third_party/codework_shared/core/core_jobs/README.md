# core_jobs

Main-thread job queue with deterministic budgeting behavior.

## Scope (v1.0.1)
- Enqueue function-pointer jobs
- Run by wall-time budget (`core_jobs_run_budget`)
- Run fixed-count replay step (`core_jobs_run_n`)
- Overflow policies (`REJECT`, `DROP_OLDEST`)
- Queue execution statistics

## Dependencies
- `core_time`

## Current Contract
- jobs run synchronously on the caller thread
- job function pointers and `user_ctx` values are borrowed caller-owned data
- `core_jobs_run_budget(..., 0)` means unlimited budget for that pass
- non-zero budget checks are pre-execution soft limits:
  - the queue stops before starting the next job once elapsed time is strictly greater than the configured budget
- overflow policies are exactly:
  - `CORE_JOBS_OVERFLOW_REJECT`
  - `CORE_JOBS_OVERFLOW_DROP_OLDEST`
- stats are queue-local counters only:
  - `enqueued`
  - `executed`
  - `dropped`
  - `budget_stops`
  - `dropped_oldest`

## Boundaries
- no worker-thread ownership or execution
- no wake, scheduler, or kernel phase policy
- no retry, priority, or backpressure semantics
- no payload ownership, destructors, or cancellation of already-queued jobs
- no instrumentation hooks beyond the current stats struct

## Status
- baseline job-queue contract hardened for kernel usage with expanded edge coverage
