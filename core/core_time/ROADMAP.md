# core_time Roadmap

## Mission
Provide a stable, deterministic monotonic time foundation used by all higher execution cores.

## Immediate Steps
1. Truth-lock provider failure sentinel and absolute-difference semantics in docs/tests.
2. Keep platform overflow handling aligned across macOS, POSIX, and Windows backends.
3. Expand helper edge coverage for reset behavior, compare ordering, and conversion boundaries.

## Future Steps
1. Add optional simulation time scaling hooks.
2. Add conformance tests across macOS/Linux/Windows monotonic behavior.
3. Stabilize API as a high-stability core dependency.
