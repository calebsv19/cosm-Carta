# core_config Roadmap

## Mission
Provide lightweight typed config state that app shells can use without pulling full schema systems.

## Immediate Steps
1. Stabilize fixed-capacity typed entry table API.
2. Keep deterministic update semantics and explicit error codes.
3. Validate behavior under capacity pressure and key replacement.

## Future Steps
1. Add optional key namespace segments (`group.key`).
2. Add lightweight diff/merge helpers for authoring sessions.
3. Add optional adapter helpers for `core_pack` serialization.
