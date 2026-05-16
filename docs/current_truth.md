# MapForge Current Truth

Last updated: 2026-05-04

## Program Identity
- Repository directory: `map_forge/`
- Public product name: `Carta`
- Primary runtime entry:
  - `src/main.c` -> `map_forge_app_main()`
  - wrapper still delegates run-loop through legacy runtime path for behavior parity

## Current Shipped State
- Runtime scaffold A-D lane is complete:
  - A: viewport stability kernel
  - B: continuity + residency
  - C: package/runtime-source contract discipline
  - D: throughput/productization closure
- Current in-flight worktree boundary:
  - offline tooling decomposition under `tools/`
  - `mapforge_graph.c` now delegates source-format detection/input normalization and output/publish responsibilities into dedicated helper files
  - `mapforge_region.c` now delegates staged build/publish, archive/meta emission, metrics dataset output, and tile-file responsibilities into dedicated helper files
  - goal remains structural reduction with behavior parity and unchanged public region-pack contracts

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active source subsystems:
  - `app`, `camera`, `core`, `map`, `render`, `route`, `ui`
- Dependency policy:
  - `third_party/codework_shared` remains the explicit vendored shared lane

## Core Runtime Contract
- Tile-store contract is explicit in region metadata (`kind`, `root`, optional archive path).
- Runtime source-policy lane is active (`archive_required`, `archive_preferred`, `filesystem_only`) with validation.
- Coverage metadata contract is active and used by runtime queue suppression and diagnostics.
- Offline build-tool contract is active:
  - `mapforge_graph` owns graph ingest + publish for XML/PBF-backed inputs
  - `mapforge_region` owns staged region-pack generation, validation, and publish
  - generated region directories still publish `meta.json`, tile payloads, and dataset/archive companions without changing operator-facing layout

## Verification Contract
- Build/tests:
  - `make -C map_forge clean && make -C map_forge`
  - `make -C map_forge test`
- Primary smoke/stability gates:
  - `make -C map_forge run-headless-smoke`
  - `make -C map_forge test-phase-a-viewport-scenario`
  - `make -C map_forge test-phase-b-continuity-stress`
  - `make -C map_forge test-tile-manager-residency`
  - `make -C map_forge test-phase-d-throughput`

## Packaging and Release Snapshot
- Packaging lanes and release-readiness lanes are complete and maintained.
- Standard package/release contract remains active via `package-desktop*` and release audit/sign/notary targets.

## Current Boundary
- Continue post-A-D maintenance hardening while preserving contract-driven runtime behavior.
- Complete current `tools/` decomposition without changing published operator contract semantics.
- Camera runtime now keeps Mercator zoom semantics, smoothing, and hot screen/world render transforms local while routing generic cursor-anchor zoom and drag-pan target math through vendored shared `core_viewport2d`.
- Throughput guardrail calibration now reflects current local variance:
  - D2 `seattle` matrix precondition uses a higher absolute `l0_peak` ceiling
  - D2 `l0_relief_candidate` defaults now actually reduce pressure (`road_cap=7`, `integrate_cap=60`)
  - D2 matrix stress runs use matrix-local absolute ceilings of `load_clamp=128` and `load_ex=96`; direct D1 budget control remains strict at `load_clamp=48` and `load_ex=72`
  - D2/D3 tuning profiles retry unstable matrix rows up to three times and only compare profile deltas after Seattle/downtown meet the `0.55` coverage floor
  - D2 no longer hard-blocks on single-run profile `load_ex` or `l0_peak` deltas; D2 matrix absolute budgets remain the hard throughput gate and D3 retains throughput deltas as non-blocking trend alerts
  - D3 regression mode loosens matrix-local absolute throughput budgets and lets the matrix continuity gate own coverage-run failures instead of hard-blocking on transient single-frame `min_cov` deltas
  - D2 continuity/fallback/churn checks remain unchanged

## History and Deep Lane References
- Full phase ledgers and archived docs are in:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/map_forge/`
- This file is the compressed public current-state contract.
