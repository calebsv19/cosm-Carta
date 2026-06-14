# core_sched

Deadline-driven scheduler primitives for one-shot and repeating timers.

## Scope (v1.0.1)
- Register one-shot or repeating timers
- Cancel timers by ID
- Query next deadline
- Fire due timers without blocking

## Implementation
- Heap-backed timer storage (`O(log n)` insert/remove)
- Deterministic tie-break ordering by `(deadline, id)`
- Repeating timers reschedule by stepping forward until strictly after `now`

## Dependencies
- `core_time`

## Current Contract
- callbacks run synchronously inside `core_sched_fire_due(...)`
- callback context lifetime is caller-owned
- same-deadline ordering is deterministic by `(deadline, id)`
- `core_sched_next_deadline_ns(...)` clamps overdue deadlines up to `now_ns`
- `max_fires` limits one `fire_due` pass without changing timer ordering rules
- callback reentrancy is currently permitted:
  - callbacks may add or cancel timers through the same scheduler
- repeating timers that cannot advance strictly past `now_ns` because of `uint64_t` overflow are dropped instead of looping indefinitely
- timer id `0` remains the failure sentinel and is never issued as a valid timer id

## Boundaries
- no sleep, wake, worker, job, or kernel orchestration policy
- no callback isolation or error-report surface
- no persistence or serialization hooks
- no app timer labels, ownership, or semantic state

## Status
- baseline scheduler contract hardened with edge coverage and fuzz coverage
