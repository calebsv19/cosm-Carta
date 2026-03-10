# core_workers

Fixed-size worker pool abstraction for background task execution.

## Scope
- Initialize worker pool with fixed thread count
- Submit background tasks
- Push completion messages into shared queue boundary
- Deterministic shutdown modes (`DRAIN`, `CANCEL`)
- Worker lifecycle statistics

## Dependencies
- `core_queue`

## Status
- Baseline worker-pool contract finalized with soak coverage (`v1.0.0`).
