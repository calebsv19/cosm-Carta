# core_queue Roadmap

## Mission
Provide predictable queue primitives for both single-thread and cross-thread execution paths.

## Immediate Steps
1. Add SPSC contract and tests for wraparound/overflow behavior.
2. Add MPSC queue API and contention tests.
3. Document queue ownership and memory-lifetime rules.
4. Add optional blocking pop behavior only where explicitly required.

## Future Steps
1. Add bounded overflow policies (reject/drop-oldest/custom).
2. Add benchmark harness and throughput/latency baselines.
3. Evaluate lock-free variants only after measurable need.
