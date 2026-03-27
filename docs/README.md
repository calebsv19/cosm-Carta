# MapForge Docs

Start here for the public, operator-facing documentation that ships with the repository.

## Start Here

- `docs/ADDING_OSM_REGIONS.md`: import a new OSM file and build a local region pack.
- `docs/KEYBINDS.md`: runtime controls.
- `docs/REGION_PACK_LAYOUT.md`: generated region directory structure and metadata contract.

## Formats And Data Contracts

- `docs/MFT_V1.md`: render tile binary format.
- `docs/GRAPH_V1.md`: original routing graph format notes.
- `docs/GRAPH_V2.md`: current routing graph format and compatibility notes.
- `docs/OSM_ROAD_CLASSES.md`: OSM tag-to-road-class mapping used by the tooling.

## Context And Navigation

- `docs/TABLE_OF_CONTEXT.md`: public doc map plus private-doc location notes.
- `docs/current_truth.md`: current structural/runtime scaffold state and active verification gates.
- `docs/future_intent.md`: intended scaffold convergence path and near-term migration intent.

## Architecture And Contribution

- `docs/ARCHITECTURE_RUNTIME.md`: frame-phase runtime/dataflow architecture.
- `docs/ARCHITECTURE_THREADS_AND_QUEUES.md`: worker thread and queue topology/invariants.
- `docs/CONTRIBUTING_MODULE_MAP.md`: contributor module map and "where to edit" guide.
- `docs/KNOWN_CONSTRAINTS_AND_BACKLOG.md`: current constraints and follow-up backlog.

## Contributor Quickstart

If you are new to the runtime code, read in this order:

1. `src/app/app.c`: app bootstrap and top-level runtime orchestration.
2. `src/app/app_runtime_update.c`: update-stage state mutation flow.
3. `src/app/app_runtime_render.c`: render-stage draw flow.
4. `src/app/app_route_service.c`: route alternative selection/toggle boundary.
5. `src/app/app_tile_pipeline.c`: async tile/Vulkan queue and worker integration.

## Non-GUI Stability Checks

- `make test-presentation-stability`
- `make test`
- `make run-headless-smoke`

## Scaffold Migration References

- Private migration plan:
  - `../docs/private_program_docs/map_forge/2026-03-27_map_forge_scaffold_standardization_switchover_plan.md`
- Baseline freeze snapshot:
  - `../docs/private_program_docs/map_forge/2026-03-27_mf_s0_baseline_freeze_and_mapping.md`

## Runtime Config Persistence

- default (tracked): `config/app.config.json`
- runtime state (ignored): `data/runtime/app_state.json`

## Scaffold Policy Locks (MapForge)

- dependency lane: keep `third_party/` for vendored subtree mode (`third_party/codework_shared`)
- include strategy: retain existing domain include lanes; use `include/map_forge/` for new app-level public entry APIs

## Private Planning Docs

Private plans, execution checklists, and internal runbooks were moved out of this repo lane. They live in the CodeWork workspace at:

- `../docs/private_program_docs/map_forge/`
