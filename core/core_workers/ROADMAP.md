# core_workers Roadmap

## Mission
Provide predictable, bounded worker execution and completion signaling without introducing hidden scheduling policy.

## Immediate Steps
1. Add explicit submit backpressure and queue-full telemetry.
2. Add worker lifecycle stats (submitted/running/completed/dropped).
3. Add deterministic shutdown modes (drain vs cancel outstanding).
4. Validate no direct UI/shared-state mutation assumptions in docs/tests.

## Future Steps
1. Add task tagging and lightweight tracing hooks.
2. Add platform backends (`*_mac.c`, `*_posix.c`, `*_win.c`) while preserving API.
3. Keep advanced scheduling features out unless proven necessary.
