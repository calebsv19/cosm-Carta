# Program Shared-Lib Connection Gaps

Last updated: 2026-08-20
Purpose: canonical per-program list of shared-lib connection gaps and next integrations.

Use this with:
- `../11_version_compat_matrix.md` for minimum supported versions.
- `../../SHARED_LIBS_CURRENT_STATE.md` for current adoption snapshot.

## Gap Legend
- `Missing`: not wired in build/runtime/tooling where it would provide clear value.
- `Partial`: wired, but only in additive or narrow paths; broader consolidation still open.
- `Stabilize`: wired broadly; remaining work is hardening/cleanup/documentation.

## Per-Program Gap List

### `codework_compute_runtime`
Current shared profile:
- `core_headless_job >= 0.2.0` is the direct semantic authority for all six
  platform-v1 schema families.
- CR-S1 implements Python-owned duplicate-key rejection, canonical JSON,
  runtime validation, SQLite source/projection/replay mechanics, and
  create-only CAS storage beneath that authority.
- `core_memdb` remains deferred because its memory-item schema is not the
  coordinator database.
- `core_jobs`, `core_workers`, and the wider C execution core remain deferred
  because they do not own distributed attempts, leases, or scheduling.

Gaps:
- `Stabilize`: keep the Python validator mechanically aligned with canonical
  fixtures and standalone `core_headless_job` conformance tests; do not treat
  it as a second semantic authority.
- `Partial`: CR-S1 persists accepted workflows/jobs/events/results/artifacts;
  CR-S2 adds exact-identity app adapters; CR-S3 proves durable local stage
  recovery and provenance; CR-S4 adds exact local compatibility admission; and
  CR-S5 adds immutable multi-version package inventory plus resolve-once exact
  execution identity. CR-S6 adds canonical read-only version-root launch,
  job/attempt mutable-root isolation, process-group timeout ownership, and
  durable pre/post identity receipts. CR-S7 and CR-S8 add replay/upgrade
  lineage and durable local service operations. CR-S9 adds immutable runtime
  observations plus deterministic, TTL-bounded local admission and ranking.
  CR-S10A implements the runtime-owned, non-authorizing exact
  package-materialization planner without changing shared semantics. CR-S11A
  through CR-S11C2 implement the disposable transaction, authenticated
  compatibility/normalization/intake, contained live-root transaction, and
  immutable handoff/import boundaries. CR-S11C3P adds an immutable,
  zero-contact live-canary readiness report and deterministically blocks the
  current nonconforming live helper/profile. CR-S11C3H adds a deterministic
  fixed-profile helper zipapp, schema-18 pre-effect helper journal, contained
  recovery/readback proof, and exact envelope-to-route/host/target binding.
  CR-S11C3D1 adds runtime-local schema-20 immutable decision-bound helper-delivery
  plans and exact host-observation requests, blocks the generic
  replacement/`py_compile` installer, and keeps both future profiles
  non-executable. CR-S11E0 adds local all-sibling compatibility; CR-S12A adds
  atomic expiring capacity reservation; CR-S12B consumes one reservation into
  one claimed attempt and fenced lease while retaining canonical shared events;
  CR-S12C adds append-only renewal/release/recovery/cancellation, lifecycle
  replay, capacity release, and higher-fence retry takeover; and CR-S12D adds
  a runtime-owned transport-free fenced local runner with durable process
  receipts and exactly-once terminal release. CR-S12E adds runtime-owned
  adapter dispatch/report validation and canonical CAS artifact acceptance
  before shared terminal completion without changing shared semantics.
  CR-S12F adds one immutable plan-first finite local scheduling-to-terminal
  cycle that freezes exact admission and resumes durable lower-layer receipts.
  CR-S11E1L0H1A now also retains exact VPS source acquisition, a
  non-authorizing reconciliation plan, and a schema-37 isolated adoption-review
  dossier whose owner/history/candidate decision slots remain unresolved.
  Schema 38 adds coordinator-local immutable explicit decision recording
  without application authority; the live review still has zero decisions.
  Schema 39 adds a coordinator-local, non-executing semantic audit of the exact
  VPS bundle. The live audit is not decision-ready because three exact relative
  dependencies are absent; it also records fixed-version and mutable-current
  predecessor assumptions without promoting them into shared platform
  semantics.
  Schema 41 adds coordinator-local immutable observation of those dependencies:
  two exact identities are observed and the 625,900-byte worker source is
  explicitly blocked by the existing 262,144-byte evidence ceiling. Schema 42
  adds coordinator-local canonical-gzip transport and immutable exact
  reconstruction receipts; all 746,282 dependency bytes now reproduce from
  retained evidence without executing candidate code. Schema 44 then performs
  a dependency-extended static reproduction. Schemas 45-47 observe and
  reconstruct its eight second-order dependencies and reproduce a zero-missing
  14-file semantic review. Observation, transport, and audit reproduction
  remain runtime evidence policy and do not change `core_headless_job`.
  Schemas 58 through 73 likewise keep host/guest backend compatibility,
  scheduling, capacity, fencing, contained preparation, and exact local QEMU
  boot lifecycle coordinator-local. Schema 66 durably binds the reusable
  signer-enabled Linux guest, per-attempt Ed25519 overlay, live QEMU
  process/channel, controlled stop, and terminal cleanup beneath the unchanged
  shared job contract. Schema 67 adds exact local prepared-entrypoint dispatch;
  schema 68 adds immutable no-boot prepared-tree Linux/x86_64
  invocation/dependency admission; schema 69 adds deterministic complete-tree
  newc payload construction and exact crash adoption; schema 70 adds exact
  live-guest verification of every payload member, a process-bound signed
  receipt, controlled stop, and terminal attempt cleanup without application
  execution; and schema 71 freshly repeats that verification before exact
  admitted full-package execution, retaining a signed zero-exit receipt and
  no-rerun recovery. Schema 72 adds signed progress and durable host
  cancellation; schema 73 verifies one guest-emitted signed bounded
  stdout/stderr and artifact payload, binds it to the final immutable
  supervision result, independently rehashes its bytes, commits them to the
  existing create-only CAS, and transactionally retains canonical artifact
  manifests. A physical QEMU gate proves guest-created bytes reach that CAS.
  Schemas 74-84 then generalize bounded multi-artifact success/failure,
  version-addressed lifecycle, exact replay, derived upgrades, paired-version
  isolation/overlap, RayTracing-owned semantic comparison, durable genuine
  application-payload admission/proof import, and current local execution
  compatibility. Schema 85 retains the no-authority adapter from that genuine
  compatibility fact into fresh schema-70, schema-76, and schema-74 identities
  without creating credentials or process effects.
  All remain
  coordinator-local beneath the shared job semantics.
  Helper delivery/installation/live proof, continuous service orchestration,
  remote dispatch, and remote execution remain unimplemented.
- `Stabilize`: keep CR-S2 adapters thin and aligned with app-owned validators
  and commands; repository-declared version remains distinct from executable
  byte identity.
- `Missing`: dedicated create-only zipapp installer and fixed read-only
  observer source/contained proof, delivery and live proof of the hardened
  helper/profile, package transfer/install,
  network service transport, QEMU guest progress/result
  acceptance, and remote parity remain later
  separately governed boundaries.
- `Stabilize`: keep installer transaction policy runtime-owned. `core_io`
  remains a possible physical-helper dependency only after a second consumer
  proves a stable descriptor-relative filesystem primitive; `core_pack` is not
  the worker-package archive contract, and the local execution cores do not
  own distributed installation claims.
- `Stabilize`: keep exact-bundle semantic auditing runtime-owned and
  `core_headless_job` unchanged. Shared semantic growth is not justified by
  one legacy registry/probe bundle. The same applies to schema-41 observation,
  schema-42 exact transport, schema-44 recursive audit reproduction, and the
  schema-45 through schema-47 recursive closure chain.

### `ball_bounce_sim`
Current shared profile:
- `core_theme >= 2.0.1` and `core_font >= 1.0.2` are directly adopted by the
  compact persistent simulation picker through
  `src/app/app_simulation_menu_style.*`. Shared core owns semantic colors,
  font roles, and text-size tiers; the app owns SDL drawing, Retina scaling,
  2D/3D navigation, grouped-list/search/scroll policy, preset purpose, and
  future scene-library UX. `core_base >= 1.0.1` is linked as
  the support dependency, and the macOS package carries the resolved Lato
  regular/bold assets.
- `core_sim >= 0.4.0` is now directly adopted in the interactive SDL shell.
- The adoption is intentionally narrow: shared `core_sim` owns fixed-step
  control-plane state, pass dispatch, frame outcome status, and tick-count
  reporting for the `world_run_fixed_tick` pass.
- Ball Bounce still owns scenarios, units-annotated solver formulas, headless
  summaries/artifacts, worker-job parsing, worker-exchange packaging, SDL
  drawing, and `fisiCs` overlay proof behavior.
- `core_collision2d >= 0.2.0` now has opt-in app-adapter proof through
  `src/physics/collision2d/collision_core_adapter_2d.*` and
  `make core-collision2d-adapter-parity-contract`; this dual-runs local
  collision behavior against a shared-backed adapter and now covers the
  hardened box/box and polygon/polygon fixture tables plus the Phase 17
  compound descriptor area/mass, center-of-mass, inertia, and local-AABB parity
  for the app-local notch/stair/T generated-mask fixtures.
  `core-collision2d-compound-usage-contract` now also compiles the curated
  compound contract with `BALL_COLLISION2D_USE_CORE_COMPOUND_DESCRIPTOR_OPT_IN`
  to prove shared-backed descriptor/mass usage, and
  `core-collision2d-default-cutover-contract` now diffs curated compound and
  rigid hex polygon summaries across clang/package and fisiCs outputs. The
  clang/package compound AABB/mass route and polygon/polygon contact route
  default-adopt `core_collision2d >= 0.2.0` while fisiCs/local behavior remains
  the oracle.
  Ball Bounce also default-adopts shared-backed clang/package circle-vs-circle
  and box/box contact paths through `BALL_COLLISION2D_USE_CORE_CIRCLE_DEFAULT`
  and `BALL_COLLISION2D_USE_CORE_BOX_DEFAULT`; the polygon route now joins that
  default set through `BALL_COLLISION2D_USE_CORE_POLYGON_DEFAULT`.
- `core_rigid2d >= 0.1.1` now has app-adapter proof through
  `src/physics/rigid2d/rigid_core_adapter_2d.*` and
  `make core-rigid2d-adapter-parity-contract`, plus default clang/package
  adoption for the rigid body and solver symbol path through
  `rigid_body_core_default_2d.c` and `rigid_solver_core_default_2d.c`.
  Local rigid files remain the fisiCs oracle and standalone contract source.
- The host links the live shared root through `SHARED_ROOT ?= ../shared` rather
  than a vendored subtree. The P14-S5 adoption decision keeps this
  workspace-linked research shape for `core_sim`, `core_theme`, `core_font`,
  `core_collision2d`, and `core_rigid2d`: desktop/package builds compile the
  shared C sources into the app binary and do not need shared source files at
  runtime. Font assets are copied into the app bundle.
- The Clang simulation picker and complete seeded-compound-room,
  seeded-compound-pair, imported-compound, imported-compound-pair,
  collision-scenario, generated-scene-playback, and generic rigid-3D host
  families now actively adopt exact-source
  `vk_runtime 0.6.0` beneath `vk_renderer 1.3.2` through an app-local
  drawable-sized SDL compatibility canvas and reusable app presentation
  lifecycle. The picker preserves logical-coordinate input/text; all 49
  compatible 3D profiles preserve drawable-coordinate CPU rendering. All pass
  validation-clean capture/readback, resize/recreation, 2x Retina, lifecycle,
  and SDL fallback proof. The imported-pair owner also preserves six
  physics-selected milestone frames through native readback and the SDL
  software-surface oracle. The separate 2D random-scene/contact-world host and
  the fisiCs renderer remain direct SDL.

Gaps:
- `Stabilize`: Vulkan presentation adoption is active for the simulation picker
  and all 49 compatible 3D profiles owned by the seeded-room, seeded-pair,
  imported-single, imported-pair, collision-scenario, generated-playback, and
  generic rigid-3D hosts. Package proof, managed subtree adoption, the separate
  2D random-scene host, compute workloads, and release remain deferred.
- `Stabilize`: keep theme/font adoption behind the app-local style adapter.
  Do not move SDL rendering, menu navigation, catalog meaning, or scene-project
  policy into shared core; broader `kit_ui`/`kit_render` adoption remains
  deferred until the menu needs reusable widget or text-command behavior.
- `Stabilize`: keep the current `core_sim` adoption as a shell adapter only.
  Do not move solver equations, scenario meaning, worker contracts, or
  artifact schemas into shared code without a separate evidence-backed slice.
- `Partial`: `core_collision2d` is default-adopted only for Ball Bounce's
  clang/package circle-vs-circle, box/box, and polygon/polygon contact paths
  plus the clang/package compound descriptor AABB/mass route. Room/floor/wall
  convenience contacts, compound/generated-mask contact behavior, summaries,
  CLI routes, workers, and artifacts remain app-local or deferred.
  P15-S7 selected Phase 16 as Ball Bounce source-first generated/compound
  hardening. P16-S7 selected Phase 17 as the narrow compound descriptor
  extraction lane. P17-S1 adds the first shared `core_collision2d` compound
  descriptor scaffold with bounded primitive parts, validation, and local AABB
  helpers. P17-S2 adds shared compound area/mass, center-of-mass, inertia, and
  aggregate mass-property helpers. P17-S3 adds Ball Bounce opt-in adapter
  parity for local/shared compound descriptor conversion and mass properties
  without changing runtime defaults. P17-S4 adds opt-in shared-backed
  descriptor/mass usage through an explicit macro and contract. P17-S5 makes
  that descriptor/mass route the default clang/package path while preserving
  fisiCs/local oracle behavior. P17-S6 selected Phase 18 as narrow
  polygon/polygon default adoption. P18-S0 mapped the exact cutover, P18-S1
  made polygon/polygon the shared-backed clang/package default through
  `BALL_COLLISION2D_USE_CORE_POLYGON_DEFAULT`, P18-S2 proved generated/compound
  impact gates still align, and P18-S3 selected Phase 19 as app-local
  compound contact generation hardening. Shared compound contact APIs,
  generated-mask fixture catalogs, decomposition policy, selected-part
  attribution, responses, timelines, review media, CLI routes, workers,
  artifacts, and runtime defaults remain app-local or deferred.
- `Partial`: `core_rigid2d` is default-adopted only for Ball Bounce's
  clang/package rigid body and solver symbol path. Worlds, broadphase/contact
  discovery, named scenarios, generated masks, pair/generated/multi-scene
  definitions, timelines, review artifacts, workers, packages, CLI route
  ownership, and Visualizer publication remain app-local or
  deferred.
- `Missing`: `core_sim_trace` stays deferred unless a future Ball Bounce
  artifact/report lane needs standardized control-plane trace samples.
- `Missing`: broader execution-core adoption (`core_queue`, `core_sched`,
  `core_jobs`, `core_workers`, `core_wake`, `core_kernel`) stays deferred until
  Ball Bounce has real async/background work or cross-thread wake ownership.
- `Missing`: managed `third_party/codework_shared` subtree adoption stays
  deferred until Ball Bounce enters release-grade standalone distribution.
  `make -C ball_bounce_sim shared-source-adoption-contract` is the current
  proof gate for the direct live shared-source policy.

### `video_editor`
Current shared profile:
- `core_base`, `core_pane`, `core_theme`, `core_font`, `kit_render`, and
  `kit_ui` are now linked from the managed
  `third_party/codework_shared` subtree by the desktop SDL shell.
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
- `video_editor` is now registered in `bin/shared_subtree_targets.tsv` and
  defaults to `SHARED_ROOT ?= third_party/codework_shared`; the vendored
  snapshot now compiles and links `kit_workspace_authoring` for the first
  Workspace Authoring host slices.

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
- `Missing`: `kit_pane`, `core_action`, and broader input-control adoption stay
  deferred until pane interaction presentation and command routing need shared
  ownership.
- `Partial`: `kit_workspace_authoring >= 0.5.0` is adopted for
  `VEWA1-S1/S2/S3/S4`: shared `Alt+C` then `Alt+V` entry/toggle semantics,
  active keyboard/pointer capture before normal editor routes, pane-overlay
  button geometry/hit testing, shared full-screen Font/Theme layout/action
  classification, and accepted-only persistence routing. Host state, SDL
  drawing, live preview mutation, and settings storage stay app-local in Video
  Editor. `VEWA1-S5` is closed after the input-routing audit plus stale/zero
  authoring viewport fallback and shell-coordinate pointer refinement fixed
  top-level click/key leak-through, top-control hit misses, and background
  hover leakage enough for operator-accepted rollout closeout.
- `Partial`: execution-core adoption now covers the first thumbnail/sample
  worker lane and the first outer SDL loop wait/wake bridge. Broaden this to
  priority scheduling, preview/proxy/export job classes, and diagnostics only
  after the thumbnail and loop lanes stay stable under real media.

### `datalab`
Current shared profile:
- Strong on `core_base/core_io/core_data/core_pack`, `core_font`, and `kit_viz`.
- `WASR-S3` now adopts the shared `kit_workspace_authoring >= 0.5.0` font/theme surface with the required `kit_render`/`core_theme` dependency chain.
- `kit_ui >= 0.11.2` now owns the bottom playback HUD, top-left session
  HUD alpha-aware floating style, button/readout row layout, nested
  corner/inset sizing, and optional SDL rounded panel/button/readout/scrollbar adapter
  while DataLab keeps playback/action policy, session content, file stepping,
  manual edge-wrap navigation, active theme persistence, and theme/custom-token
  mapping local.
- Bounded `kit_graph_timeseries >= 0.2.2` trace graph adoption is now in
  place: shared view computation, zoom, hover inspection, plot draw commands,
  and hover overlay commands route through the kit while DataLab keeps
  trace/session meaning, cursor policy, and SDL replay local.
- Protected `0.3.6` source now adopts `vk_runtime 0.6.0` beneath
  `vk_renderer 1.3.1` for default picker/session presentation through an
  app-local high-DPI SDL compatibility canvas. Exact-source, validation,
  readback, resize, capture, restart, package, and real-host proofs are green;
  SDL fallback/oracle and app profile semantics remain local.

Gaps:
- `Stabilize`: keep the Vulkan lane presentation-only until a separately
  profiled workload justifies compute adoption with CPU parity/fallback. The
  source/package proof does not imply a public `0.3.6` release or Linux-PC run.
- `Stabilize`: Workspace Authoring `WASR-S3` is complete; DataLab now uses the shared font/theme authoring layout, hit IDs, labels, preset mappings, and button-to-action classification while SDL drawing, custom theme editor state, accepted mutation, and persistence remain host-owned.
- `Stabilize`: the playback HUD and session data HUD are now the first direct
  DataLab `kit_ui` HUD-row/SDL-adapter adopters; reuse this adapter in one more
  SDL program before promoting broader app-agnostic action semantics, and only
  then broaden rounded-surface polish to picker/session panels.
- `Stabilize`: picker pane geometry now routes through `core_pane`, while the
  persistent file/directory rails use the `kit_ui >= 0.11.2` direct-SDL
  scrollbar adapter; DataLab keeps pointer routing and scroll offsets local.
- `Stabilize`: `core_viewport2d` proving-host adoption is now in place for sketch/image lanes; keep viewport persistence, fit-reset behavior, and future large-image/tiled follow-ons aligned to the shared math contract instead of regressing into app-local viewport drift.
- `Stabilize`: bounded `kit_graph_timeseries` trace graph adoption is complete;
  only open a fresh graph plan for concrete needs such as panning, multi-series
  inspection, richer overlays, or style-aware hover mapping.
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
- Committed adoption source now directly builds and links `vk_runtime 0.6.0`
  beneath `vk_renderer 1.3.1`; validation-clean startup/resize/restart,
  readback/capture, and 2.0x Retina proof pass while DAW presentation and audio
  policy remain app-owned. This is committed but not released DAW truth.

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
- `core_authored_texture >= 0.2.0` is directly adopted from the canonical
  workspace source for the ITF1-ITF3 exact indexed-palette and atlas-cell
  contract. Dungeon keeps JSON, tile-key meaning, resource loading,
  diagnostics, and renderer/fallback policy app-local.
- the host now participates in the managed shared-subtree manifest, and
  build/package/shared-font paths default to `third_party/codework_shared`
  instead of direct workspace-local `../shared` linkage.
- Dungeon gameplay rules, renderer policy, input action identity, and SDL event
  ownership remain app-local.
- `vk_runtime 0.6.0` beneath `vk_renderer 1.3.1` is now adopted by the default
  Clang/package presentation path through an app-local high-DPI SDL
  compatibility canvas. Canonical-source, validation-clean startup, readback,
  real resize/recovery, 2x Retina, restart, package, installed-app, and live
  indexed-tileset application capture evidence is retained. Runtime compute,
  residency, and timing workloads remain unused.

Gaps:
- `Stabilize`: subtree-host conversion is complete; keep future shared updates
  flowing through `bin/update_shared_subtrees.sh` after the current Dungeon
  worktree is committed instead of reopening live-path defaults.
- `Stabilize`: the managed subtree now contains `core_authored_texture 0.2.0`;
  continue future updates only through `bin/update_shared_subtrees.sh` and do
  not hand-edit the vendored snapshot.
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
- `Stabilize`: managed Vulkan presentation is complete at `vk_runtime 0.6.0` /
  `vk_renderer 1.3.1`. Keep gameplay draw semantics and renderer policy
  app-local; profile before proposing any compute workload adoption.
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
- committed default clang/package presentation now adopts `vk_runtime 0.6.0`
  beneath `vk_renderer 1.3.1` through app-local SDL compatibility wrappers;
  strict source/package proof covers validation-clean lifecycle, readback,
  real resize/out-of-date recovery, capture, 2x Retina, and restart.
- SDL remains the window/event owner and `fisiCs` renderer oracle. App-specific
  pane/viewport/render policy, simulation-body colors, and domain meaning stay
  local; runtime compute, residency, and timing workloads are unused.

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
  `core_theme` / `core_font` / `kit_render` own policy, the active clang/package
  draw path uploads text through the managed Vulkan renderer, and the SDL text
  runtime remains the `fisiCs` compatibility oracle. Do not infer that Cosmic
  owns shared renderer lifecycle or uses runtime compute workloads.
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
- `vk_runtime 0.6.0` and `vk_renderer 1.3.1` are adopted in the committed
  default Clang/package presentation path; fisiCs retains the direct-SDL
  renderer oracle.
- `core_io` remains an indirect support dependency through shared trace/pack
  lanes rather than a first-class Growth-owned file API.

Gaps:
- `Stabilize`: keep the Vulkan adoption presentation-only until a separately
  profiled field workload beats the CPU oracle at a meaningful crossover;
  runtime compute, residency, and timing workload APIs are not active today.
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
- `kit_ui >= 0.11.1` is linked from the managed vendored subtree as the build
  foundation for IDE button behavior unification. Runtime button rendering is
  still app-local until the IDE adapter and compact-panel call-site cutover
  land.

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
- `Partial`: `kit_ui` is now available in the IDE build through the managed
  subtree, but no runtime button family has been cut over yet. Next work should
  add a thin IDE-local adapter and migrate common compact panel buttons before
  broadening to menu, tab, terminal, or authoring-overlay controls.

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
- `Stabilize`: managed Vulkan adoption is complete at `vk_runtime 0.6.0` /
  `vk_renderer 1.3.1`. Carta retains backend selection, SDL fallback, map
  drawing/input/diagnostic policy, and uses the compatibility-preserved tinted
  affine line-mesh seam. Source and packaged proofs cover strict validation,
  lifecycle-handle identity, readback, real resize/recovery, capture dimensions,
  and 2x Retina drawable extents. Native Linux Carta presentation remains a
  separate future proof; the RTX S4 profile is runtime/compute evidence only.
- `Partial`: consolidate map diagnostics into stronger `core_data` contracts and route optional diagnostics archives through `core_pack`.
- `Partial`: expand standardized trace-lane usage (`core_trace`) from tooling-level into clearer runtime diagnostics surfaces.

### `mem_console`
Current shared profile:
- broad direct adoption spans `core_memdb`, the execution-core primitives,
  pane/theme/font helpers, `kit_render`, `kit_ui`, `kit_graph_struct`, and
  `kit_workspace_authoring`
- the protected managed-default app worktree now carries committed shared
  `vk_runtime 0.6.0` beneath `vk_renderer 1.3.1`
- eCho retains SDL window/event policy, Memory DB/UI/graph meaning, frame
  composition, package policy, and presentation-mode ownership

Gaps:
- `Stabilize`: validation-required source proof covers runtime/renderer handle
  identity, nontrivial readback, real resize, capture dimensions, shutdown /
  restart, and measured 2x Retina drawable extents with zero validation
  warnings/errors.
- `Stabilize`: keep the rollout on the managed subtree workflow and preserve
  `SHARED_ROOT=../shared` as a bounded development override only.
- `Partial`: the current app adoption is protected and uncommitted. Review and
  commit app integration/subtree state separately before treating it as
  released program truth.
- `Missing`: no compute acceleration is adopted. Profile a specific Memory
  Console workload with a CPU oracle before proposing any use of runtime
  compute, residency, or timing APIs.

### `physics_sim`
Current shared profile:
- Strong on `core_base/core_io/core_pack/core_scene`, plus `kit_viz`, theme/font.
- `core_pane` is now adopted for editor-shell geometry in `PS4D-2B`, and live splitter hover/drag now adopts shared `kit_pane` through the vendored subtree host.
- `core_viewport2d >= 0.2.1` is now partially adopted for the retained-scene `2D` editor camera path: fit-reset, cursor-anchor zoom, drag-pan, and screen/content transforms now route through the shared viewport math while canvas selection, scene-world meaning, and `3D` orbit behavior remain app-local.
- `core_screen_pick >= 0.1.0` now owns object-body projected-origin indexing,
  nearest ranking, and the ranked overlap list used by repeat-click cycling.
  PhysicsSim retains projection, handles/imports/emitters/boundaries, hit-stack
  arbitration, selection state, drag policy, editor input, and rendering.
- first `kit_render` adoption is now in place for shared font policy resolution plus one shared Vulkan text runtime path through the app-local font bridge.
- first `kit_ui >= 0.9.1` adoption is now in place for bounded menu/editor
  button spec/state/style semantics through the app-local
  `physics_sim_ui_button` SDL wrapper.
- `core_sim >= 0.2.0` is now partially adopted for scene-level runtime stepping and the 3D solver first-pass shell: `SceneState` owns persistent loop state, the frame substep loop runs through seven ordered simulation passes, the scaffold backend owns nested solver loop state, and frame outcome diagnostics are test-covered.
- `core_mesh_preview >= 0.4.0` is now partially adopted through an app-local
  runtime mesh preview bridge: runtime-scene `mesh_asset_instance` records can
  resolve runtime mesh paths, derive preview sidecar paths, and attach
  probe/metadata state without loading full preview payloads, then draw
  transform-aware preview AABBs in retained editor and running simulation
  overlays. Default-solid PhysicsSim mesh fluid obstacles now load the actual
  `core_mesh_asset` runtime document and voxelize transformed runtime triangles
  into the 3D obstacle occupancy field; mesh instances can also switch into
  attached runtime-mesh emitter flow, clearing solid occupancy and emitting
  through actual mesh footprints. Preview sidecars remain visual and diagnostic
  only.
- `core_scene_view >= 0.2.0` is now partially adopted through the app-local
  read-only `PhysicsSimSceneViewPacketReadout`. It consumes
  `ray_tracing_scene_view_packet_v0` compact readback and uses
  `CoreSceneViewPacketSummary` for its small PhysicsSim summary while solver
  projection, cache output, retained scene apply, and
  `extensions.physics_sim` writeback remain app-local.
- the host now consumes those shared modules through a vendored `third_party/codework_shared` subtree instead of direct workspace-local `../shared` linkage.
- `core_data` and `core_trace` partial.

Gaps:
- `Stabilize`: keep `core_screen_pick` limited to projected object-body
  indexing and ranked queries. Projection invalidation, occlusion, hit-stack
  arbitration, and repeat-click cycling remain PhysicsSim-owned.
- `Stabilize`: first scene-level `core_sim` pass-network slice is now complete; keep fluid equations, mode hook bodies, emitter/backend/object operations, scene time semantics, and HUD/render payloads app-local while shared `core_sim` owns pass ordering, tick/frame outcome shape, pause sync, and exact substep-count execution.
- `Stabilize`: first shared pane-resize slice is now complete; keep pane purpose, viewport behavior, and editor semantics app-local while shared `core_pane >= 0.2.0` owns split solve and shared `kit_pane >= 0.2.0` owns splitter hover/drag interaction state.
- `Stabilize`: the retained-scene `2D` editor viewport bridge is now complete; keep scene bounds authority, local canvas rect routing, `3D` orbit projection, and higher-level editor gesture policy app-local while shared `core_viewport2d` owns fit-reset, cursor-anchor zoom, drag-pan, and screen/content transform math.
- `Partial`: complete PhysicsSim visual parity validation and trim the remaining bridge-only wrappers now that the cache/runtime layer itself lives in shared `kit_render`.
- `Stabilize`: keep the current `kit_ui` lane bounded to shared button
  spec/state/style semantics while SDL drawing, palette tuning, and exact
  menu/editor placement remain app-local.
- `Stabilize`: Workspace Authoring `PSWA1-S1/S2/S3/S4/S5` is complete on the menu/editor shell; shared `kit_workspace_authoring >= 0.5.0` owns the entry/toggle chord, active reserved-trigger capture, active pane-overlay button geometry/hit testing, and full-screen Font/Theme layout/hit/action semantics while SDL drawing, pane/module labels, `SceneEditorPaneHost` geometry readout, runtime preview mutation, and accepted-only persistence remain app-local. Apply saves theme/font/text-size through ignored `data/runtime` state, Cancel/shutdown restore active previews without saving, and S5 closed as docs/status only with no additional app commit.
- `Stabilize`: first `core_mesh_preview >= 0.4.0` adoption is now in place for
  runtime mesh preview diagnostics plus retained editor/runtime overlay
  preview bounds. Keep collision proxies, SDFs, solver projection quality, and
  physics semantics app-local while shared preview sidecars provide bounded
  path/probe/metadata/bounds state for visual inspection. The first app-local
  fluid-obstacle path now uses actual `mesh_asset_runtime_v1` geometry for
  default-solid mesh instances and feeds the existing 3D obstacle occupancy
  rebuild. Mesh-emitter attachments remain app-local PhysicsSim semantics over
  the same runtime mesh geometry.
- `Stabilize`: keep `core_scene_view >= 0.2.0` adoption as read-only packet
  readback and compact summary derivation. Do not use scene-view packets to
  write `extensions.physics_sim`, drive solver projection, or emit cache
  output unless a later metadata-authority proof explicitly selects that
  behavior.
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
- the committed Clang/package presentation path now adopts `vk_runtime 0.6.0`
  beneath `vk_renderer 1.3.1` through an app-local high-DPI SDL compatibility
  canvas. Exact-source, validation/readback/capture, resize, Retina, restart,
  real-frame, and package proof are green; SDL/fisiCs remain explicit oracles.
- `WA1-S4` plus `WASR-S4` now adopt vendored `kit_workspace_authoring >= 0.5.0` for shared entry-chord, reserved-trigger, overlay-cycle, font/theme layout, standard button hit testing, labels, enabled-state checks, preset mappings, and button-to-action classification, and the authoring chrome also directly consumes `kit_workspace_authoring_ui` helpers plus narrow `kit_render` geometry types (`KitRenderRect`) while the app-local frame chrome derives pane/module readout from shared pane-module bindings and keeps SDL drawing, accepted-only state mutation, snapshot persistence, and pane/module content host-owned; module swapping remains a future WA1 slice.

Gaps:
- `Stabilize`: mixed vendored/live host cleanup is complete; keep future shared updates flowing through `bin/update_shared_subtrees.sh` instead of reopening workspace-linked defaults except for bounded local debugging.
- `Stabilize`: packaged font assets and runtime font-path fallback now resolve through vendored/shared packaged locations first; keep launcher/package expectations aligned with that contract.
- `Stabilize`: first shared pane-resize slice is now complete; keep pane meaning and panel/canvas semantics app-local while shared `core_pane >= 0.3.0` owns split solve plus cached splitter-hit enumeration and shared `kit_pane >= 0.3.0` owns splitter hover/drag interaction state. `drawing_program` is now the first proving host routing hover/begin-drag through the cached-hit registry before any IDE cutover.
- `Stabilize`: startup/session persistence failures observed during this rollout were traced to stale incremental objects after shared header churn, not to invalid `core_pane` / `kit_pane` pack contracts. Future hosts that adopt pane/session struct changes should either run an explicit clean rebuild before diagnosis or keep Makefile/header dependency coverage in place so saved pack debugging is not polluted by mixed-object binaries.
- `Partial`: first `core_authored_texture` cutover is now in place for authored-texture export semantics, but it is intentionally bridge-first. `drawing_program` still defaults to the vendored subtree host and uses a bounded workspace-shared fallback until the next clean subtree refresh lands. Keep JSON writing, PNG/file IO, and editor UX app-local while only schema meaning/validation stays shared.
- `Partial`: first Workspace Authoring `WA1` host attach is in place through pane/module chrome, draft Apply/Cancel, accepted-only persistence, overlay cycling, and shared `kit_workspace_authoring >= 0.5.0` font/theme surface adoption; next stabilize step is visual acceptance plus closeout before module-content swapping or next-host rollout.
- `Stabilize`: keep the current `kit_render` surface narrow to shared authoring-overlay geometry/layout types unless a future visual/runtime pass justifies broader renderer/runtime extraction beyond the centralized SDL text lane.
- `Stabilize`: keep managed Vulkan adoption presentation-only. Drawing and
  compose policy remain app-local, and compute/residency/timing workloads must
  not be inferred from lifecycle linkage.
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
- `core_scene_view >= 0.1.0` is now partially adopted through the USV2
  scene-view packet bootstrap: schema constants, preview quality/degraded
  reason/display flag vocabulary, pick-id/readback structs, and compact JSON
  readback route through the shared module while packet production,
  serialization, material semantics, live editor routing, and scene mutation
  remain RayTracing-owned.
- `core_data/core_pack/core_trace` are partly additive/tooling-oriented.
- `core_scene_compile` pre-`TP-S3` baseline wiring is now in place for authoring->runtime handoff preflight.
- first `kit_render` adoption slice is now in place for the font migration: Makefile wiring, shared role/tier/render-scale bridge policy, active helper/menu/timer-HUD UTF-8 measure/draw runtime, and wrapped helper labels now route through the shared external text path.
- the scene editor pane shell now also adopts shared `core_pane` for pane solve and shared `kit_pane` for splitter hover/drag interaction, while pane meaning and editor routing remain app-local.
- the host now consumes that shared surface through a vendored `third_party/codework_shared` subtree instead of direct workspace-local `../shared` linkage.
- `core_sim >= 0.2.0` is now partially adopted for runtime-frame control-plane routing, and the app now consumes that simulation surface through the vendored subtree host instead of a direct live `../shared` `CORE_SIM_DIR` reference.
- `core_viewport3d >= 0.1.0` is adopted from managed snapshot `e804658` for native editor pan,
  anchor zoom, and controlled orbit transitions through a thin conversion
  bridge. RayTracing retains durable target/projector storage, zoom-domain
  policy, input, picking, overlays, mesh previews, final rendering, and BVHs.
- `core_screen_pick >= 0.1.0` now owns whole-object projected-origin indexing
  and deterministic nearest selection for primitives and mesh instances. Exact
  mesh/triangle/face selection remains in RayTracing's material/sub-element
  lanes, and the app still owns projection, click/hover routing, and rendering.
- `kit_ui >= 0.11.2` now owns wheel-delta scroll evaluation, top-anchor list
  content sizing, and offset clamping for the native editor object list.
  RayTracing retains row content, selection, clipping, SDL drawing, scrollbar
  paint, and pane routing.
- `kit_viewport3d >= 0.1.0` is adopted behind a thin SDL adapter for
  Solid/Material surface outlines. Shared kit owns the proven semantic accent,
  selected/hover priority, silhouette, relative depth-edge, and object-owner
  boundary composition; RayTracing retains CPU rasterization, Vulkan texture
  upload/cache, projection, picking, and overlay visibility policy.

Gaps:
- `Partial`: the `core_viewport3d` bridge, focused tests, package self-test,
  Desktop refresh, and managed-subtree refresh pass. Keep the local navigation
  math as rollback oracle until hands-on proof and the four remaining native
  volume-surface broad-suite assertions are closed.
- `Stabilize`: keep `core_screen_pick` limited to whole-object projected-origin
  ranking. RayTracing now tries its existing coherent-LOD triangle hit test
  before that shared fallback for mesh hover/click; occlusion and
  pointer/selection policy remain RayTracing-owned.
- `Stabilize`: keep the first `kit_ui` list adoption bounded to scroll math;
  object meaning, row virtualization, SDL drawing, and selection remain local.
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
- `Partial`: `core_mesh_preview >= 0.5.0` now supplies renderer-neutral preview
  diagnostics and coherent indexed LOD geometry. RayTracing keeps the loader
  ABI unchanged and prepares bounded LODs in a separate editor-only store; its
  app-local Bounds/Wire/Solid/Material vocabulary also pins stable quality
  invalidation across zoom, pan, hover, and selection. Rendering and picking
  cutover remains the next boundary. Keep final render geometry, native GPU and
  depth rendering, normals, materials, light sampling, camera, overlays, cache
  policy, and BVH construction app-local.
- `Partial`: TimerHUD host adoption is now on the explicit `TimerHUDSession` path for `ray_tracing`: the app owns session creation/config, frame and per-pass timer hooks now route through session APIs, and the packaged launcher exports runtime-owned TimerHUD settings/output defaults with the overlay forced on for proof. The remaining follow-up is vendored `third_party/codework_shared/timer_hud` subtree refresh in a clean worktree so the default subtree-backed build matches the live shared-root verification path.
- `Partial`: native `3D` tile scheduling now uses shared `core_queue` and
  `core_workers` for bounded completion handoff plus worker-pool dispatch; keep
  scheduler/job/wake/kernel policy and tile-domain behavior app-local unless a
  broader execution-core convergence is justified later.
- `Partial`: first `core_scene_view >= 0.1.0` cutover is now in place for the
  renderer-free scene-view schema/readback vocabulary. `ray_tracing` uses a
  workspace-shared fallback until the vendored subtree includes
  `core_scene_view`; keep packet production, JSON serialization, material
  display meaning, live editor routing, and mutation policy app-local.

### `line_drawing`
Current shared profile:
- Broad shared adoption: `core_base/core_scene/core_trace/core_math/core_time`, theme/font, and vendored shape dependencies.
- the host now consumes that shared surface through a vendored `third_party/codework_shared` subtree instead of direct workspace-local `../shared` linkage.
- first `kit_render` adoption slice is now in place for the font migration: Makefile wiring, shared role/tier/zoom bridge policy, active Vulkan UTF-8 draw/measure runtime, and packaged launcher/runtime default alignment to the shared `ide` font baseline.
- first pane-host interaction slice is now in place for layout resizing: shared `core_pane` owns pane solve and shared `kit_pane` owns splitter hover/drag state while pane purpose stays app-local.
- first `core_mesh_asset >= 0.3.1` adoption slice is now in place for the object-workspace asset lane: primitive-seed authored object assets save/load through shared `mesh_asset_authoring_v1` documents while `ObjectAuthoring` evaluation, app-local extensions, and asset-browser UX remain local.
- `core_mesh_preview >= 0.5.0` is adopted for the imported STL/runtime mesh viewport lane: shared sidecars own bounded feature-edge payloads, explicit source/preview counts, bounds/span/sphere metadata, sampled-triangle/point-cloud/bounds-proxy modes, runtime-file helpers, metadata-only reads, and probes. The additive coherent indexed LOD builder now also owns renderer-neutral triangle-budget reduction for Wire, Solid, and Material previews. LineDrawing keeps projection, CPU depth rasterization, silhouette composition, renderer colors/cache lifetime, interaction-quality timing, auto-scale placement, scene-bounds preservation, and pane layout local.
- `core_viewport3d >= 0.1.0` is adopted from managed snapshot `a0714c0` for free-view pan, orbit,
  anchor zoom, and frame transitions through an app-local effective-target
  bridge. `FreeViewCamera`/Grid storage, degrees, projection, input and
  authoring arbitration, picking, overlays, and rendering remain LineDrawing
  owned. Hands-on proof remains pending CV3D4.
- `core_screen_pick >= 0.1.0` now owns whole-object projected visual-center
  indexing and deterministic nearest selection. LineDrawing retains
  `FreeViewCamera`/Grid projection, hitbox rebuild lifecycle, gizmo/resize-handle
  priority, topology picking, authoring arbitration, and rendering.
- `kit_viewport3d >= 0.1.0` is adopted for the filled-surface and
  outline-only composition stage over LineDrawing's existing CPU raster
  buffers. Shared kit owns only the matching semantic palette and boundary
  composition; LineDrawing retains projection, rasterization, adaptive quality,
  cache, picking, authoring, and renderer ownership.
- first `core_scene_view >= 0.1.0` adoption slice is now in place for the USV2
  read-only packet consumer: `LayoutSceneViewPacketConsumer` consumes shared
  schema/readback helpers while plane/prism face-group mapping, canonical scene
  mutation, viewport drawing, and editor picking remain LineDrawing-owned.
- first Workspace Authoring host slices are now in place: `kit_workspace_authoring >= 0.5.0` owns the entry chord, reserved trigger semantics, overlay button layout/hit testing, and full-screen font/theme panel layout/hit/action semantics while `line_drawing` owns SDL routing, host state, app-local pane readout drawing, runtime font/theme preview, and accepted-only preference persistence.
- committed managed `vk_runtime 0.6.0` / `vk_renderer 1.3.1` presentation
  adoption now delegates Vulkan lifecycle ownership through compatibility
  wrappers. Automated proof covers exact source, validation, readback/capture,
  actual resize, restart, and 2x Retina extents while editor, scene, CPU
  raster/quality, text, input, and compute policy remain app-owned.

Gaps:
- `Stabilize`: keep the managed Vulkan lane presentation-only and preserve the
  compatibility surface; do not infer compute adoption from runtime linkage.
- `Stabilize`: keep `core_screen_pick` limited to whole-object projected-center
  ranking. Hitbox rebuild scheduling, gizmo/handle/topology priority,
  authoring arbitration, and rendering remain LineDrawing-owned.
- `Stabilize`: first font-runtime unification slice is complete; active Vulkan text and the former scattered fallback UI text paths now route through the centralized bridge/helper layer over shared `kit_render`, while remaining drift is bounded to centralized non-Vulkan fallback behavior and thin local fallback font-path ownership.
- `Stabilize`: first shared pane-resize slice is complete; keep future resizing/persistence/layout-authoring work additive on top of the shared `core_pane` + `kit_pane` seam instead of reopening app-local splitter math.
- `Stabilize`: keep the object-asset primitive-seed save/load lane on shared `core_mesh_asset` documents while `ObjectAuthoring` evaluation, line-drawing extension payloads, mesh generation, and asset browser/UI semantics remain app-local.
- `Stabilize`: keep runtime mesh viewport previews on shared `core_mesh_preview` sidecars and coherent indexed LODs so high-triangle imported STL assets remain bounded in UI paths. RayTracing can adopt the same neutral feature-edge/LOD data while retaining its native renderer, materials, and BVHs; GPU buffers, collision proxies, solver truth, retopo, and mesh repair remain separate.
- `Stabilize`: `LDWA1` host attach is complete through S5; active-only pane overlay, shared full-screen font/theme panel adoption, accepted-only preference persistence, and closeout are done, with module content placement still deferred.
- `Partial`: first `core_scene_view` cutover is deliberately read-only and
  schema/readback-only. Keep the workspace-shared fallback temporary until a
  clean managed subtree refresh lands, and do not route LineDrawing's live
  editor drawing, picking, or canonical object mutation through the packet.
- `Missing`: decide later whether the centralized non-Vulkan fallback in `text_draw.c` should also move fully into shared runtime, or whether it should remain intentionally local because the Vulkan path is the authoritative host mode.
- `Missing`: execution-core adoption beyond `core_time` only if future loop/dispatch behavior warrants standardization.

### `workspace_sandbox`
Current shared profile:
- Active UI text already renders through shared `kit_render`.
- Runtime now adopts shared `core_action`, `core_layout`, `core_config`, `core_pane`, `core_pane_module`, `core_pane_snapshot`, and `kit_workspace_authoring`.
- build/package/test/doc paths now resolve through a vendored `third_party/codework_shared` subtree instead of direct workspace-local `../shared` linkage.
- committed managed `vk_runtime 0.6.0` / `vk_renderer 1.3.2` presentation
  adoption passes from the vendored default: runtime
  lifecycle identity, compatibility mirrors, strict validation, real resize,
  capture readback, package self-test, and refreshed Desktop bundle are proven.

Gaps:
- `Stabilize`: subtree-host conversion is complete and verified; keep future shared updates flowing through `bin/update_shared_subtrees.sh` instead of reopening live-path coupling.
- `Stabilize`: keep future Vulkan updates on the managed subtree workflow and
  keep presentation lifecycle adoption distinct from compute usage.
- `Stabilize`: font default normalization is now complete for the active shared-kit lane: lifecycle boot, invalid saved-font fallback, font-theme panel selection, and packaged launcher defaults now align to the shared `ide` font baseline.
- `Stabilize`: Workspace Authoring `WASR-S2` is complete; font/theme authoring layout, hit testing, labels, preset mappings, and action classification now come from `kit_workspace_authoring >= 0.5.0`, while `kit_render` drawing, live renderer mutation, runtime state, and session persistence remain host-owned.
- `Stabilize`: stale copied foreign font/theme env wiring has been removed from the packaged launcher in favor of WorkspaceSandbox-specific env names.
- `Missing`: no new text-runtime migration is needed unless a future visual pass proves a real host-specific issue, because the active text path is already on shared `kit_render`.

### `connected_mechanics_sim`
Current shared profile:
- CMS-F9 defaults to the managed `third_party/codework_shared` snapshot at
  shared commit `8ac8abf` and records the exact source tree identity.
- `core_base 1.0.1`, `core_sim 0.4.2`, `core_trace 1.0.2`, and
  `core_sim_trace 0.1.1` are directly linked for app-foundation identity,
  persistent fixed-step control, ordered no-op passes, frame tracing, and
  shutdown finalization.
- `core_action 0.1.1`, `core_time 1.0.1`, and `core_wake 1.0.2` are directly
  linked for stable input-trigger normalization, monotonic deadlines, and
  interruptible waits. Event intake, routing precedence, invalidation,
  diagnostics, runtime-mode selection, and wait-policy selection remain
  app-owned.
- `core_theme 2.0.1`, `core_font 1.0.2`, `core_pane 0.3.1`,
  `kit_render 0.14.4`, `kit_ui 0.11.2`, `kit_pane 0.3.1`, and
  `kit_workspace_authoring 0.5.1` are directly linked for the responsive
  five-region workspace and null command path. Topology, labels, control
  actions, empty-project meaning, and frame text storage remain app-owned.
- `core_pack 1.1.1` remains explicit transitive trace closure.
- `vk_runtime 0.6.0` beneath `vk_renderer 1.3.1` is directly linked for the
  managed Vulkan lifecycle, device, swapchain,
  pipelines, and presentation. Validation requirements, resource resolution,
  typed failure reporting, recreation policy, and the conservative
  device-idle foundation workaround remain app-owned. Source/package proof
  covers validation, compatibility identity, readback, resize/recovery,
  capture, and material frames without claiming compute adoption.
  `kit_render 0.14.4` forwards existing line/polyline thickness to the additive
  `vk_renderer 1.3.1` filled-stroke primitive; Connected Mechanics retains the choice of
  thin versus outlined structural strokes.
- `core_viewport3d 0.1.0`, `core_screen_pick 0.1.0`, and
  `kit_viewport3d 0.1.0` are directly linked for camera math, deterministic
  screen-space rank, and outline color policy. Reference topology, stable
  semantic keys, navigation policy, device scale, hover/selection state, and
  render composition remain app-owned.
- `core_config 0.1.1` and `core_io 1.1.1` are directly linked for bounded
  display preferences, file reads, and atomic project replacement. Root
  policy, schema/version meaning, session transactions, import policy, and
  stronger durability guarantees remain app-owned or explicitly unclaimed.
- the checked 21-module adoption manifest pins exact versions and assigns each
  planned integration to CMS-F3 through CMS-F9.

Gaps:
- `Stabilize`: keep `core_sim` and `core_sim_trace` bounded to runtime cadence,
  no-op pass routing, trace emission, and cleanup; mechanics meaning remains
  app-owned.
- `Stabilize`: keep `core_action`, `core_time`, and `core_wake` bounded to
  trigger identity, time primitives, and wake mechanics; do not move
  Connected Mechanics routing or mode policy into shared code.
- `Stabilize`: CMS-F7 now routes identical frame-owned `kit_render` command
  frames through null and Vulkan targets; snapshot meaning and phase policy
  remain app-owned.
- `Stabilize`: CMS-F8 keeps the reference scene, camera commands, high-DPI
  policy, and selection meaning app-local while using the three viewport
  modules only for their documented app-neutral contracts.
- `Stabilize`: CMS-F9 keeps project roots, schema compatibility, session
  transitions, and copy/reference meaning app-local while using `core_config`
  and `core_io` only for their documented generic contracts.
- `Stabilize`: upgrade the shared renderer's presentation semaphore ownership
  before removing the app's validation-clean device-idle frame boundary.
- `Missing`: a native renderer capture contract remains deferred because the
  current swapchain images do not advertise transfer-source usage; do not
  re-enable transfer copies without a renderer-owned image-usage/layout
  contract.
- `Stabilize`: retain the vendored default, explicit development override, and
  exact tree/version parity gates; do not replace them with live-root coupling
  or unused blanket linkage.

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
