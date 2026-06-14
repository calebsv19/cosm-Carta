# core_workers Roadmap

## Mission
Provide predictable, bounded worker execution and completion signaling without introducing hidden scheduling policy.

## Immediate Steps
1. Truth-lock background execution, borrowed payload lifetime, optional completion semantics, and shutdown behavior against the live pthread implementation.
2. Expand invalid-init, submit rejection, completion-queue, and shutdown edge coverage.
3. Keep post-shutdown safety and cancel accounting explicit and bounded.

## Future Steps
1. Add task tagging and lightweight tracing hooks.
2. Add platform backends (`*_mac.c`, `*_posix.c`, `*_win.c`) while preserving API.
3. Keep advanced scheduling features out unless proven necessary.
