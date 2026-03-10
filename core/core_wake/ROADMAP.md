# core_wake Roadmap

## Mission
Decouple kernel wake behavior from UI/event frameworks so runtime orchestration remains portable and headless-safe.

## Immediate Steps
1. Add SDL adapter backend implementation outside core (`adapter layer`).
2. Add timeout semantics tests (spurious wake, repeated wake, shutdown wake).
3. Document backend selection/init rules for kernel boot.
4. Add wake statistics hooks for observability.

## Future Steps
1. Add explicit platform backend files (`*_mac.c`, `*_posix.c`, `*_win.c`) as needed.
2. Add wake coalescing policy hooks if event storms require it.
3. Lock stable wake contract for `core_kernel` policy modes.
