# MapForge Current Truth

Last updated: 2026-04-27

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
  - tile-pipeline modularization split around helper/runtime seams
  - goal is structural reduction with behavior parity

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
- Complete current tile-pipeline modularization without changing published operator contract semantics.
- Camera runtime now keeps Mercator zoom semantics, smoothing, and hot screen/world render transforms local while routing generic cursor-anchor zoom and drag-pan target math through vendored shared `core_viewport2d`.

## History and Deep Lane References
- Full phase ledgers and archived docs are in:
  - `/Users/calebsv/Desktop/CodeWork/docs/private_program_docs/map_forge/`
- This file is the compressed public current-state contract.
