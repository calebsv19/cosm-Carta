# Program Shared-Lib Connection Gaps

Last updated: 2026-06-06
Purpose: canonical per-program list of shared-lib connection gaps and next integrations.

Use this with:
- `../11_version_compat_matrix.md` for minimum supported versions.
- `../../SHARED_LIBS_CURRENT_STATE.md` for current adoption snapshot.

## Gap Legend
- `Missing`: not wired in build/runtime/tooling where it would provide clear value.
- `Partial`: wired, but only in additive or narrow paths; broader consolidation still open.
- `Stabilize`: wired broadly; remaining work is hardening/cleanup/documentation.

## Per-Program Gap List

### `ball_bounce_sim`
Current shared profile:
- `core_sim >= 0.4.0` is now directly adopted in the interactive SDL shell.
- The adoption is intentionally narrow: shared `core_sim` owns fixed-step
  control-plane state, pass dispatch, frame outcome status, and tick-count
  reporting for the `world_run_fixed_tick` pass.
- Ball Bounce still owns scenarios, units-annotated solver formulas, headless
  summaries/artifacts, worker-job parsing, worker-exchange packaging, SDL
  drawing, and `fisiCs` overlay proof behavior.
- The host links the live shared root through `../shared/core/core_sim` rather
  than a vendored subtree.

Gaps:
- `Stabilize`: keep the current `core_sim` adoption as a shell adapter only.
  Do not move solver equations, scenario meaning, worker contracts, or
  artifact schemas into shared code without a separate evidence-backed slice.
- `Missing`: `core_sim_trace` stays deferred unless a future Ball Bounce
  artifact/report lane needs standardized control-plane trace samples.
- `Missing`: broader execution-core adoption (`core_queue`, `core_sched`,
  `core_jobs`, `core_workers`, `core_wake`, `core_kernel`) stays deferred until
  Ball Bounce has real async/background work or cross-thread wake ownership.

### `video_editor`
Current shared profile:
- `core_base`, `core_pane`, `core_theme`, `core_font`, `kit_render`, and
  `kit_ui` are now linked from the live shared root by the desktop SDL shell.
- Shared `core_theme` owns palette tokens, shared `core_font` owns role specs,
  shared `kit_render` owns role/tier/text-zoom policy through a
  `KIT_RENDER_BACKEND_NULL` context, and shared `kit_ui` owns bounded toolbar
  button style resolution plus point-in-rect hit semantics for app-local shell
  routing.
- Active UTF-8 rasterization and SDL draw submission remain app-local through
  `src/ui/video_editor_ui_theme_text.c`.
- Shared `core_pane` owns split-pane solve and splitter-hitbox collection for
  Library width and Preview/Timeline resizing, while SDL drag lifecycle and
  `ui_settings.cfg` persistence remain app-local.
- Shared `core_queue`, `core_jobs`, and `core_workers` now back the first
  execution-core thumbnail lane: Video Editor schedules decoder thumbnail/sample
  work off the SDL thread, receives bounded worker completions through a shared
  queue, and applies those completions through a main-thread job budget.
- Shared `core_sched`, `core_wake`, and `core_kernel` now back the first outer
  SDL loop bridge: Video Editor registers an app-owned SDL wake event, preserves
  user events consumed during wait, uses a shared scheduler timer for frame
  cadence, exposes the wake object to worker completions, and ticks the shared
  kernel instead of sleeping through fixed `SDL_Delay(16)` polling.
- Shared font assets are bundled into `VideoEditor.app`.

Gaps:
- `Stabilize`: keep the current text/theme bridge honest about its boundary:
  shared policy is adopted, but SDL drawing, text texture creation, demo-shell
  layout tuning, and app-specific palette mapping remain Video Editor-owned.
- `Stabilize`: keep future pane work additive on top of the adopted
  `core_pane >= 0.3.1` seam. Do not reintroduce app-local split math for pane
  solve or splitter hitbox collection.
- `Missing`: shared retained/Vulkan renderer adoption stays deferred until
  there is a real video preview/render backend seam. Do not route the plain SDL
  shell through `kit_render_external_text.*` because that helper currently
  assumes the shared Vulkan renderer runtime.
- `Missing`: `kit_pane`, `kit_workspace_authoring`, `core_action`, and broader
  input-control adoption stay deferred until pane interaction presentation,
  authoring overlays, and command routing need shared ownership.
- `Partial`: execution-core adoption now covers the first thumbnail/sample
  worker lane and the first outer SDL loop wait/wake bridge. Broaden this to
  priority scheduling, preview/proxy/export job classes, and diagnostics only
  after the thumbnail and loop lanes stay stable under real media.

### `datalab`
Current shared profile:
- Strong on `core_base/core_io/core_data/core_pack`, `core_font`, and `kit_viz`.
- `WASR-S3` now adopts the shared `kit_workspace_authoring >= 0.5.0` font/theme surface with the required `kit_render`/`core_theme` dependency chain.
- `kit_ui >= 0.11.1` now owns the bottom playback HUD and top-left session
  HUD alpha-aware floating style, button/readout row layout, nested
  corner/inset sizing, and optional SDL rounded panel/button/readout adapter
  while DataLab keeps playback/action policy, session content, file stepping,
  manual edge-wrap navigation, active theme persistence, and theme/custom-token
  mapping local.
- First incremental `kit_graph_timeseries` adoption is now in place (shared stride guidance for trace decimation).

Gaps:
- `Stabilize`: Workspace Authoring `WASR-S3` is complete; DataLab now uses the shared font/theme authoring layout, hit IDs, labels, preset mappings, and button-to-action classification while SDL drawing, custom theme editor state, accepted mutation, and persistence remain host-owned.
- `Stabilize`: the playback HUD and session data HUD are now the first direct
  DataLab `kit_ui` HUD-row/SDL-adapter adopters; reuse this adapter in one more
  SDL program before promoting broader app-agnostic action semantics, and only
  then broaden rounded-surface polish to picker/session panels.
- `Stabilize`: `core_viewport2d` proving-host adoption is now in place for sketch/image lanes; keep viewport persistence, fit-reset behavior, and future large-image/tiled follow-ons aligned to the shared math contract instead of regressing into app-local viewport drift.
- `Partial`: continue `kit_graph_timeseries` migration (shared view math, hover inspection, then one shared draw path).
- `Missing`: `core_trace` trace-import/view path standardization.
- `Stabilize`: active DataLab runtime HUD colors now follow the persisted
  workspace-authoring theme/custom palette. Startup/reopen picker polish and
  broader non-HUD surface theming remain normal UI polish, not a core-theme
  adoption blocker.
- `Partial`: normalize profile loaders so all external data lanes map through one shared parsing contract.

### `daw`
Current shared profile:
- `core_base`, `core_io`, `core_time`, `core_queue`, `core_sched`, `core_jobs`, `core_wake`, `core_kernel`, `core_theme`, `core_font`, `kit_viz` adopted.
- `core_data` + `core_pack` mainly additive/diagnostics.
- `timer_hud` now uses the explicit session-owned host bootstrap/render/shutdown path, and packaged `soniCs.app` resolves a runtime-owned TimerHUD settings file through the launcher instead of relying on bundle-local config writes.

Gaps:
- `Stabilize`: no mandatory shared-lib gap remains for the current DAW rollout plan.
- `Stabilize`: centralized UI font lane now partially adopts vendored `kit_render_external_text.*` for active Vulkan draw/measure while preserving the `daw_default` preset; the only remaining local seam is clipped draw because the shared external text runtime still lacks source-rect crop support.
- `Stabilize`: Workspace Authoring `DWA1-S0/S1/S2/S3/S4/S5` is complete. DAW now has a thin app-local authoring host around shared `kit_workspace_authoring` entry-chord semantics, routes `Alt+C` then `Alt+V` before normal input, captures runtime input only while authoring is active, supports `Tab`/`Enter`/`Esc` for overlay-cycle/apply/cancel state transitions, draws active pane inventory labels over transport/timeline/inspector/library using shared overlay button geometry, and uses the shared full-screen Font/Theme layout, labels, enabled checks, hit testing, preset mapping, and button-to-action classification for live text-size/font/theme preview. Apply persists accepted theme/font/text-size drafts through DAW-owned preference lanes; Cancel, toggle-off, and shutdown restore/cancel active drafts without saving. The managed DAW subtree refresh brought the committed authoring kit to `0.5.0`; the default vendored build remains blocked by the separate TimerHUD session/snapshot subtree follow-up while live-shared verification passes with `SHARED_ROOT=../shared`. `gravity_orbit_sim` is queued as the next recommended host because its shared pane/theme/font/render-policy seams are already in place and its active SDL renderer remains a useful host-agnostic proving boundary.
- `Stabilize`: Slice 3 complete (data contract hardening) - DAW dataset metadata now includes additive `schema_family`/`schema_variant` keys and deterministic contract coverage for canonical `daw_selection_v1` table.
- `Stabilize`: Slice 2 complete (pack/data contract parity guard) - deterministic `daw_pack_contract_parity_test` now verifies `DAWH/WMIN/WMAX/MRKS/JSON` chunk presence plus canonical `core_dataset` schema keys (`daw_timeline_v1`, `dataset_schema`, `dataset_contract_version`).
- `Stabilize`: Slice 4 complete (trace diagnostics lane) - deterministic `daw_trace_export_contract_test` now verifies canonical transport/scheduler timing lanes and `trace_start/trace_end` markers exported through shared `core_trace`.
- `Stabilize`: Slice 5 complete (workers lane adoption) - async diagnostics trace export now uses shared `core_workers` with deterministic completion/contract coverage (`daw_trace_export_async_contract_test`), while preserving runtime-loop behavior.

### `dungeon`
Current shared profile:
- `core_time` adopted through the vendored shared subtree for SR2 runtime frame
  timing.
- `core_sim` adopted through the vendored shared subtree for SR3 fixed-step pass
  routing and frame outcome diagnostics.
- `core_io` adopted through the vendored shared subtree for save/session/config
  file existence checks and whole-file reads/writes.
- `core_base`, `core_theme`, `core_font`, and `kit_render` adopted through the
  vendored shared subtree for SR4 render/text policy and null-backend command
  recording.
- the host now participates in the managed shared-subtree manifest, and
  build/package/shared-font paths default to `third_party/codework_shared`
  instead of direct workspace-local `../shared` linkage.
- Dungeon gameplay rules, renderer policy, input action identity, and SDL event
  ownership remain app-local.

Gaps:
- `Stabilize`: subtree-host conversion is complete; keep future shared updates
  flowing through `bin/update_shared_subtrees.sh` after the current Dungeon
  worktree is committed instead of reopening live-path defaults.
- `Stabilize`: SR2 timing/wake slice is in place. Keep `core_time` as the shared
  owner for monotonic timestamps while Dungeon owns dirty reasons, SDL waits,
  and gameplay update policy.
- `Stabilize`: SR3 pass-routing slice is in place. Keep `core_sim` as the shared
  owner for fixed-step accumulator state, pass order, max-tick clamp, and frame
  outcomes while Dungeon owns every domain mutation inside app-local callbacks.
- `Stabilize`: SR4 render/text bridge is in place. Keep `core_theme`,
  `core_font`, and `kit_render` as shared policy/command-recording support while
  Dungeon owns HUD layout/data, copied command text storage, and active SDL
  bitmap submission.
- `Stabilize`: first persistence slice is in place. Keep `core_io` as the shared
  byte/file helper while Dungeon owns runtime-root policy, recursive directory
  creation, schema parsing, migrations, and gameplay save semantics.
- `Stabilize`: the preset-editor shell now adopts shared `core_pane` for
  top-level left/right/context geometry solve while pane semantics, room
  meaning, viewport behavior, and all scenario-authoring policy remain
  app-local.
- `Missing`: `core_action` stays deferred until app-local IR1 action IDs need
  shared trigger binding.
- `Missing`: `vk_renderer` stays deferred until command submission has a clean
  active backend seam.
- `Missing`: broader execution core (`core_queue`, `core_sched`, `core_jobs`,
  `core_workers`, `core_wake`, `core_kernel`) stays deferred until Dungeon has
  async/background work or cross-thread wake ownership.

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
- `core_pane` is now adopted for pane-shell geometry, and live splitter
  hover/drag now adopts shared `kit_pane`.
- `core_io` is now adopted for close/reopen session-state file reads/writes.
- `core_theme` and `core_font` are now adopted for shell palette/font defaults.
- `core_sim` is now adopted as the first proving-host lane for fixed-step
  playback/single-step shell orchestration.
- `core_sim_trace >= 0.1.0` is now adopted by the headless artifact path for
  reusable control-plane trace sample/marker emission.
- `core_viewport2d` is now adopted for cursor-anchor zoom, drag-pan, and
  fit-reset camera math through an app-local world-meter bridge.
- first `kit_render` adoption is now in place for shared role/tier text sizing
  and text zoom policy through a `KIT_RENDER_BACKEND_NULL` context.
- `kit_ui >= 0.9.1` is now adopted for the bounded shared button
  spec/state/style lane through an app-local SDL wrapper.
- `timer_hud` is now adopted through a thin app-local session adapter with a
  runtime-owned settings path, HUD-off-by-default packaged launcher policy, and
  only two initial scopes: `Gravity Tick` and `Render Frame`.
- `kit_workspace_authoring >= 0.5.0` is now wired for the first
  Workspace Authoring host shell: shared `Alt+C` then `Alt+V` entry/toggle
  handling routes before normal input, active authoring captures reserved
  runtime input, and `Tab`/`Enter`/`Esc` own overlay-cycle/apply/cancel
  semantics while active.
- the host now participates in the managed shared-subtree manifest, and
  build/package/shared-font paths resolve through vendored
  `third_party/codework_shared` instead of direct workspace-local `../shared`
  linkage.
- UTF-8 draw runtime ownership and simulation-body colors remain app-local by
  design.

Gaps:
- `Stabilize`: first shared pane-resize slice is now complete; keep pane
  meaning, menu-button behavior, viewport transforms, and simulation semantics
  app-local while shared `core_pane >= 0.2.0` owns split solve and shared
  `kit_pane >= 0.2.0` owns splitter hover/drag interaction state.
- `Stabilize`: first `core_sim` proving-host slice is now complete; keep
  orbit bodies, gravity force accumulation, integration equations, rendering,
  and scenario persistence app-local while shared `core_sim >= 0.4.0` owns the
  fixed-step accumulator, pause/play/single-step, max-tick clamp, and frame
  outcome/frame-record contract.
- `Stabilize`: first `core_sim_trace` host adoption is complete in the
  headless artifact path; keep domain snapshots and orbit-specific analysis
  app-owned while shared `core_sim_trace >= 0.1.0` owns standard
  `core_sim.*` trace lanes and frame/reason markers.
- `Stabilize`: subtree-host conversion is complete and verified; keep future
  shared updates flowing through `bin/update_shared_subtrees.sh` instead of
  reopening workspace-linked defaults.
- `Stabilize`: keep the current text bridge honest about its boundary:
  `core_theme` / `core_font` / `kit_render` own policy, while active SDL host
  draw/runtime stays local unless the host later moves onto a shared renderer
  backend that can actually consume `kit_render_external_text.*`.
- `Stabilize`: first `kit_ui` adoption is now code-backed and intentionally
  narrow. Keep SDL drawing, palette tuning, interaction routing, and
  app-specific button placement local while shared `kit_ui >= 0.9.1` owns the
  reusable button spec/state/style semantics.
- `Stabilize`: first shared `core_viewport2d` camera slice is now complete;
  keep world-meter bridge policy, viewport input routing, edit-handle hit
  semantics, and far-body despawn behavior app-local while shared
  `core_viewport2d >= 0.1.0` owns generic pan/zoom/fit math.
- `Partial`: first shared `core_headless_job` protocol adoption is now in
  place at the wrapper layer only; the visual-review bundle adapter proves the
  outer `codework_job / headless_bundle_v1` plus shared report surface around
  the current orbit/body request family, but detached worker-style run roots,
  status polling, and VPS package wiring remain future work.
- `Partial`: first TimerHUD adoption is now in place and intentionally narrow;
  keep scope count small, keep the SDL text/render bridge app-local, and avoid
  broad instrumentation churn unless Gravity Orbit Sim becomes an active
  performance tuning host.
- `Stabilize`: `GOWA1-S1/S2/S3/S4/S5` Workspace Authoring entry host shell,
  active-only pane overlay, full-screen Font/Theme overlay, accepted-only
  persistence, and closeout are complete after user visual acceptance. Cosmos
  uses shared `kit_workspace_authoring >= 0.5.0` entry chord,
  reserved-trigger semantics, overlay button geometry, Font/Theme layout,
  labels, enabled-state checks, hit testing, preset mapping, and action
  classification. Cosmos owns SDL drawing, app-specific pane/module inventory
  labels, live text-size/font/theme preview mutation through
  `GravityOrbitSimUiThemeText`, and session-state persistence for accepted
  theme/font/text-size state. Apply saves through the existing
  `last_session.gosimstate` lane; Cancel, toggle-off, and shutdown restore or
  cancel active previews without saving drafts. `mem_console` is the next
  recommended host because it already has theme/font preference persistence,
  `core_pane` split-pane evaluation, and a useful operational UI surface. Live
  shared-root verification passes; default vendored builds still need the
  separate TimerHUD session/snapshot subtree refresh because the app TimerHUD
  adapter expects the newer live shared session API.
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

### `growth_sim`
Current shared profile:
- `core_sim` is adopted through the vendored subtree host for Mold and Fire
  pass execution.
- `core_sim_trace >= 0.1.1` over `core_trace >= 1.0.0` is adopted for the
  deterministic control-plane trace diagnostics route.
- `core_data >= 1.0.0` is adopted for copied typed Mold and Fire field
  snapshots.
- `core_pack >= 1.1.1` is adopted for producer-side Mold and Fire field-frame
  export and validation.
- `core_pane` / `kit_pane` are adopted for the pane-backed editor shell.
- `core_theme`, `core_font`, and `kit_render` are adopted for visual text and
  theme policy.
- `kit_ui >= 0.9.0` is now adopted for the richer selected/pressed/focused
  button semantic contract proven first through the FireSim shell chrome.
- `core_io` remains an indirect support dependency through shared trace/pack
  lanes rather than a first-class Growth-owned file API.

Gaps:
- `Stabilize`: keep Phase 18 trace adoption bounded to shared `core_sim.*`
  control-plane lanes while mold occupancy/nutrient/decay metrics and launcher
  handoff markers stay app-owned.
- `Stabilize`: copied `core_data` snapshots now cover both Mold and Fire scalar
  fields while live solver storage and domain semantics remain app-local.
- `Stabilize`: producer-side `core_pack` export is now in place for Mold and
  Fire field-frame bundles; keep chunk meaning, schema evolution, export-root
  policy, and consumer handoff semantics app-owned until a second real consumer
  stabilizes the interchange contract.
- `Partial`: first shared `core_headless_job` protocol adoption is now in
  place at the wrapper layer only; the visual-review bundle adapter proves the
  outer `codework_job / headless_bundle_v1` plus shared report surface around
  the current field/grid review request family, but detached worker-style run
  roots, status polling, and VPS package wiring remain future work.
- `Missing`: shared `sim_growth` extraction remains deferred until a second real
  Growth ruleset proves repeated solver/domain shape.
- `Partial`: Growth/Kinetic interop remains unimplemented even though durable
  field-frame bundles now exist; the next consumer-side work should start from
  the exported bundle contract instead of inventing a second transport.
- `Stabilize`: first shared `kit_ui` button-semantics adoption is in place;
  keep app-specific palette tuning, SDL launcher drawing, and editor behavior
  policy app-local while shared `kit_ui >= 0.9.0` owns the reusable
  selected/pressed/focused/variant-aware button contract.

### `ide`
Current shared profile:
- Full execution-core stack adopted (`time/queue/sched/jobs/workers/wake/kernel`).
- `core_base/core_io/core_data/core_pack` and `core_theme/core_font` are adopted.
- `kit_workspace_authoring >= 0.5.0` is adopted for the first Workspace
  Authoring host lane.
- `kit_graph_struct >= 0.8.1` is adopted for the Libraries-panel include
  dependency graph layout and node hit testing over the IDE-local
  `include_graph` snapshot.
- `core_viewport2d >= 0.2.1` is adopted for Libraries-panel graph camera state,
  cursor-anchor wheel zoom, and drag-pan math.

Gaps:
- `Partial`: complete migration of remaining ad-hoc file/diagnostics paths into shared `core_io`/`core_data` patterns.
- `Partial`: move from export-first `core_pack` diagnostics into broader standardized snapshot/restore contracts (if needed).
- `Stabilize`: continue execution-core hardening tests (idle policies, wake behavior, shutdown boundaries).
- `Stabilize`: Timer HUD text now routes through the central IDE text helpers, and `ide` is now the first explicit `TimerHUDSession` adopter instead of relying on the default global shim; any further cleanup should stay bounded and avoid reopening the main reference text stack unless there is clear cross-host value.
- `Stabilize`: Workspace Authoring `IDEWA1-S0/S1/S2/S3/S4/S5` is complete:
  shared `kit_workspace_authoring` owns the entry/toggle chord,
  active-authoring reserved input capture, pane overlay button geometry/hit
  testing, and full-screen Font/Theme layout/hit/action semantics. IDE owns
  app-local drawing, pane labels, live theme/font/text-size preview mutation,
  and accepted-only preference persistence. Normal runtime remains free of
  authoring HUD/reminder text. Future work should start a new plan for real
  pane/module mutation rather than extending the host-attach lane.
- `Stabilize`: first `kit_graph_struct` adoption is in place for the
  Libraries-panel `Graph` view, with `core_viewport2d` now owning graph camera
  math. Keep compiler include-graph storage, source/header semantics, SDL
  drawing, graph HUD, collapse policy, and package/build execution behavior
  IDE-owned while shared modules own generic layout, hit-test, and viewport
  camera math.

### `line_drawing`
Current shared profile:
- `core_base`, `core_scene`, `core_math`, `core_time`, `core_theme`, `core_font` adopted.
- `core_io/core_data/core_pack/core_trace` are additive/partial.
- pane-shell geometry now also adopts shared `core_pane`, and live splitter hover/drag now adopts shared `kit_pane` through the vendored subtree host.
- Workspace Authoring S1/S2/S3/S4 now adopt `kit_workspace_authoring >= 0.5.0` for the shared entry chord, reserved authoring trigger classification, shared overlay button layout/hit testing, and shared full-screen font/theme panel layout/hit/action semantics; pane readout, SDL/Vulkan drawing, and accepted-only runtime preference persistence remain app-local over `LineDrawingPaneHost`.

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
- `Stabilize`: `LDWA1-S1/S2/S3/S4/S5` authoring host, active-only pane overlay, shared font/theme overlay adoption, accepted-only font/theme preference persistence, and closeout are complete through shared `kit_workspace_authoring >= 0.5.0`; module content placement remains deferred. Next recommended authoring host is `physics_sim`.
- `Missing`: execution-core adoption beyond `core_time` where background/task orchestration appears.

### `map_forge`
Current shared profile:
- `core_base/core_io/core_space/core_time/core_queue/core_sched/core_jobs/core_workers/core_wake/core_kernel/core_theme/core_font` adopted.
- `core_pane` is now adopted for the first left-pane + constrained-viewport shell under the header.
- `kit_runtime_diag` adopted for runtime perf diagnostics stage timing and input counter totals.
- `core_data/core_pack/core_trace` partial/additive.

Gaps:
- `Stabilize`: Slice 1 execution-core completion in tile-loader lane now integrated (`core_sched/core_jobs/core_kernel`) with additive behavior.
- `Stabilize`: Slice 2 execution-core queue migration complete in `app_tile_pipeline` Vulkan asset ready-handoff (shared `core_queue` with additive eviction/retry behavior retained).
- `Stabilize`: Slice 3 execution-core queue migration complete in `app_tile_pipeline` Vulkan polygon prep in/out handoff queues (shared `core_queue` with additive worker policy retained).
- `Stabilize`: Slice 4 diagnostics contract guard complete - deterministic `test_build_safety.sh` assertions now lock required `meta.dataset.json` schema/table keys, and deterministic `map_trace_contract_test` locks shared trace pack chunk presence (`TRHD/TRSM/TREV`) plus canonical runtime lane/marker vocabulary (including `trace_start/trace_end` lifecycle markers).
- `Stabilize`: Slice 5 trace pack parity guard complete - deterministic `map_trace_contract_test` now locks exact shared `core_pack` chunk count/order (`TRHD` -> `TRSM` -> `TREV`) and deterministic payload sizes for sample/marker chunks.
- `Stabilize`: runtime diagnostics math/counter consolidation now uses shared `kit_runtime_diag` helpers; app-specific routing/render semantics remain local.
- `Partial`: first bounded `core_pane` runtime-shell adoption is now in place for the pin workflow lane: the top header now owns a `PINS` toggle, shared `core_pane` solves the left-pane + viewport split, and app-local pane chrome plus pane meaning remain local while splitter interaction and broader pane-host consolidation stay deferred.
- `Partial`: first bounded `core_viewport2d` camera bridge is now in place for cursor-anchor zoom and drag-pan math; keep Mercator projection, hot `screen<->world` render transforms, region-fit policy, and smoothing semantics local while stabilizing parity coverage.
- `Partial`: Workspace Authoring `MFWA1-S0/S1/S2/S3/S4` baseline, host-entry, active pane/surface overlay, shared Font/Theme overlay, and accepted-only persistence slices are complete. Vendored `kit_workspace_authoring` is refreshed to `0.5.0`, Carta routes the shared `Alt+C` then `Alt+V` entry/toggle chord before normal map input while keeping normal runtime free of authoring HUD/reminder text, active pane/surface mode uses shared overlay button geometry/hit testing with app-local surface inventory drawing over the live map shell, and Font/Theme mode uses the shared full-screen layout, hit testing, labels, enabled checks, preset mappings, and action classification while MapForge owns SDL drawing plus live theme/font/text-size preview mutation. Apply persists accepted drafts through app-owned preference lanes; Cancel/toggle-off/shutdown restore the entry baseline. Next is closeout and next-host selection.
- `Partial`: consolidate map diagnostics into stronger `core_data` contracts and route optional diagnostics archives through `core_pack`.
- `Partial`: expand standardized trace-lane usage (`core_trace`) from tooling-level into clearer runtime diagnostics surfaces.

### `physics_sim`
Current shared profile:
- Strong on `core_base/core_io/core_pack/core_scene`, plus `kit_viz`, theme/font.
- `core_pane` is now adopted for editor-shell geometry in `PS4D-2B`, and live splitter hover/drag now adopts shared `kit_pane` through the vendored subtree host.
- `core_viewport2d >= 0.2.1` is now partially adopted for the retained-scene `2D` editor camera path: fit-reset, cursor-anchor zoom, drag-pan, and screen/content transforms now route through the shared viewport math while canvas selection, scene-world meaning, and `3D` orbit behavior remain app-local.
- first `kit_render` adoption is now in place for shared font policy resolution plus one shared Vulkan text runtime path through the app-local font bridge.
- first `kit_ui >= 0.9.1` adoption is now in place for bounded menu/editor
  button spec/state/style semantics through the app-local
  `physics_sim_ui_button` SDL wrapper.
- `core_sim >= 0.2.0` is now partially adopted for scene-level runtime stepping and the 3D solver first-pass shell: `SceneState` owns persistent loop state, the frame substep loop runs through seven ordered simulation passes, the scaffold backend owns nested solver loop state, and frame outcome diagnostics are test-covered.
- the host now consumes those shared modules through a vendored `third_party/codework_shared` subtree instead of direct workspace-local `../shared` linkage.
- `core_data` and `core_trace` partial.

Gaps:
- `Stabilize`: first scene-level `core_sim` pass-network slice is now complete; keep fluid equations, mode hook bodies, emitter/backend/object operations, scene time semantics, and HUD/render payloads app-local while shared `core_sim` owns pass ordering, tick/frame outcome shape, pause sync, and exact substep-count execution.
- `Stabilize`: first shared pane-resize slice is now complete; keep pane purpose, viewport behavior, and editor semantics app-local while shared `core_pane >= 0.2.0` owns split solve and shared `kit_pane >= 0.2.0` owns splitter hover/drag interaction state.
- `Stabilize`: the retained-scene `2D` editor viewport bridge is now complete; keep scene bounds authority, local canvas rect routing, `3D` orbit projection, and higher-level editor gesture policy app-local while shared `core_viewport2d` owns fit-reset, cursor-anchor zoom, drag-pan, and screen/content transform math.
- `Partial`: complete PhysicsSim visual parity validation and trim the remaining bridge-only wrappers now that the cache/runtime layer itself lives in shared `kit_render`.
- `Stabilize`: keep the current `kit_ui` lane bounded to shared button
  spec/state/style semantics while SDL drawing, palette tuning, and exact
  menu/editor placement remain app-local.
- `Stabilize`: Workspace Authoring `PSWA1-S1/S2/S3/S4/S5` is complete on the menu/editor shell; shared `kit_workspace_authoring >= 0.5.0` owns the entry/toggle chord, active reserved-trigger capture, active pane-overlay button geometry/hit testing, and full-screen Font/Theme layout/hit/action semantics while SDL drawing, pane/module labels, `SceneEditorPaneHost` geometry readout, runtime preview mutation, and accepted-only persistence remain app-local. Apply saves theme/font/text-size through ignored `data/runtime` state, Cancel/shutdown restore active previews without saving, and S5 closed as docs/status only with no additional app commit.
- `Partial`: deepen `core_data` model breadth beyond current export tables into broader sim-domain datasets.
- `Partial`: further align `core_pack` payload semantics with canonical `core_data` schema.
- `Partial`: standardize `core_trace` lanes/contracts beyond tooling-centric usage.

### `behavior_sim`
Current shared profile:
- `core_font` adopted for shell text through the vendored shared subtree.
- `core_pane` adopted for top-level shell geometry in `UI-S3`.
- `core_theme`, `kit_render`, `kit_ui`, and `kit_pane` are now also adopted for shared splitter hover/drag interaction, shared shell color/text policy, direct `KitRenderRect` authoring-overlay handoff types, and shared button spec/state/style semantics while active SDL draw ownership stays local.
- `core_sim >= 0.4.0` adopted for persistent `CoreSimLoopState` ownership, frame records, and behavior-preserving ordered stub-pass execution through a 30ms simulation shell.
- `core_sim_trace >= 0.1.0` adopted for shared headless control-plane trace sample/marker diagnostics beside app-owned behavior metrics.
- `kit_workspace_authoring >= 0.5.0` adopted for the first Workspace Authoring host-attach slices: shared `Alt+C`/`Alt+V` entry chord handling, active authoring reserved-trigger classification, shared overlay button layout/hit testing for active-only pane mode, and shared font/theme layout/hit/action classification for text-size preview.
- the host now participates in the managed shared-subtree manifest, and the vendored snapshot is refreshed to the current committed shared baseline.
- sim-domain world/entities/systems behavior remains app-local by design.

Gaps:
- `Stabilize`: subtree-host formalization is complete; keep future shared updates flowing through `bin/update_shared_subtrees.sh` instead of manual vendored drift.
- `Stabilize`: the host now defaults to the shared `ide` font baseline through `BEHAVIOR_SIM_FONT_PRESET`; keep launcher/package expectations aligned with that default.
- `Stabilize`: first shared pane-resize slice is now complete; keep pane-host ownership at the shell-geometry layer and future resize persistence/layout-authoring work additive on top of the shared `core_pane >= 0.2.0` + `kit_pane >= 0.2.0` seam instead of reopening app-local rect math in `UI-S4` or `BS-P7`.
- `Stabilize`: `BWA1-S2/S3/S4/S5` active-only pane overlay, shared font/theme overlay, accepted-only font-step persistence, and closeout are complete with app-local SDL drawing/state persistence and shared `kit_workspace_authoring` control geometry/hit/action semantics; `line_drawing` has now completed its follow-on host attach, and `physics_sim` is queued next.
- `Stabilize`: persistent `core_sim` behavior host slice is complete; keep window wait policy, world/entity storage, metrics meaning, and domain policies app-local while shared `core_sim` owns fixed-step loop state, single-step consumption, frame outcomes, frame records, and ordered pass-routing shell.
- `Stabilize`: `core_sim_trace` adoption is now complete for the headless control-plane summary; keep behavior-domain trace lanes and durable data/pack artifacts deferred until multiple hosts prove those schemas.
- `Partial`: Workspace Authoring `BWA1-S1` is complete as an invisible host state; next stabilization step is an active-only pane overlay before adopting the shared full-screen font/theme panel.
- `Stabilize`: `kit_ui` is now directly adopted for the bounded shell button semantics lane; keep app-local drawing, palette tuning, and interaction routing outside the shared button contract.
- `Missing`: broader runtime/domain shared-lib promotion beyond pass routing should stay deferred until the local sim model proves stable enough to generalize.

### `drawing_program`
Current shared profile:
- `core_base`, `core_pack`, `core_theme`, `core_font`, `core_scene`, and `core_viewport2d` are adopted.
- `core_object` is also directly adopted in the bounded texture-scene import lane for `CoreObjectVec3` parsing and plane-lock helpers while broader object/store semantics remain app-local.
- `core_authored_texture >= 0.1.1` is now partially adopted through the authored-texture export contract lane: manifest schema/version, binding/output/primitive vocabulary, and JSON-free contract validation helpers now come from the shared module while JSON writing and PNG export policy remain app-local.
- the host now defaults to a vendored `third_party/codework_shared` subtree for build/package/shared-font asset resolution instead of live `../shared` wiring.
- the active SDL text/runtime lane is centralized in app-local facade files and already defaults to the shared `ide` font baseline.
- pane-shell geometry and layout transaction state also adopt shared `core_pane`, `core_layout`, and `core_pane_module`, and live splitter hover/drag now adopts shared `kit_pane` through the vendored subtree host.
- panel button framing now adopts shared `kit_ui` button spec/state/style semantics while SDL drawing and palette policy stay app-local.
- `WA1-S4` plus `WASR-S4` now adopt vendored `kit_workspace_authoring >= 0.5.0` for shared entry-chord, reserved-trigger, overlay-cycle, font/theme layout, standard button hit testing, labels, enabled-state checks, preset mappings, and button-to-action classification, and the authoring chrome also directly consumes `kit_workspace_authoring_ui` helpers plus narrow `kit_render` geometry types (`KitRenderRect`) while the app-local frame chrome derives pane/module readout from shared pane-module bindings and keeps SDL drawing, accepted-only state mutation, snapshot persistence, and pane/module content host-owned; module swapping remains a future WA1 slice.

Gaps:
- `Stabilize`: mixed vendored/live host cleanup is complete; keep future shared updates flowing through `bin/update_shared_subtrees.sh` instead of reopening workspace-linked defaults except for bounded local debugging.
- `Stabilize`: packaged font assets and runtime font-path fallback now resolve through vendored/shared packaged locations first; keep launcher/package expectations aligned with that contract.
- `Stabilize`: first shared pane-resize slice is now complete; keep pane meaning and panel/canvas semantics app-local while shared `core_pane >= 0.3.0` owns split solve plus cached splitter-hit enumeration and shared `kit_pane >= 0.3.0` owns splitter hover/drag interaction state. `drawing_program` is now the first proving host routing hover/begin-drag through the cached-hit registry before any IDE cutover.
- `Stabilize`: startup/session persistence failures observed during this rollout were traced to stale incremental objects after shared header churn, not to invalid `core_pane` / `kit_pane` pack contracts. Future hosts that adopt pane/session struct changes should either run an explicit clean rebuild before diagnosis or keep Makefile/header dependency coverage in place so saved pack debugging is not polluted by mixed-object binaries.
- `Partial`: first `core_authored_texture` cutover is now in place for authored-texture export semantics, but it is intentionally bridge-first. `drawing_program` still defaults to the vendored subtree host and uses a bounded workspace-shared fallback until the next clean subtree refresh lands. Keep JSON writing, PNG/file IO, and editor UX app-local while only schema meaning/validation stays shared.
- `Partial`: first Workspace Authoring `WA1` host attach is in place through pane/module chrome, draft Apply/Cancel, accepted-only persistence, overlay cycling, and shared `kit_workspace_authoring >= 0.5.0` font/theme surface adoption; next stabilize step is visual acceptance plus closeout before module-content swapping or next-host rollout.
- `Stabilize`: keep the current `kit_render` surface narrow to shared authoring-overlay geometry/layout types unless a future visual/runtime pass justifies broader renderer/runtime extraction beyond the centralized SDL text lane.
- `Missing`: broader shared-core promotion (`core_io`, `core_data`, `core_trace`, execution core) should stay deferred until a concrete app lane needs it.

### `ray_tracing`
Current shared profile:
- Broad shared adoption: `core_base/core_io/core_scene/core_space/core_time`, theme/font, `kit_viz`.
- partial execution-core adoption is now in place through shared
  `core_queue >= 1.0.0` and `core_workers >= 1.0.0` in the native `3D` tile
  scheduler completion and worker lanes, while tile planning, adaptive split
  policy, and the rest of execution-core ownership remain RayTracing-owned.
- `core_mesh_asset >= 0.3.0` is now partially adopted through the MRT2 runtime
  mesh asset loader: schema meaning, runtime payload validation, and file-backed
  runtime mesh document loading route through the shared module, while
  scene-relative asset resolution, JSON scene traversal, retained app-local mesh
  asset sets, native triangle build, material lookup, and acceleration remain
  RayTracing-owned.
- `core_authored_texture >= 0.1.1` is now partially adopted through the authored-texture loader/runtime-binding lane: schema/version, binding/output/primitive vocabulary, face-role semantics, and JSON-free manifest-contract validation now route through the shared module while JSON parsing, image loading, runtime invalid-binding UX, and scene persistence remain app-local.
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
- `Stabilize`: first Workspace Authoring host attach is closed for rollout over the menu/scene-editor shell. `RWA1-S0/S1/S2/S3/S4/S5` are complete as an implementation/docs lane: the vendored authoring kit is refreshed to `kit_workspace_authoring >= 0.5.0`, the shared `Alt+C` then `Alt+V` entry/toggle chord routes before menu/editor input, active-only pane overlay drawing uses shared overlay button layout/hit testing with app-local RayTracing pane/module readout, the full-screen Font/Theme overlay uses shared layout, hit testing, labels, enabled checks, preset mappings, and button-to-action classification, and accepted-only persistence routes text/font/theme drafts through app-owned preference lanes. Reserved authoring triggers are consumed only while active, `Tab` / Mode cycles overlay state, `Enter` / Apply accepts and persists the runtime draft, `Esc` / Cancel restores the entry baseline, Add/custom theme slots remain stubs, and normal runtime remains free of authoring HUD/reminder text. The package gate is no longer blocked by TimerHUD drift when built against the current live shared root; the vendored TimerHUD subtree refresh remains a separate support-lane follow-up, and `map_forge` has since started its follow-on attach through `MFWA1-S1`.
- `Partial`: first `core_authored_texture` cutover is now in place for authored-texture loader validation and face-role vocabulary, but it is intentionally bridge-first. `ray_tracing` still defaults to the vendored subtree host and uses a bounded workspace-shared fallback until the next clean subtree refresh lands. Keep JSON parsing, PNG/image IO, runtime invalid-binding UX, and scene persistence app-local while only schema meaning/validation stays shared.
- `Partial`: first `core_mesh_asset` cutover is now in place for runtime mesh
  asset file validation and payload ownership. `ray_tracing` uses a
  workspace-shared fallback until the vendored subtree includes
  `core_mesh_asset`; keep native `3D` triangle generation and BVH out of this
  shared contract.
- `Partial`: TimerHUD host adoption is now on the explicit `TimerHUDSession` path for `ray_tracing`: the app owns session creation/config, frame and per-pass timer hooks now route through session APIs, and the packaged launcher exports runtime-owned TimerHUD settings/output defaults with the overlay forced on for proof. The remaining follow-up is vendored `third_party/codework_shared/timer_hud` subtree refresh in a clean worktree so the default subtree-backed build matches the live shared-root verification path.
- `Partial`: native `3D` tile scheduling now uses shared `core_queue` and
  `core_workers` for bounded completion handoff plus worker-pool dispatch; keep
  scheduler/job/wake/kernel policy and tile-domain behavior app-local unless a
  broader execution-core convergence is justified later.

### `line_drawing`
Current shared profile:
- Broad shared adoption: `core_base/core_scene/core_trace/core_math/core_time`, theme/font, and vendored shape dependencies.
- the host now consumes that shared surface through a vendored `third_party/codework_shared` subtree instead of direct workspace-local `../shared` linkage.
- first `kit_render` adoption slice is now in place for the font migration: Makefile wiring, shared role/tier/zoom bridge policy, active Vulkan UTF-8 draw/measure runtime, and packaged launcher/runtime default alignment to the shared `ide` font baseline.
- first pane-host interaction slice is now in place for layout resizing: shared `core_pane` owns pane solve and shared `kit_pane` owns splitter hover/drag state while pane purpose stays app-local.
- first `core_mesh_asset >= 0.3.1` adoption slice is now in place for the object-workspace asset lane: primitive-seed authored object assets save/load through shared `mesh_asset_authoring_v1` documents while `ObjectAuthoring` evaluation, app-local extensions, and asset-browser UX remain local.
- first `core_mesh_preview >= 0.1.0` adoption slice is now in place for the imported STL/runtime mesh viewport lane: shared `core_mesh_preview_runtime_v1` sidecars own bounded feature-edge preview payloads, source counts, and local bounds while `line_drawing` keeps renderer colors, selection, hitboxes, auto-scale placement, scene-bounds preservation, and pane readouts local.
- first Workspace Authoring host slices are now in place: `kit_workspace_authoring >= 0.5.0` owns the entry chord, reserved trigger semantics, overlay button layout/hit testing, and full-screen font/theme panel layout/hit/action semantics while `line_drawing` owns SDL routing, host state, app-local pane readout drawing, runtime font/theme preview, and accepted-only preference persistence.

Gaps:
- `Stabilize`: first font-runtime unification slice is complete; active Vulkan text and the former scattered fallback UI text paths now route through the centralized bridge/helper layer over shared `kit_render`, while remaining drift is bounded to centralized non-Vulkan fallback behavior and thin local fallback font-path ownership.
- `Stabilize`: first shared pane-resize slice is complete; keep future resizing/persistence/layout-authoring work additive on top of the shared `core_pane` + `kit_pane` seam instead of reopening app-local splitter math.
- `Stabilize`: keep the object-asset primitive-seed save/load lane on shared `core_mesh_asset` documents while `ObjectAuthoring` evaluation, line-drawing extension payloads, mesh generation, and asset browser/UI semantics remain app-local.
- `Stabilize`: keep runtime mesh viewport previews on shared `core_mesh_preview` sidecars so high-triangle imported STL assets do not require full triangle rendering in UI paths; defer retopo/LOD/GPU-buffer/collision-proxy ownership until RayTracing and PhysicsSim adoption needs are concrete.
- `Stabilize`: `LDWA1` host attach is complete through S5; active-only pane overlay, shared full-screen font/theme panel adoption, accepted-only preference persistence, and closeout are done, with module content placement still deferred.
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
- `Stabilize`: Workspace Authoring `WASR-S2` is complete; font/theme authoring layout, hit testing, labels, preset mappings, and action classification now come from `kit_workspace_authoring >= 0.5.0`, while `kit_render` drawing, live renderer mutation, runtime state, and session persistence remain host-owned.
- `Stabilize`: stale copied foreign font/theme env wiring has been removed from the packaged launcher in favor of WorkspaceSandbox-specific env names.
- `Missing`: no new text-runtime migration is needed unless a future visual pass proves a real host-specific issue, because the active text path is already on shared `kit_render`.

## Cross-System Priority Order (next)
1. Stabilize `core_sim` adoption docs/examples across the four proven shapes:
   fixed-step (`gravity_orbit_sim`), entity/group pass order (`behavior_sim`),
   solver/substep (`physics_sim`), and progressive/runtime-frame routing
   (`ray_tracing`).
2. Complete `core_data` + `core_pack` consolidation in `map_forge`, `ray_tracing`, `physics_sim` (DAW is now stabilize-only for this lane).
3. Expand standardized runtime `core_trace` lanes in `map_forge`, `physics_sim`, and later `core_sim` host adapters where tooling-first usage still dominates.
4. Evaluate whether `ray_tracing` should widen beyond the current
   `core_queue`/`core_workers` tile-scheduler slice into broader execution-core
   adoption (`sched/jobs/wake/kernel`).
4. Keep `line_drawing` in stabilize mode and only migrate additional IO/helpers or fallback text paths when they provide clear value beyond the now-shared active Vulkan runtime.
5. Keep `drawing_program` in stabilize mode and only revisit shared text-runtime extraction if the centralized SDL lane becomes a real maintenance problem.
6. Expand `fisiCs` shared-core usage only where it improves compiler/runtime clarity without disrupting shim-focused flows.

## Maintenance Rule
When any app materially changes shared-lib usage:
- Update this doc first (gap state + next steps).
- Update `11_version_compat_matrix.md` if minimum required versions changed.
- Update `SHARED_LIBS_CURRENT_STATE.md` if adoption level changed.
