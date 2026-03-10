# core_sched

Deadline-driven scheduler primitives for one-shot and repeating timers.

## Scope
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

## Status
- Baseline scheduler contract finalized with fuzz coverage (`v1.0.0`).
