# MapForge Docs

Start here for the public, operator-facing documentation that ships with the repository.
Last audited: 2026-05-04.

## Current Focus

- Runtime scaffold A-D is closed; continuity/residency guardrails remain active and are still the main non-GUI safety gates.
- Offline tooling decomposition is currently in progress under `tools/`:
  - `tools/mapforge_graph.c` now routes graph ingest through `tools/mapforge_graph_source.c` and graph output/publish through `tools/mapforge_graph_output.c`.
  - `tools/mapforge_region.c` now routes staged region-pack production across dedicated source, publish, archive/meta, metrics-dataset, and tile-file helper seams.
- Camera input math now partially adopts vendored shared `core_viewport2d`:
  - cursor-anchor zoom and drag-pan target math route through a camera-local bridge
  - Mercator semantics, smoothing, and hot render transforms remain app-local for parity/performance
- Phase D throughput harness remains active and currently calibrated to:
  - tolerate higher absolute `seattle` `l0_peak` variance in D2 matrix preconditions
  - use a lower-pressure `l0_relief_candidate` preset (`road_cap=7`, `integrate_cap=60`)
  - keep D1's strict absolute budget gate separate from the D2 profile matrix, whose default absolute load ceilings are `load_clamp=128` and `load_ex=96`
  - retry unstable D2/D3 profile matrix rows up to three times before comparing profile deltas, requiring Seattle/downtown to meet the `0.55` coverage floor
  - keep D2 profile `load_ex`/`l0_peak` deltas out of the hard-block path because the matrix absolute budgets already own hard throughput gating and D3 keeps deltas as non-blocking trend pressure
  - keep D3 hard-blocking focused on matrix continuity-contract failures rather than transient single-frame `min_cov` deltas, with absolute throughput budgets reported as trend pressure

## Start Here

- `docs/ADDING_OSM_REGIONS.md`: import a new OSM file and build a local region pack.
- `docs/KEYBINDS.md`: runtime controls.
- `docs/REGION_PACK_LAYOUT.md`: generated region directory structure and metadata contract.
- `docs/desktop_packaging.md`: macOS `.app` packaging workflow and launcher/runtime root behavior.

## Formats And Data Contracts

- `docs/MFT_V1.md`: render tile binary format.
- `docs/GRAPH_V1.md`: original routing graph format notes.
- `docs/GRAPH_V2.md`: current routing graph format and compatibility notes.
- `docs/OSM_ROAD_CLASSES.md`: OSM tag-to-road-class mapping used by the tooling.
- `docs/carta_route_job_format.md`: headless route-job JSON contract.
- `docs/carta_pin_format.md`: persistent pin JSON contract.

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
4. `src/app/route/app_route.c` and `src/app/route/app_route_service.c`: route graph worker flow plus route selection/toggle boundary.
5. `src/app/app_tile_pipeline.c` and `src/render/vk_tile_cache_policy.c`: async tile/Vulkan queue integration and cache slot policy boundary.
6. `tools/mapforge_region.c` plus `tools/mapforge_region_*.c`, and `tools/mapforge_graph.c` plus `tools/mapforge_graph_*.c`: offline region-build and graph-build orchestration.

## Non-GUI Stability Checks

- `make test-presentation-stability`
- `make test`
- `make run-headless-smoke`
- `make test-phase-a-viewport-scenario`
- `make test-phase-b-continuity-stress`
- `make test-tile-manager-residency`
- `make test-phase-d-throughput`
- `make test-phase-d2-guardrails`

## Scaffold Migration References

- Private migration plan:
  - `../../docs/private_program_docs/map_forge/2026-03-27_map_forge_scaffold_standardization_switchover_plan.md`
- Baseline freeze snapshot:
  - `../../docs/private_program_docs/map_forge/2026-03-27_mf_s0_baseline_freeze_and_mapping.md`

## Connection Pass References

- CP0/CP1 execution snapshot:
  - `../../docs/private_program_docs/map_forge/2026-03-30_map_forge_connection_pass_cp0_cp1_execution.md`
- CP2 dispatch extraction snapshot:
  - `../../docs/private_program_docs/map_forge/2026-03-30_map_forge_connection_pass_cp2_dispatch_extraction.md`
- CP3 update/render separation snapshot:
  - `../../docs/private_program_docs/map_forge/2026-03-30_map_forge_connection_pass_cp3_update_render_separation.md`
- CP4 resource lifetime hardening snapshot:
  - `../../docs/private_program_docs/map_forge/2026-03-30_map_forge_connection_pass_cp4_resource_lifetime_hardening.md`
- CP5 closeout snapshot:
  - `../../docs/private_program_docs/map_forge/2026-03-30_map_forge_connection_pass_cp5_closeout.md`

## App Packaging References

- Public packaging guide:
  - `docs/desktop_packaging.md`
- PK0/PK1 execution snapshot:
  - `../../docs/private_program_docs/map_forge/2026-03-31_map_forge_app_packaging_cp0_cp1_execution.md`
- PK2 closeout snapshot:
  - `../../docs/private_program_docs/map_forge/2026-03-31_map_forge_app_packaging_cp2_closeout.md`
  - includes post-closeout hardening notes from real Finder-launch failures and fixes

## Runtime Config Persistence

- default (tracked): `config/app.config.json`
- runtime state (ignored): `data/runtime/app_state.json`

## Scaffold Policy Locks (MapForge)

- dependency lane: keep `third_party/` for vendored subtree mode (`third_party/codework_shared`)
- include strategy: retain existing domain include lanes; use `include/map_forge/` for new app-level public entry APIs

## Private Planning Docs

Private plans, execution checklists, and internal runbooks were moved out of this repo lane. They live in the CodeWork workspace at:

- `../../docs/private_program_docs/map_forge/`
