# core_kernel Roadmap

## Mission
Provide a deterministic, policy-driven runtime loop that orchestrates timers, jobs, workers, and wake behavior with no UI coupling.

## Immediate Steps
1. Implement full phase order:
   - drain external events
   - drain worker messages
   - fire due timers
   - run budgeted jobs
   - module update notifications
   - render-needed signal aggregation
   - idle/block policy execution
2. Add policy-driven idle modes (`BLOCK`, `SPIN`, `BACKOFF`) with tests.
3. Integrate `core_workers` completion queue and `core_wake` wait/signal path.
4. Add deterministic shutdown and lifecycle failure handling.

## Future Steps
1. Add pluggable adapter interfaces for app event and render integration.
2. Add trace hooks to integrate kernel phase timing with `core_trace`.
3. Add headless harness and long-run stability/perf tests.
4. Freeze lifecycle contracts once adoption begins in IDE/runtime apps.
