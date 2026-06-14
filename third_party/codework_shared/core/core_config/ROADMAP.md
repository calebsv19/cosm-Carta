# core_config Roadmap

## Mission
Provide lightweight typed config state that app shells can use without pulling full schema systems.

## Immediate Steps
1. Stabilize fixed-capacity typed entry table API.
2. Keep deterministic update semantics while the API remains bool-only.
3. Validate behavior under capacity pressure, key replacement, max-length boundaries, and non-finite double rejection.

## Hardened Current State
- Fixed-capacity copied key/value table is stable for bool/int/double/string storage.
- Existing-key updates are deterministic even when the table is full.
- Failed get paths now clear output deterministically before returning `false`.
- Non-finite doubles are rejected at the shared boundary instead of relying on host wrappers.
- Error classification, namespace semantics, iteration, persistence, and merge/diff remain outside the current surface.

## Future Steps
1. Add optional key namespace segments (`group.key`).
2. Add lightweight diff/merge helpers for authoring sessions.
3. Add optional adapter helpers for `core_pack` serialization.
