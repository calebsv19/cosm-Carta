# core_sched Roadmap

## Mission
Provide a deterministic, non-blocking timer scheduler driven by `core_time` and orchestrated by `core_kernel`.

## Immediate Steps
1. Truth-lock synchronous callback, reentrancy, and ordering semantics against the live heap-backed implementation.
2. Expand invalid-argument, capacity, same-deadline ordering, `max_fires`, and cancellation edge coverage.
3. Keep timer-id wrap and repeating-deadline overflow handling explicit and bounded.

## Future Steps
1. Add deterministic serialization hooks for replay tests.
2. Add timer wheel or hybrid policy exploration only if profiling requires it.
3. Revisit callback isolation or richer diagnostics only if multiple hosts need them.
