# core_workers

Fixed-size worker pool abstraction for background task execution.

## Scope (v1.0.1)
- Initialize worker pool with fixed thread count
- Submit background tasks
- Push completion messages into shared queue boundary
- Deterministic shutdown modes (`DRAIN`, `CANCEL`)
- Worker lifecycle statistics

## Dependencies
- `core_queue`

## Current Contract
- task functions run on background worker threads
- task function pointers, task contexts, and optional completion payloads are borrowed caller-owned data
- completion signaling is optional:
  - if `completion_queue == NULL`, tasks still execute and complete
  - if a task returns `NULL`, no completion message is pushed
  - if the completion queue rejects a completion payload, execution still completes and the payload is not retried by `core_workers`
- shutdown modes are exactly:
  - `CORE_WORKERS_SHUTDOWN_DRAIN`
  - `CORE_WORKERS_SHUTDOWN_CANCEL`
- `DRAIN` lets already-queued tasks finish before worker exit
- `CANCEL` clears pending queued tasks before they start and increments `stats.canceled`
- stats are worker-pool-local counters only:
  - `submitted`
  - `completed`
  - `rejected`
  - `canceled`

## Boundaries
- no scheduling policy, wake semantics, or kernel phase ordering
- no task retries, priorities, or work stealing
- no payload ownership, destructors, or completion retry lane
- no UI/shared-state safety beyond caller-owned task discipline
- no tracing or instrumentation beyond the current stats struct

## Status
- baseline worker-pool contract hardened with expanded edge and soak coverage
