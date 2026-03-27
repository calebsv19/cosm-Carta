# MapForge Docs: Table of Context

Use this file as the primary navigation map for project context.

## Top-Level Public Docs
- `README.md`: GitHub-facing project overview and quick-start guidance.

## Active Docs (`docs/`)
- `docs/ADDING_OSM_REGIONS.md`: operator workflow for importing new OSM XML files and building region packs.
- `docs/KEYBINDS.md`: current control bindings grouped by use case.
- `docs/MFT_V1.md`: tile binary format notes.
- `docs/GRAPH_V1.md`: routing graph binary format notes.
- `docs/GRAPH_V2.md`: routing graph v2 format + compatibility policy.
- `docs/REGION_PACK_LAYOUT.md`: runtime region directory structure.
- `docs/OSM_ROAD_CLASSES.md`: OSM tag -> road class mapping.
- `docs/ARCHITECTURE_RUNTIME.md`: runtime phases, ownership boundaries, and dataflow.
- `docs/ARCHITECTURE_THREADS_AND_QUEUES.md`: thread topology and queue semantics.
- `docs/CONTRIBUTING_MODULE_MAP.md`: source module map and contributor edit guide.
- `docs/KNOWN_CONSTRAINTS_AND_BACKLOG.md`: known constraints plus follow-up items.
- `docs/current_truth.md`: current scaffold/runtime status and verification contract.
- `docs/future_intent.md`: scaffold convergence intent and pending migration slices.
- `make test-presentation-stability`: focused non-GUI presenter stability regression gate.
- `make run-headless-smoke`: non-interactive smoke gate for scaffold migration slices.

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
- Runtime persistence policy:
  - tracked defaults in `config/app.config.json`
  - mutable runtime state in ignored `data/runtime/app_state.json`
- Dependency/include policy:
  - keep `third_party/` for vendored subtree integration
  - keep existing domain include lanes; place new app-level public entry APIs under `include/map_forge/`
