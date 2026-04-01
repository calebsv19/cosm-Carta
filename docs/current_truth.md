# MapForge Current Truth

Last updated: 2026-03-31

## Program Identity
- Repository directory: `map_forge/`
- Public product name in README: `Carta`
- Primary runtime entry path today:
  - `src/main.c` -> `map_forge_app_main()` in `src/app/map_forge_app_main.c`
  - wrapper run-loop stage still delegates to `app_run_legacy()` in `src/app/app.c` for behavior parity
  - lifecycle stage transitions are now context-owned and stage-validated in `map_forge_app_main`

## Current Structure
- Required scaffold lanes are present:
  - `docs/`, `src/`, `include/`, `tests/`, `build/`
- Active source subsystem lanes:
  - `app`, `camera`, `core`, `map`, `render`, `route`, `ui`
- Header strategy:
  - include-dominant (`include/*`), with mirrored subsystem directories.
  - project-level entry/lifecycle headers use `include/map_forge/*`.

## Dependency Lane Policy
- `map_forge` keeps `third_party/` as an explicit scaffold exception because this repo runs in vendored-subtree mode (`third_party/codework_shared`).
- No `third_party` -> `external` rename is planned for this migration track.

## Include Namespace Strategy
- Existing domain include lanes remain in place (`include/app`, `include/map`, `include/route`, etc.) to avoid churn-heavy renames.
- New cross-subsystem/public app entry APIs use project namespace:
  - `include/map_forge/...`
- Private/local-only headers remain allowed in `src/<subsystem>/` when locality is clearer.

## Runtime/Verification Contract (Current)
- Build:
  - `make -C map_forge clean && make -C map_forge`
- Full non-interactive tests:
  - `make -C map_forge test`
- Headless smoke gate (non-interactive):
  - `make -C map_forge run-headless-smoke`
  - current implementation validates app build + focused runtime-adjacent smoke tests without launching interactive UI.
- Visual harness build gate:
  - `make -C map_forge visual-harness`
  - current implementation is an app-build availability alias.
- Header UI behavior:
  - right-side layer chips render inside a clipped strip viewport and support mouse-wheel horizontal scrolling when hovered.

## App Packaging Status (Current)
- `MF-PK0` complete:
  - packaging baseline/resources map captured against app-relative runtime assumptions (`config/`, `assets/`, `data/runtime/`, Vulkan shader root, regions root).
  - target contract locked to scaffold packaging standard (`package-desktop*` target set).
- `MF-PK1` complete:
  - `.app` bundle assembly landed in `Makefile` with standardized targets:
    - `package-desktop`
    - `package-desktop-smoke`
    - `package-desktop-self-test`
    - `package-desktop-copy-desktop`
    - `package-desktop-sync`
    - `package-desktop-open`
    - `package-desktop-remove`
    - `package-desktop-refresh`
  - launcher/plist/resources path landed:
    - `tools/packaging/macos/mapforge-launcher`
    - `tools/packaging/macos/Info.plist`
  - public packaging reference added:
    - `docs/desktop_packaging.md`
- `MF-PK2` complete:
  - packaging closeout verification gates passed:
    - `make -C map_forge clean && make -C map_forge`
    - `make -C map_forge package-desktop`
    - `make -C map_forge package-desktop-smoke`
    - `make -C map_forge package-desktop-self-test`
    - `make -C map_forge package-desktop-copy-desktop`
    - `make -C map_forge package-desktop-refresh`
  - desktop launch dispatch gate executed:
    - `make -C map_forge package-desktop-open`
  - packaging docs/trackers synchronized for PK0-PK2.
- post-PK2 hardening complete:
  - launcher now emits deterministic startup diagnostics (`--print-config`, selection reason, logfile path).
  - packaged default backend is Vulkan unless explicitly overridden.
  - runtime-compatible shader mirror added at `Contents/Resources/shaders/*` to prevent packaged Vulkan shader-root mismatch.
  - region packs are bundled into `Contents/Resources/data/regions` so icon/Finder launch does not require external dev-path access.

## Connection Pass Status (Current)
- `MF-CP0` complete:
  - top-level routing map captured across wrapper and legacy runtime lanes.
  - subsystem handoff boundaries identified for next extraction waves.
- `MF-CP1` complete:
  - top-level app context introduced (`MapForgeAppContext`) with explicit lifecycle stage tracking.
  - stage transitions are now guarded and order-validated (`bootstrap` -> `config_load` -> `state_seed` -> `subsystems_init` -> `runtime_start` -> `run_loop` -> `shutdown`).
  - wrapper still delegates runtime execution to `app_run_legacy()` to preserve behavior before deeper extraction.
- `MF-CP2` complete:
  - frame dispatch seam extracted into `app_runtime_dispatch_frame(...)`.
  - explicit dispatch output contract added (`AppRuntimeDispatchFrame`) for event/update/queue/route/render timing boundaries and global-control skip behavior.
  - `app_run_legacy()` now uses the dispatch seam while preserving runtime behavior.
- `MF-CP3` complete:
  - update/render boundary tightened so render-derived tile metrics are produced as explicit frame outputs (`AppVisibleTileRenderStats`) instead of mutating cross-phase state inside tile draw.
  - dispatch/control lane now applies `loading_expected`, `loading_done`, and `vk_asset_misses` state after render and advances loading timeout using those explicit outputs.
  - frame contract now carries loading metrics (`AppRuntimeDispatchFrame.loading_*`) so stalled-loading checks in the runtime loop consume dispatch outputs directly.
- `MF-CP4` complete:
  - runtime lifetime ownership is now explicit via `AppRuntimeLifetime` flags in `AppState`.
  - `app_init()` marks subsystem ownership as each initialization stage succeeds (SDL/window/renderer/workers/tile loader/cache/TTF/trace/route state).
  - `app_shutdown()` is idempotent and reverse-order guarded, preventing unsafe teardown calls on partially initialized subsystems.
  - failure paths now route through one deterministic cleanup lane without calling shutdown APIs for subsystems that never initialized.
- `MF-CP5` complete:
  - closeout verification gates passed:
    - `make -C map_forge clean && make -C map_forge`
    - `make -C map_forge test`
    - `make -C map_forge run-headless-smoke`
    - `make -C map_forge visual-harness`
  - connection-pass docs/matrix trackers are synchronized for CP0-CP5 completion.

## Runtime State Persistence
- Committed default config:
  - `config/app.config.json`
- Runtime-persisted mutable state:
  - `data/runtime/app_state.json`
- Persisted text zoom state:
  - `map_view.text_zoom_step` in `data/runtime/app_state.json`
- Load order:
  - runtime file first, then default config fallback.
- Save path:
  - runtime file only (`data/runtime/app_state.json`), so normal app runs do not dirty tracked config defaults.

## Active Scaffold Migration State
- Private migration plan:
  - `../docs/private_program_docs/map_forge/2026-03-27_map_forge_scaffold_standardization_switchover_plan.md`
- Baseline freeze:
  - `../docs/private_program_docs/map_forge/2026-03-27_mf_s0_baseline_freeze_and_mapping.md`
- Completed phases:
  - `MF-S0`, `MF-S1`, `MF-S2`, `MF-S3`, `MF-S4`, `MF-S5`
- Next phase:
  - `.app` packaging baseline is closed (`MF-PK0` + `MF-PK1` + `MF-PK2` complete).
