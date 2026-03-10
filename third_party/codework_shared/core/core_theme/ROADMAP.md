# core_theme Roadmap

## Mission
`core_theme` defines shared semantic UI token contracts so multiple applications can switch and reuse presets without renderer coupling.

## Current Surface
- Public API: `include/core_theme.h`
- Implementation: `src/core_theme.c`
- Tests: `tests/core_theme_test.c`

## Capability Layers

### Layer 1: Token Contract (Current)
- Stable preset IDs and semantic token enums.
- Deterministic preset lookup by id and name.

### Layer 2: Runtime Selection (Current)
- Active preset selection API by id/name.
- Environment-variable override hook for runtime preset selection.

### Layer 3: Override Model
- Add additive override merge behavior (app, user, runtime).
- Add conflict-resolution and validation rules.

### Layer 4: Serialization
- Add optional JSON encode/decode helpers for preset bundles.
- Keep serialization optional and separate from renderer code.

### Layer 5: Adoption
- Integrate app adapters in DAW, IDE, LineDrawing, and MapForge.
- Maintain fallback path until visual parity is confirmed.
