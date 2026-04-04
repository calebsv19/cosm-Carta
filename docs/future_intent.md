# MapForge Future Intent

Last updated: 2026-04-02

## Scaffold Alignment Intent
1. Preserve current subsystem decomposition strengths (`app`, `map`, `route`, `render`, etc.).
2. Keep locked scaffold lifecycle wrapper entrypoint stable while preserving behavior.
3. Keep shared subtree usage explicit and stable.
4. Keep verification gates deterministic and non-interactive for migration slices.

## Planned Next Structural Intent
- `MF-S3` (completed):
  - canonical wrapper entry API is now present:
    - `include/map_forge/map_forge_app_main.h`
    - `src/app/map_forge_app_main.c`
  - locked stage symbols are now present.
  - legacy runtime loop remains behavior-preserving through delegation to `app_run_legacy()`.

- `MF-S4` (completed in slices):
  - dependency-lane decision locked: keep `third_party/` exception for vendored-subtree mode.
  - temp/runtime lane hygiene landed: `tmp/` and `data/runtime/` ignored.
  - include strategy locked: keep existing domain include lanes; route new app-level public entry APIs through `include/map_forge/...`.

- `MF-S5` (completed):
  - stabilization closeout completed in scaffold matrix/backlog/private plan.
  - scaffold completion commit title policy executed.

## Post-Scaffold Connection Pass Intent
- `MF-CP0`, `MF-CP1`, `MF-CP2`, and `MF-CP3` are complete:
  - CP0 mapped top-level routing ownership between wrapper and legacy runtime.
  - CP1 introduced context-owned lifecycle stage transitions in `map_forge_app_main`.
  - CP2 extracted explicit per-frame dispatch seam (`app_runtime_dispatch_frame`) and frame-output contract (`AppRuntimeDispatchFrame`) from `app_run_legacy`.
- CP3 separated render derivation and control-plane mutation for tile loading metrics:
  - render path derives tile/loading telemetry via `AppVisibleTileRenderStats`.
  - dispatch path applies mutable loading state and loading timeout updates from explicit frame outputs.
- CP4 hardened resource lifetime/shutdown ownership inside legacy runtime lane:
  - explicit lifetime flags now guard teardown on partial init failures.
  - shutdown order is enforced in reverse-init sequence with idempotent behavior.
- `MF-CP5` complete:
  - closeout verification gates passed.
  - connection-pass docs and global matrix/backlog trackers are synchronized.
- cross-program wrapper initiative (`W1` + `W2`) is complete:
  - wrapper diagnostics are now aligned to the canonical contract (error taxonomy, stage-order diagnostics, dispatch summary, exit summary).
  - next connection-pass posture for `map_forge` is optional `CP6+` deeper runtime extraction away from `app_run_legacy()` only if needed.

## IR1 Input Routing Intent
- `IR1-S0` + `IR1-S1` + `IR1-S2` + `IR1-S3` complete:
  - top-level input routing seam now has explicit typed contracts and phase boundaries at runtime dispatch:
    - intake
    - normalize
    - route
    - invalidate
  - normalize policy now includes explicit text-entry shortcut gating with unit-test coverage.
- diagnostics closeout now includes bounded runtime roll-up of input counters in perf logs.
- next planned slice:
  - maintain-only IR1 posture while RS1 continues.
- shared-first constraint for this lane:
  - only extract contract-only helpers to shared libraries when at least one more program converges on the same shape; keep program behavior local.

## RS1 Render Split Intent
- `RS1-S0` + `RS1-S1` + `RS1-S2` + `RS1-S3` complete:
  - render lane now has explicit derive/submit typed contracts.
  - dispatch/perf diagnostics now expose derive vs submit timing/counters and invalidation reason bits consumed by render lane.
  - explicit render title contract is now in place so overlay/title formatting derivation is no longer mixed with submit-stage rendering.
- next planned slice:
  - maintain-only RS1 posture; no additional render-split extraction is currently required.
- shared-first constraint for this lane:
  - keep map/render policy behavior local.
  - shared diagnostics helper baseline is now active via `kit_runtime_diag` (`v0.1.0`) for timing/counter math.
  - consider expanding shared contract-only helpers only if `physics_sim`/`ray_tracing` converge on equivalent render/input contract shapes.

## App Packaging Intent
- `MF-PK0` complete:
  - baseline package map recorded for runtime-relative roots, required resources, and standard target parity goals.
- `MF-PK1` complete:
  - standard `package-desktop*` target set landed in `Makefile`.
  - launcher/plist and bundle resource copy paths are now in place.
- `MF-PK2` complete:
  - closeout verification and tracker synchronization are complete for the initial packaging lane.
- packaging next-step posture:
  - maintain parity with packaging standard contract updates (no active packaging migration slice pending for `map_forge`).
  - use the post-PK2 hardening flow as packaging reference for other data-dependent apps:
    - avoid external data-root dependence for icon/Finder launch,
    - verify packaged shader/runtime roots explicitly,
    - require launcher diagnostics (`--print-config`) and log-based backend confirmation before lane closeout.

## Non-Goals During Scaffold Migration
- No feature expansion unrelated to scaffold alignment.
- No shared subtree redesign in migration commits.
- No broad naming churn in one pass; only bounded slices with verification gates.
