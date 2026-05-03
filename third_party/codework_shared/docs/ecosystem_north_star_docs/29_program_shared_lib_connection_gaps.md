# Program Shared-Lib Connection Gaps

Last updated: 2026-05-03
Purpose: canonical per-program list of shared-lib connection gaps and next integrations.

Use this with:
- `../11_version_compat_matrix.md` for minimum supported versions.
- `../../SHARED_LIBS_CURRENT_STATE.md` for current adoption snapshot.

## Gap Legend
- `Missing`: not wired in build/runtime/tooling where it would provide clear value.
- `Partial`: wired, but only in additive or narrow paths; broader consolidation still open.
- `Stabilize`: wired broadly; remaining work is hardening/cleanup/documentation.

## Per-Program Gap List

### `datalab`
Current shared profile:
- Strong on `core_base/core_io/core_data/core_pack`, `core_font`, and `kit_viz`.
- First incremental `kit_graph_timeseries` adoption is now in place (shared stride guidance for trace decimation).

Gaps:
- `Stabilize`: `core_viewport2d` proving-host adoption is now in place for sketch/image lanes; keep viewport persistence, fit-reset behavior, and future large-image/tiled follow-ons aligned to the shared math contract instead of regressing into app-local viewport drift.
- `Partial`: continue `kit_graph_timeseries` migration (shared view math, hover inspection, then one shared draw path).
- `Missing`: `core_trace` trace-import/view path standardization.
- `Missing`: `core_theme` adoption if/when DataLab UI colors should align with shared preset tokens.
- `Partial`: normalize profile loaders so all external data lanes map through one shared parsing contract.

### `daw`
Current shared profile:
- `core_base`, `core_io`, `core_time`, `core_queue`, `core_sched`, `core_jobs`, `core_wake`, `core_kernel`, `core_theme`, `core_font`, `kit_viz` adopted.
- `core_data` + `core_pack` mainly additive/diagnostics.

Gaps:
- `Stabilize`: no mandatory shared-lib gap remains for the current DAW rollout plan.
- `Stabilize`: centralized UI font lane now partially adopts vendored `kit_render_external_text.*` for active Vulkan draw/measure while preserving the `daw_default` preset; the only remaining local seam is clipped draw because the shared external text runtime still lacks source-rect crop support.
- `Stabilize`: Slice 3 complete (data contract hardening) - DAW dataset metadata now includes additive `schema_family`/`schema_variant` keys and deterministic contract coverage for canonical `daw_selection_v1` table.
- `Stabilize`: Slice 2 complete (pack/data contract parity guard) - deterministic `daw_pack_contract_parity_test` now verifies `DAWH/WMIN/WMAX/MRKS/JSON` chunk presence plus canonical `core_dataset` schema keys (`daw_timeline_v1`, `dataset_schema`, `dataset_contract_version`).
- `Stabilize`: Slice 4 complete (trace diagnostics lane) - deterministic `daw_trace_export_contract_test` now verifies canonical transport/scheduler timing lanes and `trace_start/trace_end` markers exported through shared `core_trace`.
- `Stabilize`: Slice 5 complete (workers lane adoption) - async diagnostics trace export now uses shared `core_workers` with deterministic completion/contract coverage (`daw_trace_export_async_contract_test`), while preserving runtime-loop behavior.

### `fisiCs`
Current shared profile:
- `sys_shims` is strong.
- `core_base/core_io/core_data/core_pack` are partial/additive.

Gaps:
- `Partial`: expand core usage beyond diagnostics/utility flows into more compiler/runtime helper paths.
- `Partial`: stabilize diagnostics schema contracts and pack artifacts with reader validation.
- `Missing`: execution-core adoption (only if/when runtime-loop orchestration enters scope).

### `gravity_orbit_sim`
Current shared profile:
- `core_pane` is now adopted in a narrow `workspace-linked` lane for
  pane-shell geometry, and live splitter hover/drag now adopts shared
  `kit_pane`.
- `core_io` is now adopted for close/reopen session-state file reads/writes.
- `core_theme` and `core_font` are now adopted for shell palette/font defaults.
- `core_sim` is now adopted as the first proving-host lane for fixed-step
  playback/single-step shell orchestration.
- `core_viewport2d` is now adopted for cursor-anchor zoom, drag-pan, and
  fit-reset camera math through an app-local world-meter bridge.
- first `kit_render` adoption is now in place for shared role/tier text sizing
  and text zoom policy through a `KIT_RENDER_BACKEND_NULL` context.
- UTF-8 draw runtime ownership and simulation-body colors remain app-local by
  design.

Gaps:
- `Stabilize`: first shared pane-resize slice is now complete; keep pane
  meaning, menu-button behavior, viewport transforms, and simulation semantics
  app-local while shared `core_pane >= 0.2.0` owns split solve and shared
  `kit_pane >= 0.2.0` owns splitter hover/drag interaction state.
- `Stabilize`: first `core_sim` proving-host slice is now complete; keep
  orbit bodies, gravity force accumulation, integration equations, rendering,
  and scenario persistence app-local while shared `core_sim >= 0.1.0` owns the
  fixed-step accumulator, pause/play/single-step, max-tick clamp, and frame
  outcome contract.
- `Stabilize`: keep the current text bridge honest about its boundary:
  `core_theme` / `core_font` / `kit_render` own policy, while active SDL host
  draw/runtime stays local unless the host later moves onto a shared renderer
  backend that can actually consume `kit_render_external_text.*`.
- `Stabilize`: first shared `core_viewport2d` camera slice is now complete;
  keep world-meter bridge policy, viewport input routing, edit-handle hit
  semantics, and far-body despawn behavior app-local while shared
  `core_viewport2d >= 0.1.0` owns generic pan/zoom/fit math.
- `Stabilize`: low-risk `core_io` cleanup is now in place for session-state
  persistence (`core_io_path_exists` + `core_io_read_all` /
  `core_io_write_all`); directory-create helpers and the session schema remain
  app-local for now.
- `Missing`: `core_layout`, `core_pane_module`, and `core_pane_snapshot`
  should stay deferred until the app has real pane authoring or persistent
  pane-module semantics.
- `Missing`: `core_trace` should stay deferred until diagnostics become an
  artifact/export lane instead of only an on-screen overlay.
- `Missing`: `core_data` / `core_pack` should stay deferred until scenario
  persistence expands beyond the current plain-text authoring seam.

### `ide`
Current shared profile:
- Full execution-core stack adopted (`time/queue/sched/jobs/workers/wake/kernel`).
- `core_base/core_io/core_data/core_pack` and `core_theme/core_font` are adopted.

Gaps:
- `Partial`: complete migration of remaining ad-hoc file/diagnostics paths into shared `core_io`/`core_data` patterns.
- `Partial`: move from export-first `core_pack` diagnostics into broader standardized snapshot/restore contracts (if needed).
- `Stabilize`: continue execution-core hardening tests (idle policies, wake behavior, shutdown boundaries).
- `Stabilize`: Timer HUD text now routes through the central IDE text helpers; any further font/runtime cleanup should stay bounded and avoid reopening the main reference text stack unless there is clear cross-host value.

### `line_drawing`
Current shared profile:
- `core_base`, `core_scene`, `core_math`, `core_time`, `core_theme`, `core_font` adopted.
- `core_io/core_data/core_pack/core_trace` are additive/partial.
- pane-shell geometry now also adopts shared `core_pane`, and live splitter hover/drag now adopts shared `kit_pane` through the vendored subtree host.

Gaps:
- `Partial`: shared-scene contract rollout is now through the compile-lane primitive hardening seam:
  - `line_drawing` export validates root/object metadata through `core_scene 1.1.0`
  - authored plane/prism objects emit canonical `primitive` payloads directly on exported `objects[]` entries
  - `core_scene_compile 0.3.0` now explicitly validates those canonical primitive payloads and preserves them in `scene_runtime_v1`
  - remaining work is deterministic fixture expansion plus eventual root-level `scene3d` promotion out of `extensions.line_drawing.*`
- `Stabilize`: build/package/tooling host paths now resolve shared modules through vendored `third_party/codework_shared` instead of direct live `../shared` linkage.
- `Stabilize`: runtime import policy locked to JSON-only (`.pack` remains diagnostics-tooling only).
- `Stabilize`: `core_data` schema parity with 3D is now locked for shared metadata + shared `anchors_v1`/`walls_v1` tables; 3D-only fields are additive via `anchors_3d_ext_v1`.
- `Stabilize`: `core_pack` diagnostics contract parity with 3D is now locked (shared chunk sequence + shared base `LDAN` layout + additive `LDA3` extension).
- `Stabilize`: `core_trace` tooling consistency now aligned with 3D sibling (shared targets, CLI, and output lane contract).
- `Stabilize`: low-risk `core_io` cleanup completed for theme preset persistence (`core_io_path_exists` + `core_io_read_all`/`core_io_write_all`); remaining directory/create helpers stay app-local for now.
- `Stabilize`: first font-runtime unification slice is complete; active Vulkan text plus the former scattered fallback UI text paths now route through the centralized bridge/helper layer over shared `kit_render`, with only bounded centralized non-Vulkan fallback behavior and emergency local font-path ownership left intentionally app-local.
- `Stabilize`: first pane-host retrofit is now complete; `line_drawing` keeps pane meaning app-local while shared `core_pane >= 0.2.0` owns split solve and shared `kit_pane >= 0.2.0` owns splitter hover/drag interaction state.
- `Missing`: execution-core adoption beyond `core_time` where background/task orchestration appears.

### `map_forge`
Current shared profile:
- `core_base/core_io/core_space/core_time/core_queue/core_sched/core_jobs/core_workers/core_wake/core_kernel/core_theme/core_font` adopted.
- `kit_runtime_diag` adopted for runtime perf diagnostics stage timing and input counter totals.
- `core_data/core_pack/core_trace` partial/additive.

Gaps:
- `Stabilize`: Slice 1 execution-core completion in tile-loader lane now integrated (`core_sched/core_jobs/core_kernel`) with additive behavior.
- `Stabilize`: Slice 2 execution-core queue migration complete in `app_tile_pipeline` Vulkan asset ready-handoff (shared `core_queue` with additive eviction/retry behavior retained).
- `Stabilize`: Slice 3 execution-core queue migration complete in `app_tile_pipeline` Vulkan polygon prep in/out handoff queues (shared `core_queue` with additive worker policy retained).
- `Stabilize`: Slice 4 diagnostics contract guard complete - deterministic `test_build_safety.sh` assertions now lock required `meta.dataset.json` schema/table keys, and deterministic `map_trace_contract_test` locks shared trace pack chunk presence (`TRHD/TRSM/TREV`) plus canonical runtime lane/marker vocabulary (including `trace_start/trace_end` lifecycle markers).
- `Stabilize`: Slice 5 trace pack parity guard complete - deterministic `map_trace_contract_test` now locks exact shared `core_pack` chunk count/order (`TRHD` -> `TRSM` -> `TREV`) and deterministic payload sizes for sample/marker chunks.
- `Stabilize`: runtime diagnostics math/counter consolidation now uses shared `kit_runtime_diag` helpers; app-specific routing/render semantics remain local.
- `Partial`: first bounded `core_viewport2d` camera bridge is now in place for cursor-anchor zoom and drag-pan math; keep Mercator projection, hot `screen<->world` render transforms, region-fit policy, and smoothing semantics local while stabilizing parity coverage.
- `Partial`: consolidate map diagnostics into stronger `core_data` contracts and route optional diagnostics archives through `core_pack`.
- `Partial`: expand standardized trace-lane usage (`core_trace`) from tooling-level into clearer runtime diagnostics surfaces.

### `physics_sim`
Current shared profile:
- Strong on `core_base/core_io/core_pack/core_scene`, plus `kit_viz`, theme/font.
- `core_pane` is now adopted for editor-shell geometry in `PS4D-2B`, and live splitter hover/drag now adopts shared `kit_pane` through the vendored subtree host.
- first `kit_render` adoption is now in place for shared font policy resolution plus one shared Vulkan text runtime path through the app-local font bridge.
- `core_sim >= 0.2.0` is now partially adopted for scene-level runtime stepping and the 3D solver first-pass shell: `SceneState` owns persistent loop state, the frame substep loop runs through seven ordered simulation passes, the scaffold backend owns nested solver loop state, and frame outcome diagnostics are test-covered.
- the host now consumes those shared modules through a vendored `third_party/codework_shared` subtree instead of direct workspace-local `../shared` linkage.
- `core_data` and `core_trace` partial.

Gaps:
- `Stabilize`: first scene-level `core_sim` pass-network slice is now complete; keep fluid equations, mode hook bodies, emitter/backend/object operations, scene time semantics, and HUD/render payloads app-local while shared `core_sim` owns pass ordering, tick/frame outcome shape, pause sync, and exact substep-count execution.
- `Stabilize`: first shared pane-resize slice is now complete; keep pane purpose, viewport behavior, and editor semantics app-local while shared `core_pane >= 0.2.0` owns split solve and shared `kit_pane >= 0.2.0` owns splitter hover/drag interaction state.
- `Partial`: complete PhysicsSim visual parity validation and trim the remaining bridge-only wrappers now that the cache/runtime layer itself lives in shared `kit_render`.
- `Partial`: deepen `core_data` model breadth beyond current export tables into broader sim-domain datasets.
- `Partial`: further align `core_pack` payload semantics with canonical `core_data` schema.
- `Partial`: standardize `core_trace` lanes/contracts beyond tooling-centric usage.

### `behavior_sim`
Current shared profile:
- `core_font` adopted for shell text through the vendored shared subtree.
- `core_pane` adopted for top-level shell geometry in `UI-S3`.
- `core_theme`, `kit_render`, and `kit_pane` are now also adopted for shared splitter hover/drag interaction and shared shell color/text policy while active SDL draw ownership stays local.
- `core_sim >= 0.2.0` adopted for persistent `CoreSimLoopState` ownership and behavior-preserving ordered stub-pass execution through a 30ms simulation shell.
- the host now participates in the managed shared-subtree manifest, and the vendored snapshot is refreshed to the current committed shared baseline.
- sim-domain world/entities/systems behavior remains app-local by design.

Gaps:
- `Stabilize`: subtree-host formalization is complete; keep future shared updates flowing through `bin/update_shared_subtrees.sh` instead of manual vendored drift.
- `Stabilize`: the host now defaults to the shared `ide` font baseline through `BEHAVIOR_SIM_FONT_PRESET`; keep launcher/package expectations aligned with that default.
- `Stabilize`: first shared pane-resize slice is now complete; keep pane-host ownership at the shell-geometry layer and future resize persistence/layout-authoring work additive on top of the shared `core_pane >= 0.2.0` + `kit_pane >= 0.2.0` seam instead of reopening app-local rect math in `UI-S4` or `BS-P7`.
- `Stabilize`: persistent `core_sim` behavior host slice is complete; keep window wait policy, world/entity storage, metrics meaning, and domain policies app-local while shared `core_sim` owns fixed-step loop state, single-step consumption, frame outcomes, and ordered pass-routing shell.
- `Missing`: evaluate broader `kit_ui` adoption only if later shell chrome and interaction semantics justify a fuller shared presentation host.
- `Missing`: broader runtime/domain shared-lib promotion beyond pass routing should stay deferred until the local sim model proves stable enough to generalize.

### `drawing_program`
Current shared profile:
- `core_base`, `core_pack`, `core_theme`, and `core_font` are adopted.
- the host now defaults to a vendored `third_party/codework_shared` subtree for build/package/shared-font asset resolution instead of live `../shared` wiring.
- the active SDL text/runtime lane is centralized in app-local facade files and already defaults to the shared `ide` font baseline.
- pane-shell geometry and layout transaction state also adopt shared `core_pane`, `core_layout`, and `core_pane_module`, and live splitter hover/drag now adopts shared `kit_pane` through the vendored subtree host.
- `WA1-S2` authoring entry/mode state plus minimal chrome now adopts vendored `kit_workspace_authoring` for shared entry-chord and reserved-trigger mapping while the app-local frame chrome derives pane/module readout from shared pane-module bindings; Apply/Cancel, module swapping, and persistence remain future WA1 slices.

Gaps:
- `Stabilize`: mixed vendored/live host cleanup is complete; keep future shared updates flowing through `bin/update_shared_subtrees.sh` instead of reopening workspace-linked defaults except for bounded local debugging.
- `Stabilize`: packaged font assets and runtime font-path fallback now resolve through vendored/shared packaged locations first; keep launcher/package expectations aligned with that contract.
- `Stabilize`: first shared pane-resize slice is now complete; keep pane meaning and panel/canvas semantics app-local while shared `core_pane >= 0.3.0` owns split solve plus cached splitter-hit enumeration and shared `kit_pane >= 0.3.0` owns splitter hover/drag interaction state. `drawing_program` is now the first proving host routing hover/begin-drag through the cached-hit registry before any IDE cutover.
- `Stabilize`: startup/session persistence failures observed during this rollout were traced to stale incremental objects after shared header churn, not to invalid `core_pane` / `kit_pane` pack contracts. Future hosts that adopt pane/session struct changes should either run an explicit clean rebuild before diagnosis or keep Makefile/header dependency coverage in place so saved pack debugging is not polluted by mixed-object binaries.
- `Partial`: first Workspace Authoring `WA1-S2` host attach is in place; next stabilize step is draft Apply/Cancel before persistence, module-content swapping, or second-host rollout.
- `Missing`: evaluate shared `kit_render` adoption only if a future visual/runtime pass shows clear value beyond the existing centralized SDL text lane.
- `Missing`: broader shared-core promotion (`core_io`, `core_data`, `core_trace`, execution core) should stay deferred until a concrete app lane needs it.

### `ray_tracing`
Current shared profile:
- Broad shared adoption: `core_base/core_io/core_scene/core_space/core_time`, theme/font, `kit_viz`.
- `core_data/core_pack/core_trace` are partly additive/tooling-oriented.
- `core_scene_compile` pre-`TP-S3` baseline wiring is now in place for authoring->runtime handoff preflight.
- first `kit_render` adoption slice is now in place for the font migration: Makefile wiring, shared role/tier/render-scale bridge policy, active helper/menu/timer-HUD UTF-8 measure/draw runtime, and wrapped helper labels now route through the shared external text path.
- the scene editor pane shell now also adopts shared `core_pane` for pane solve and shared `kit_pane` for splitter hover/drag interaction, while pane meaning and editor routing remain app-local.
- the host now consumes that shared surface through a vendored `third_party/codework_shared` subtree instead of direct workspace-local `../shared` linkage.
- `core_sim >= 0.2.0` is now partially adopted for runtime-frame control-plane routing, and the app now consumes that simulation surface through the vendored subtree host instead of a direct live `../shared` `CORE_SIM_DIR` reference.

Gaps:
- `Partial`: raise `core_data` usage from render-metrics export slice into broader analyzable runtime datasets.
- `Partial`: lock `core_pack` export/import schemas around that data model for cross-app reuse.
- `Partial`: promote `core_trace` from tooling-first to clearer runtime contract lanes where useful.
- `Stabilize`: Slice 1 complete (io hardening) - `fluid_import` now uses shared `core_io` for file-exists and manifest file reads in low-risk paths.
- `Stabilize`: Slice 2 complete (data contract hardening) - render metrics dataset now includes additive `schema_family`/`schema_variant` metadata with test coverage.
- `Stabilize`: Slice 3 complete (io cleanup) - shared theme preset persistence now uses shared `core_io` helpers in `ui/shared_theme_font_adapter`.
- `Stabilize`: trace tooling source lane restored - `ray_trace_tool` now builds and `manifest_to_trace` now exports valid trace packs through shared `core_trace`.
- `Stabilize`: deterministic trace contract smoke assertions are now in place (`make -C ray_tracing test-manifest-to-trace-export`) for canonical lanes/markers.
- `Stabilize`: deterministic `core_pack` parity guard is now in place (`make -C ray_tracing test-fluid-pack-contract-parity`) for `VFHD/DENS/VELX/VELY` import contract parity.
- `Stabilize`: pre-`TP-S3` runtime-scene preflight lane is in place (`import/runtime_scene_bridge`) with contract tests against trio fixtures (`scene_runtime_v1` accept, authoring-variant reject).
- `Stabilize`: RayTracing font-runtime bridge + default-baseline polish are complete; active text draw owners and wrapped labels now use shared `kit_render`, the host defaults to the shared `ide` font baseline, the old local `text_font_quality` helper is retired, and build/package/test/doc paths now resolve through vendored `third_party/codework_shared`. Remaining work is limited to optional thin-wrapper cleanup and visual tuning.
- `Stabilize`: first shared pane-resize slice is now complete; the scene editor uses SDL-resizable windows plus shared `core_pane >= 0.2.0` and `kit_pane >= 0.2.0` for left/center/right pane solve and live splitter hover/drag, the menu host is now SDL-resizable with runtime-sized layout rebuilds, and simulation runtime windows remain config-sized/fixed while chrome semantics, viewport routing, and pane meaning remain app-local.
- `Stabilize`: vendored shared simulation cutover is complete; `CORE_SIM_DIR` now resolves through `third_party/codework_shared` and the subtree snapshot is refreshed to the current committed shared baseline.
- `Stabilize`: native `3D` tile preview now relies on the additive shared `vk_renderer` in-place texture subrect update seam (`shared/vk_renderer >= 1.1.0`) instead of recreating a fresh full-frame preview texture for each visible tile step.
- `Missing`: execution-core adoption beyond `core_time` if worker/job/scheduler behavior should be standardized with IDE/MapForge.

### `line_drawing`
Current shared profile:
- Broad shared adoption: `core_base/core_scene/core_trace/core_math/core_time`, theme/font, vendored shape/timer HUD dependencies.
- the host now consumes that shared surface through a vendored `third_party/codework_shared` subtree instead of direct workspace-local `../shared` linkage.
- first `kit_render` adoption slice is now in place for the font migration: Makefile wiring, shared role/tier/zoom bridge policy, active Vulkan UTF-8 draw/measure runtime, and packaged launcher/runtime default alignment to the shared `ide` font baseline.
- first pane-host interaction slice is now in place for layout resizing: shared `core_pane` owns pane solve and shared `kit_pane` owns splitter hover/drag state while pane purpose stays app-local.

Gaps:
- `Stabilize`: first font-runtime unification slice is complete; active Vulkan text and the former scattered fallback UI text paths now route through the centralized bridge/helper layer over shared `kit_render`, while remaining drift is bounded to centralized non-Vulkan fallback behavior and thin local fallback font-path ownership.
- `Stabilize`: first shared pane-resize slice is complete; keep future resizing/persistence/layout-authoring work additive on top of the shared `core_pane` + `kit_pane` seam instead of reopening app-local splitter math.
- `Missing`: decide later whether the centralized non-Vulkan fallback in `text_draw.c` should also move fully into shared runtime, or whether it should remain intentionally local because the Vulkan path is the authoritative host mode.
- `Missing`: execution-core adoption beyond `core_time` only if future loop/dispatch behavior warrants standardization.

### `workspace_sandbox`
Current shared profile:
- Active UI text already renders through shared `kit_render`.
- Runtime now adopts shared `core_action`, `core_layout`, `core_config`, `core_pane`, `core_pane_module`, `core_pane_snapshot`, and `kit_workspace_authoring`.
- build/package/test/doc paths now resolve through a vendored `third_party/codework_shared` subtree instead of direct workspace-local `../shared` linkage.

Gaps:
- `Stabilize`: subtree-host conversion is complete and verified; keep future shared updates flowing through `bin/update_shared_subtrees.sh` instead of reopening live-path coupling.
- `Stabilize`: font default normalization is now complete for the active shared-kit lane: lifecycle boot, invalid saved-font fallback, font-theme panel selection, and packaged launcher defaults now align to the shared `ide` font baseline.
- `Stabilize`: stale copied foreign font/theme env wiring has been removed from the packaged launcher in favor of WorkspaceSandbox-specific env names.
- `Missing`: no new text-runtime migration is needed unless a future visual pass proves a real host-specific issue, because the active text path is already on shared `kit_render`.

## Cross-System Priority Order (next)
1. Complete `core_data` + `core_pack` consolidation in `map_forge`, `ray_tracing`, `physics_sim` (DAW is now stabilize-only for this lane).
2. Expand standardized runtime `core_trace` lanes in `map_forge` and `physics_sim` where tooling-first usage still dominates.
3. Evaluate high-value execution-core adoption candidates in `ray_tracing` (`queue/sched/jobs/workers/wake/kernel`) beyond `core_time`.
4. Keep `line_drawing` in stabilize mode and only migrate additional IO/helpers or fallback text paths when they provide clear value beyond the now-shared active Vulkan runtime.
5. Keep `drawing_program` in stabilize mode and only revisit shared text-runtime extraction if the centralized SDL lane becomes a real maintenance problem.
6. Expand `fisiCs` shared-core usage only where it improves compiler/runtime clarity without disrupting shim-focused flows.

## Maintenance Rule
When any app materially changes shared-lib usage:
- Update this doc first (gap state + next steps).
- Update `11_version_compat_matrix.md` if minimum required versions changed.
- Update `SHARED_LIBS_CURRENT_STATE.md` if adoption level changed.
