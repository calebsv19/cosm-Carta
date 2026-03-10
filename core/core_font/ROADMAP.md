# core_font Roadmap

## Mission
`core_font` provides shared font-role contracts and fallback rules so apps can resolve typography consistently without hardcoded per-app path logic.

## Current Surface
- Public API: `include/core_font.h`
- Implementation: `src/core_font.c`
- Tests: `tests/core_font_test.c`

## Capability Layers

### Layer 1: Role Contract (Current)
- Stable role IDs, preset IDs, and role lookup API.
- Primary and fallback path hints for each role.

### Layer 2: Manifest Contract (Current)
- Line-based manifest parse/validate API (`MANIFEST_FORMAT.md`).
- Source/license metadata per role mapping.

### Layer 3: Fallback Policy (Current)
- Deterministic fallback chain resolution helper.
- Explicit failure behavior when no candidate path resolves.

### Layer 4: Adoption
- Migrate DAW/IDE/LineDrawing/MapForge font loaders to role-based selection.
- Keep current app-local loaders as fallbacks during migration.

## Recent Updates
- `1.0.1`: synced mono role defaults to the currently vendored shared font asset set and aligned preset fallback paths with `shared/assets/fonts/*` so shared UI hosts can reliably render TTF text without per-app path assumptions.
