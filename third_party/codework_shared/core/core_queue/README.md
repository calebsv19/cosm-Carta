# core_queue

Queue primitives for deterministic data passing across runtime layers.

## Scope
- Bounded ring queue (SPSC-style usage)
- Mutex + cond queue for MPSC/consumer runtime paths
- Overflow policy support (`REJECT`, `DROP_OLDEST`)
- Timed pop for wake-integrated consumer loops
- Queue operation statistics

## Dependencies
- None

## Status
- Baseline queue contract finalized with stress coverage (`v1.0.0`).
