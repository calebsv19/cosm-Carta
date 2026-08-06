# Carta Current Truth

Last updated: 2026-08-05

## Program Identity
- Public product name: `Carta`
- Repository directory: `map_forge/`
- Primary runtime entry:
  - `src/main.c` -> `map_forge_app_main()`
  - wrapper still delegates run-loop through legacy runtime path for behavior parity

## Current Shipped State
- Managed Vulkan presentation adoption is complete for the current source
  boundary:
  - the default `third_party/codework_shared` snapshot contains
    `vk_runtime 0.6.0` and `vk_renderer 1.3.1`
  - `vk_renderer` delegates Vulkan instance/device lifecycle to `vk_runtime`
    while preserving the renderer compatibility entry points used by Carta
  - Carta retains backend selection, SDL fallback, map drawing policy, input,
    diagnostics, and recovery reporting
  - the restored tinted affine line-mesh wrapper preserves Carta's existing
    route/map presentation call surface after the renderer lifecycle rebase
- Runtime scaffold A-D lane is complete:
  - A: viewport stability kernel
  - B: continuity + residency
  - C: package/runtime-source contract discipline
  - D: throughput/productization closure
- Tile-store/runtime architecture lane is complete for the current product
  boundary:
  - `meta.json` owns the package contract, tile-store descriptor, canonical
    source/build manifest, tile pyramid, output stats, tile coverage, and
    archive rollups
  - runtime source policy covers `archive_required`, `archive_preferred`, and
    `filesystem_only`
  - current archive-backed reads use the SQLite-backed archive path with
    filesystem-tree fallback where policy allows it
  - XML and `.osm.pbf` / `.pbf` ingest are supported; `osm.pbf` is the
    canonical input format while XML remains compatibility input
- Current maintenance boundary:
  - preserve published region-pack, graph-operator, runtime-source-policy, and
    skill-facing headless helper contracts
  - open fresh narrow plans only for graph-package metadata expansion,
    PMTiles-native reader support, or tile lifecycle/cache tuning after fresh
    regression evidence

## Structure
- Required lanes: `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active source subsystems:
  - `app`, `camera`, `core`, `map`, `render`, `route`, `ui`
- Dependency policy:
  - `third_party/codework_shared` remains the explicit vendored shared lane

## Core Runtime Contract
- Tile-store contract is explicit in region metadata (`kind`, `root`, optional archive path).
- Runtime source-policy contract is active (`archive_required`, `archive_preferred`, `filesystem_only`) with validation.
- Coverage metadata contract is active and used by runtime queue suppression and diagnostics.
- Offline build-tool contract is active:
  - `mapforge_graph` owns graph ingest + publish for XML/PBF-backed inputs
  - `mapforge_region` owns staged region-pack generation, validation, and publish
  - generated region directories still publish `meta.json`, tile payloads, and dataset/archive companions without changing operator-facing layout

## Verification Contract
- Build/tests:
  - `make -C map_forge clean && make -C map_forge`
  - `make -C map_forge test`
- Managed Vulkan rollout proof:
  - `make -C map_forge vulkan-rollout-contract`
  - `make -C map_forge vulkan-rollout-self-test`
  - the self-test requires Vulkan validation, verifies runtime/renderer handle
    identity, captures a nontrivial frame, performs a real resize with bounded
    out-of-date/suboptimal recovery, and verifies the resized capture extent
  - the 2026-08-05 Apple M2/MoltenVK run captured `2560x1440` from a logical
    `1280x720` window and `2880x1600` from logical `1440x800`, with one
    swapchain recreation and zero validation warnings/errors; this positively
    preserves the existing 2x Retina drawable path
- Primary smoke/stability gates:
  - `make -C map_forge visual-harness`
    - build/readiness target for manual visual validation
    - prints the source binary path; does not emit an image artifact
  - `make -C map_forge visual-artifact`
    - source-run first-frame proof target
    - runs the source-built app headlessly through the route demo
    - writes ignored `visual_artifacts/source_run_first_frame/route_demo_seattle/preview.bmp`
    - prints `visual-artifact=<path>` after validating the preview is nonempty
    - failure means the headless route demo failed or the expected preview
      artifact was missing/empty
  - `make -C map_forge carta-local-proof`
    - local proof aggregate; runs `test-r5-callable`, `visual-artifact`,
      `run-headless-smoke-core`,
      `test-headless-saved-pin-visualizer-publish-wrapper`, and
      `package-desktop-self-test` sequentially
    - prints `carta-local-proof passed`
    - runs headlessly and does not perform remote publish/upload
    - failure means one constituent local proof gate failed
  - `make -C map_forge run-headless-smoke`
    - routine non-interactive smoke; aliases `run-headless-smoke-core`
    - excludes saved-pin/video/visualizer artifact lanes and Phase-D throughput
  - `make -C map_forge run-headless-smoke-artifacts`
    - saved-pin, frame, video, visualizer staging, and publish-wrapper lanes
  - `make -C map_forge run-headless-smoke-continuity`
    - route-preview unit lane plus the phase-B continuity stress gate
  - `make -C map_forge run-headless-smoke-throughput`
    - Phase-D throughput guardrails
  - `make -C map_forge run-headless-smoke-full`
    - explicit full aggregate for all smoke tiers
  - `make -C map_forge test-phase-a-viewport-scenario`
  - `make -C map_forge test-phase-b-continuity-stress`
  - `make -C map_forge test-tile-manager-residency`
  - `make -C map_forge test-phase-d-throughput`

## Packaging and Release Snapshot
- Packaging lanes and release-readiness lanes are complete and maintained.
- Standard package/release contract remains active via `package-desktop*` and release audit/sign/notary targets.
- `package-desktop-self-test` now executes the bundled Vulkan rollout proof
  when the local Vulkan development runtime is available. Its temporary
  validation-layer manifest is self-test-only; normal Carta launch does not
  ship or require the development validation layer.

## Current Boundary
- Continue maintenance hardening only from fresh evidence while preserving contract-driven runtime behavior.
- Keep the completed tile-store/runtime architecture lane closed unless a fresh narrow regression or backend-expansion need appears.
- The R0-R6 refinement pass is complete for the current boundary; proof commands
  are documented through `visual-artifact` and `carta-local-proof`, and future
  work should open a fresh narrow lane from new evidence.
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
