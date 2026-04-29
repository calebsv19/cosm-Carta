# core_pack

Versioned, chunk-based binary container (`.pack`) for offline interchange.

## Scope (Phase 1 continuation)
- File header with format version
- Chunk append API
- Optional encoded chunk write API (`none`, `rle8`)
- Sequential chunk read API
- Random access via embedded chunk index
- Partial reads via chunk slice API
- Decoded read API for encoded chunks
- Explicit little-endian on-disk encoding for header/index metadata
- Minimal CLI for round-trip and inspection
- PhysicsSim raw-frame to `.pack` conversion helpers for legacy `vf2d` and truthful `vf3d`

## Dependencies
- `core_base`
- `core_io`

## Notes
- Existing `vf2d` flow is preserved unchanged.
- Additive `vf3d`/`VF3H` conversion is now available for truthful volumetric PhysicsSim export.
- Core Pack is additive until migration is explicitly approved.
- `.pack` v1 freeze contract is documented in `PACK_V1_SPEC.md`.
- Shared SemVer policy is documented in `../VERSIONING.md`.

## CLI
`tools/pack_cli.c` commands:
- `write <pack_path>`
- `inspect <pack_path>`
- `roundtrip <pack_path>`
- `vf2d_to_pack <vf2d_path> <pack_path> [manifest_json]`

## Version
- Module version source: `VERSION`
- Current module version: `1.1.0`
- Current target format: `PACK_FORMAT_VERSION_MAJOR=1`, `PACK_FORMAT_VERSION_MINOR=0`
