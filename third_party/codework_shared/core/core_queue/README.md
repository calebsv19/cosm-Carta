# core_queue

Queue primitives for deterministic data passing across runtime layers.

## Scope (v1.0.1)
- Bounded ring queue (SPSC-style usage)
- Mutex + cond queue for MPSC/consumer runtime paths
- Overflow policy support (`REJECT`, `DROP_OLDEST`)
- Timed pop for wake-integrated consumer loops
- Queue operation statistics

## Dependencies
- None

## Current Contract
- queues store borrowed `void *` pointers only
- queue code does not allocate, free, copy, or own payload lifetime
- ring queue is the current primitive for single-thread or externally coordinated SPSC-style use
- mutex queue layers a `pthread_mutex_t` + `pthread_cond_t` around the same caller-backed ring storage for cross-thread producer/consumer paths
- supported overflow policies are exactly:
  - `CORE_QUEUE_OVERFLOW_REJECT`
  - `CORE_QUEUE_OVERFLOW_DROP_OLDEST`
- stats track queue-local outcomes only:
  - `push_ok`
  - `push_fail`
  - `pop_ok`
  - `pop_empty`
  - `drops`
- timed pop is a queue helper only; it is not a scheduler, wake policy, or kernel orchestration surface

## Boundaries
- no payload ownership hooks
- no priority or lock-free queue variants
- no scheduler, wake, or kernel policy
- no app backpressure or retry policy
- no custom waiter registration or cross-process transport semantics

## Status
- baseline queue contract finalized with edge coverage and stress coverage
