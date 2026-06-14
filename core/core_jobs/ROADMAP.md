# core_jobs Roadmap

## Mission
Provide deterministic, budgeted main-thread job execution to prevent starvation in runtime loops.

## Immediate Steps
1. Truth-lock synchronous execution, borrowed job payload lifetime, and budget semantics against the live ring-buffer implementation.
2. Expand invalid-argument, wraparound, overflow-stat, and budget edge coverage.
3. Keep overflow-policy validation and budget conversion overflow handling explicit and bounded.

## Future Steps
1. Add priority bands only if justified by measured starvation cases.
2. Add replay-friendly deterministic execution mode.
3. Revisit richer instrumentation hooks only if multiple hosts need more than the current stats surface.
