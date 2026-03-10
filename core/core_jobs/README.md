# core_jobs

Main-thread job queue with deterministic budgeting behavior.

## Scope
- Enqueue function-pointer jobs
- Run by wall-time budget (`core_jobs_run_budget`)
- Run fixed-count replay step (`core_jobs_run_n`)
- Overflow policies (`REJECT`, `DROP_OLDEST`)
- Queue execution statistics

## Dependencies
- `core_time`

## Status
- Baseline job-queue contract finalized for kernel usage (`v1.0.0`).
