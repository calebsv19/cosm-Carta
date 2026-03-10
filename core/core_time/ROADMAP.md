# core_time Roadmap

## Mission
Provide a stable, deterministic monotonic time foundation used by all higher execution cores.

## Immediate Steps
1. Add provider injection for deterministic tests.
2. Split platform internals into `*_mac.c`, `*_posix.c`, `*_win.c`.
3. Add trace timestamp conversion helpers aligned with `core_trace` contracts.
4. Define explicit overflow/saturation behavior for arithmetic helpers.

## Future Steps
1. Add optional simulation time scaling hooks.
2. Add conformance tests across macOS/Linux/Windows monotonic behavior.
3. Stabilize API as a high-stability core dependency.
