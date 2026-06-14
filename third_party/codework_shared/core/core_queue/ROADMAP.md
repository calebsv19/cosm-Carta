# core_queue Roadmap

## Mission
Provide predictable queue primitives for both single-thread and cross-thread execution paths.

## Immediate Steps
1. Truth-lock ring versus mutex queue usage guidance and borrowed-pointer lifetime rules.
2. Expand null/invalid-argument, wraparound, overflow-stat, and timeout edge coverage.
3. Keep overflow-policy validation explicit and bounded.

## Future Steps
1. Add benchmark harness and throughput/latency baselines.
2. Evaluate lock-free variants only after measurable need.
3. Revisit richer overflow/backpressure hooks only if multiple hosts need more than reject/drop-oldest behavior.
