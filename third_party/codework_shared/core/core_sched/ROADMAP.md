# core_sched Roadmap

## Mission
Provide a deterministic, non-blocking timer scheduler driven by `core_time` and orchestrated by `core_kernel`.

## Immediate Steps
1. Replace bootstrap linear scan with min-heap (`O(log n)` insert/remove).
2. Add stable timer-ID reuse and cancellation safety tests.
3. Add repeating-timer drift policy and documentation.
4. Add callback error/isolation strategy for kernel integration.

## Future Steps
1. Add deterministic serialization hooks for replay tests.
2. Add timer wheel or hybrid policy exploration only if profiling requires it.
3. Stabilize scheduler behavior contract for long-lived runtime loops.
