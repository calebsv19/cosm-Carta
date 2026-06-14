# core_kernel Roadmap

## Mission
Provide a deterministic, policy-driven runtime loop that orchestrates timers, jobs, workers, and wake behavior with no UI coupling.

## Immediate Steps
1. Keep renderer and platform event adapters outside `core_kernel`; only the borrowed queue/module orchestration boundary belongs here.
2. Revisit whether currently stored policy fields (`frame_cap_hz`, `worker_thread_count`, `coalesce_input_events`) should gain behavior or remain higher-layer configuration inputs.
3. Decide later whether richer shutdown/result reporting belongs here without taking ownership of adjacent queue/scheduler/worker lifecycles.
4. Add trace integration only as a separate additive seam after the base lifecycle contract stays stable.

## Future Steps
1. Add pluggable adapter interfaces for app event and render integration if multiple hosts need the same abstraction.
2. Add trace hooks to integrate kernel phase timing with `core_trace`.
3. Add broader long-run stability/perf coverage if adoption exposes real scheduler-pressure regressions.
4. Revisit fixed scheduler `max_fires` only if hosts need policy control over that boundary.
