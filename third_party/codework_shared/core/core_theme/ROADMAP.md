# core_theme Roadmap

## Mission
`core_theme` defines shared semantic UI token contracts so multiple applications can switch and reuse presets without renderer coupling.

## Current Surface
- Public API: `include/core_theme.h`
- Implementation: `src/core_theme.c`
- Tests: `tests/core_theme_test.c`

## Current Implementation Status
- Implemented now:
  - preset lookup by id and name
  - canonical preset names plus legacy alias parsing
  - color token lookup
  - process-global selected preset state
  - environment-variable preset selection hook
- Not implemented yet:
  - override merge behavior
  - serialization helpers
  - session- or renderer-scoped selected state

## Capability Layers

### Layer 1: Token Contract (Current)
- Stable preset IDs and semantic token enums.
- Deterministic preset lookup by id and name.

### Layer 2: Runtime Selection (Current)
- Active preset selection API by id/name.
- Environment-variable override hook for runtime preset selection.
- Selected-preset state is currently process-global mutable state and should stay documented that way until a scoped alternative exists.

### Layer 3: Override Model
- Add additive override merge behavior (app, user, runtime).
- Add conflict-resolution and validation rules.
- Keep this clearly roadmap-only until source support exists.

### Layer 4: Serialization
- Add optional JSON encode/decode helpers for preset bundles.
- Keep serialization optional and separate from renderer code.
- Keep this clearly roadmap-only until source support exists.

### Layer 5: Adoption
- Integrate app adapters in DAW, IDE, LineDrawing, and MapForge.
- Maintain fallback path until visual parity is confirmed.

## Recent Updates
- `2.0.1`: added exhaustive preset/color-token and scale coverage, pinned selected-preset and env-override behavior, documented process-global selected-state expectations, and kept override merge plus serialization explicitly roadmap-only.
