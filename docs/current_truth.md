# MapForge Current Truth

Last updated: 2026-04-03

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
- Input policy gate:
  - `make -C map_forge test-input-policy`
  - validates text-entry shortcut precedence rules for top-level normalize lane.
- Header UI behavior:
  - right-side layer chips render inside a clipped strip viewport and support mouse-wheel horizontal scrolling when hovered.

## Release Readiness Status (Current)
- `MF-RL0` complete (release contract freeze for pilot):
  - bundle identifier locked in plist: `com.cosm.carta`
  - version source-of-truth file added: `VERSION` (`0.1.0`)
  - Makefile release identity variables added:
    - `RELEASE_PRODUCT_NAME := Carta`
    - `RELEASE_PROGRAM_KEY := map_forge`
    - `RELEASE_BUNDLE_ID := com.cosm.carta`
    - `RELEASE_VERSION_FILE ?= VERSION`
    - `RELEASE_CHANNEL ?= stable`
    - `RELEASE_ARTIFACT_BASENAME := Carta-<version>-macOS-<channel>`
  - release env contract placeholders added:
    - `APPLE_SIGN_IDENTITY`
    - `APPLE_NOTARY_PROFILE`
    - `APPLE_TEAM_ID`
  - release contract inspection target added:
    - `make -C map_forge release-contract`
- `MF-RL1` complete (bundle audit + writable-path hardening):
  - writable runtime/config persistence roots are launcher-owned and exported:
    - `MAPFORGE_RUNTIME_DIR`
    - `MAPFORGE_THEME_PERSIST_PATH`
  - launcher now falls back to `TMPDIR` support lane when default app-support root is not writable (keeps packaged startup stable in restricted sandboxes).
  - packaged dylib dependency closure is now automated:
    - `tools/packaging/macos/bundle-dylibs.sh`
    - bundled to `Contents/Frameworks` with install-name rewrites for app-local runtime loading.
  - ad-hoc re-sign pass now runs at the end of package assembly:
    - re-signs bundled frameworks + `mapforge-bin` + launcher + app bundle after install-name rewrites/resource copy.
    - prevents Finder launch failure from invalidated code pages after `install_name_tool` edits.
  - release bundle audit target added and passing:
    - `make -C map_forge release-bundle-audit`
    - verifies bundle id, launcher print-config contract, writable persistence paths (not in app bundle), and non-portable dependency bans (`/opt/homebrew`, `/usr/local/Cellar`, workspace paths) across app binary + bundled frameworks.
- release lane status:
  - `MF-RL2` signing + notarization integration is complete with real Developer ID credentials:
    - notarization accepted submissions:
      - `00c34bf7-eed3-49ff-8a5c-6976e676b5a6`
      - `8595ba98-fa62-4fd4-a604-e44aeb18eca7`
      - `07120954-c306-4942-b766-e3febf393f9d`
      - `73f36611-bda2-49c0-8dd0-d36d64d5b42c`
    - stapling and staple-validation pass on current distribution app.
  - release target graph now includes explicit production-safe flow:
    - `release-verify-signed`
    - `release-verify-notarized`
    - `release-distribute` (one-shot: sign -> notarize accepted -> staple -> verify -> artifact)
  - signing rules locked from pilot:
    - ad-hoc path keeps `--timestamp=none` for local/dev signing.
    - Developer ID path requires secure timestamp and hardened runtime on executables.
    - notarization target fails on non-`Accepted` status and writes notary JSON/logs under `build/release/`.
  - launch resiliency lock:
    - when Vulkan window creation fails early (`VK_KHR_surface` portability gap), app now falls back to SDL window creation instead of immediate process exit.

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
- cross-program wrapper initiative update (`W1` + `W2`) complete:
  - wrapper context now carries normalized dispatch diagnostics:
    - `dispatch_count`
    - `dispatch_succeeded`
    - `last_dispatch_exit_code`
  - wrapper boundary diagnostics are standardized:
    - structured wrapper error taxonomy
    - function-context stage transition violations (`expected`/`actual`/`next`)
    - wrapper exit summary (stage, exit code, dispatch summary, wrapper error)
  - verification gates passed:
    - `make -C map_forge clean && make -C map_forge`
    - `make -C map_forge test`
    - `make -C map_forge run-headless-smoke`
    - `make -C map_forge visual-harness`

## IR1 Input Routing Status (Current)
- `IR1-S0` complete:
  - current top-level input flow was re-mapped at dispatch boundary and captured in private execution docs.
- `IR1-S1` complete:
  - typed input contracts are now explicit in top-level app runtime:
    - `AppInputEventRaw`
    - `AppInputEventNormalized`
    - `AppInputRouteResult`
    - `AppInputInvalidationResult`
  - explicit phase function landed:
    - `app_runtime_process_input_frame(...)`
    - sequencing: `InputIntake -> InputNormalize -> InputRoute -> InputInvalidate`
  - `AppRuntimeDispatchFrame` now carries the `input` diagnostics payload.
- `IR1-S2` complete:
  - text-entry precedence policy is now explicit and testable:
    - policy module: `app_runtime_input_policy.c`
    - gating API: `app_runtime_apply_text_entry_shortcut_policy(...)`
  - normalize phase now applies text-entry shortcut gating before global route classification.
  - policy test lane added:
    - `tests/app_runtime_input_policy_test.c`
    - `make -C map_forge test-input-policy` (also included in `test` + `run-headless-smoke`)
- `IR1-S3` complete:
  - runtime diagnostics now roll up input-routing counters in existing perf logs (both backends):
    - frame-level: raw/normalized/gate/route/invalidation + reason bits
    - cumulative: totals for raw/actions/gated/route buckets/invalidation buckets
- behavior status:
  - runtime behavior remains preserved; this slice is contract/instrumentation first.
- lane status:
  - map_forge IR1 is closed (`S0` through `S3`).
  - next top-level contract lane for map_forge is active `RS1`.

## RS1 Render Split Status (Current)
- `RS1-S0` complete:
  - render/update ownership baseline remapped against RS1 contract shape.
- `RS1-S1` complete:
  - typed render contracts added:
    - `AppRuntimeRenderDeriveFrame`
    - `AppRuntimeRenderSubmitFrame`
  - explicit phase functions added:
    - `app_runtime_render_derive_frame(...)`
    - `app_runtime_render_submit_frame(...)`
- `RS1-S2` complete:
  - dispatch contract now carries render split diagnostics:
    - `after_render_derive`
    - `render_draw_pass_count`
    - `render_invalidation_reason_bits`
  - runtime perf logs now report:
    - derive timing (`rderive`)
    - submit timing (`rsubmit`)
    - draw pass count
- `RS1-S3` complete:
  - explicit window-title contract added:
    - `AppRuntimeRenderTitleFrame`
    - `app_runtime_render_derive_title_frame(...)`
    - `app_runtime_render_apply_title_frame(...)`
  - title/overlay derivation is now separated from render submit and applied as an explicit post-submit contract stage.
- shared diagnostics contract adoption complete:
  - runtime perf diagnostics timing/counter math now uses shared `kit_runtime_diag` (`v0.1.0`) helpers.
  - app behavior ownership remains local (`map_forge` input/routing/render policy unchanged).
- lane status:
  - map_forge RS1 is closed (`S0` through `S3`).

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
