# core_jobs Roadmap

## Mission
Provide deterministic, budgeted main-thread job execution to prevent starvation in runtime loops.

## Immediate Steps
1. Rebase internal storage on `core_queue` primitives with strict ownership rules.
2. Add queue overflow policies and rejection telemetry.
3. Define budget semantics (`0` unlimited vs strict zero-work) and lock contract.
4. Add instrumentation hooks for per-tick execution time and queue depth.

## Future Steps
1. Add priority bands only if justified by measured starvation cases.
2. Add replay-friendly deterministic execution mode.
3. Lock API guarantees for kernel and module integrations.
