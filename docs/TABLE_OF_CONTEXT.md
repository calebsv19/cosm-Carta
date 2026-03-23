# Carta Docs: Table of Context

Use this file as the primary navigation map for project context.

## Top-Level Public Docs
- `README.md`: GitHub-facing project overview and quick-start guidance.
- `MAPFORGE_SUMMARY.md`: high-level product goals and architecture snapshot.

## Active Docs (`docs/`)
- `docs/ADDING_OSM_REGIONS.md`: operator workflow for importing new OSM XML files and building region packs.
- `docs/KEYBINDS.md`: current control bindings grouped by use case.
- `docs/MFT_V1.md`: tile binary format notes.
- `docs/GRAPH_V1.md`: routing graph binary format notes.
- `docs/GRAPH_V2.md`: routing graph v2 format + compatibility policy.
- `docs/REGION_PACK_LAYOUT.md`: runtime region directory structure.
- `docs/OSM_ROAD_CLASSES.md`: OSM tag -> road class mapping.

## Private Planning Docs
- Private MapForge plan/runbook docs are stored outside this repo path at:
  - `../docs/private_program_docs/map_forge/`
- Migration snapshot from `2026-03-12`:
  - `../docs/private_program_docs/map_forge/migrated_private_docs_2026-03-12/`
- Completed historical phase plans moved out of `map_forge/docs/` are stored at:
  - `../docs/private_program_docs/map_forge/completed_plans/`

## Notes
- Keep `map_forge/docs/` public and operator-facing.
- Keep private planning, implementation checklists, and internal runbooks in `docs/private_program_docs/map_forge/`.
