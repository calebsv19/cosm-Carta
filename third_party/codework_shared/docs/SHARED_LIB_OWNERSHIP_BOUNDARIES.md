# Shared Lib Ownership Boundaries

This document defines what each shared library owns so behavior does not overlap.

## Core Libs

- `core_base`: error/result types, common primitives, shared low-level utilities.
- `core_io`: filesystem/text/binary IO helpers and load/save boundaries.
- `core_data`: structured in-memory data containers and typed table/object model.
- `core_memdb`: active durable SQLite connection, query, migration, event/replay, and graph-storage boundary; CLI and agent workflows remain higher tooling layers in the same shared subtree.
- `core_math`: generic numeric primitives and math helpers.
- `core_collision2d`: UI-free 2D collision shape, geometry, AABB, manifold, bounded compound-descriptor and compound mass-property helpers, and primitive contact-generation semantics.
- `core_rigid2d`: UI-free 2D rigid-body descriptors, mass/inertia helpers, integration helpers, and deterministic contact-solver primitives over `core_collision2d`.
- `core_time`: monotonic time reads and duration arithmetic (no sleep/scheduler behavior).
- `core_queue`: bounded queue primitives and queue ownership semantics.
- `core_sched`: timer/deadline scheduling data structures and callbacks.
- `core_jobs`: main-thread budgeted job queue execution.
- `core_workers`: fixed-size worker pool and task execution lifecycle.
- `core_wake`: cross-thread wake/wait abstraction for kernel orchestration.
- `core_kernel`: runtime phase orchestration and module lifecycle policy.
- `core_scene`: scene schema and scene-level object grouping/state metadata.
- `core_scene_compile`: shared authoring-to-runtime scene compile,
  normalization, content-addressed dependency payload publication, and bundle
  verification boundary; apps retain discovery and dependency-kind meaning.
- `core_scene_view`: renderer-free scene-view packet schema/readback vocabulary, including preview quality, degraded reason, display flags, pick ids, compact JSON readback validation, and compact summary derivation from validated readback metadata.
- `core_mesh_preview`: viewport-safe runtime mesh preview sidecar contract,
  bounded feature-edge payload generation, local bounds/source-count metadata,
  file-backed preview save/load helpers, and renderer-neutral coherent indexed
  LOD construction with host-selected triangle budgets.
- `core_space`: coordinate-space mapping, transforms, grid/window/world
  conversion, canonical right-handed Z-up meter-frame meaning, and validated
  app-neutral legacy Y-up frame conversion. It does not own projection,
  cameras, renderer/solver interpretation, persistence, or host frame-selection
  policy.
- `core_viewport2d`: renderer-agnostic 2D viewport/camera state transitions for fit-to-window, screen/content transforms, drag pan, and cursor-anchor zoom.
- `core_viewport3d`: renderer-agnostic double-precision 3D viewport target,
  canonical orientation/basis, camera-basis pan, anchor zoom, orbit,
  frame/reset/resize transitions, and projected-extents fit-scale math. It
  does not own host camera storage, projector matrices, input, picking,
  rendering, or authoring policy.
- `core_screen_pick`: renderer-agnostic projected-candidate storage, uniform
  hashed-grid indexing, and deterministic nearest/ranked selection by logical
  screen distance, view depth, and stable key. It does not own projection,
  visibility/occlusion, selection state, input, dragging, specialized picking,
  rendering, or authoring arbitration.
- `core_units`: unit vocabulary, unit conversions, and world-scale conversion primitives.
- `core_object`: app-neutral object identity/transform/dimensional-mode validation primitives.
- `core_authored_texture`: authored-texture manifest semantics, binding/output vocabulary, supported primitive/face-role vocabulary, and manifest-contract validation primitives.
- `core_pack`: versioned chunked interchange container (`.pack`).
- `core_layout`: renderer-agnostic layout transaction state (runtime/authoring mode, apply/cancel, revision/rebuild flags).
- `core_config`: lightweight typed runtime configuration table boundary.
- `core_action`: action identity + trigger-binding registry boundary.
- `core_headless_job`: shared outer job, workflow, append-only event, terminal
  result, content/provenance artifact, worker-capability, state-transition,
  attempt, claim, and lease semantics and validation. Coordinator persistence,
  transport, dispatch policy, deployment, and application payloads remain
  outside the module.
- `core_pane_module`: renderer-agnostic pane-module descriptor registry and binding validation semantics.
- `core_trace`: trace capture/ingest/export primitives.
- `core_sim`: UI-free simulation control-plane semantics for fixed-step accumulation, pause/play/single-step state, max-tick clamping, ordered pass execution, and deterministic frame outcomes.
- `core_sim_trace`: optional `core_sim` to `core_trace` adapter for shared simulation control-plane trace lanes and frame/reason markers.
- `core_pane`: renderer-agnostic pane tree layout semantics (split ratios, constraints, splitter hit/drag math). It does not own app snapshot selection, session fallback policy, or host build dependency hygiene around those structs.
- `core_theme`: tokenized color + spacing presets.
- `core_font`: font roles + font tier/size preset contracts.

## Kit Libs

- `kit_render`: shared render command vocabulary, frame-recording/submission contract, backend attach/adopt boundary, shared theme/font/text policy resolution, and renderer-adjacent external text helpers. It does not own widget behavior, pane semantics, host event loops/window lifetimes, persistence, or app-local layout/cursor policy.
- `kit_ui`: shared immediate-mode widget expression, reusable button/state/style semantics, HUD button-row/readout layout, alpha-aware floating HUD style fields, nested corner/inset math, and optional SDL rounded-surface draw adapters. It does not own app action dispatch, playback/session policy, active theme persistence, event loops, pane topology, or renderer lifecycle. Optional caller-owned interaction contexts and surface snapshots retain bounded button focus/capture handles, visible geometry and activation state; optional caller-owned text editors and presentation snapshots own bounded scalar editing and measured row/caret/selection/preedit/hit mechanics. Hosts own scope/field eligibility, actual font/viewport, native text-input session policy, domain actions and persistence.
- `kit_viz`: visualization-specific helpers layered on top of core contracts.
- `kit_viewport3d`: optional renderer-neutral 3D viewport presentation helpers
  for semantic object-outline palettes plus CPU color/depth/owner-buffer
  silhouette, depth-discontinuity, and object-boundary composition. It does not
  own projection, rasterization, renderer resources, cache lifetime, picking,
  input, scene objects, or overlay visibility policy.
- `kit_workspace_authoring`: host-agnostic authoring interaction glue (entry chord checks, trigger mapping, callback-driven action/text-step adapters) plus host-attach contract guidance for theme/font state handoff and top-level shell parity expectations.

## Non-Core Shared Modules

- `vk_runtime`: SDL-independent Vulkan loader, instance, physical/logical
  device, queue-role discovery, headless or caller-surface staged lifecycle,
  graphics/compute/transfer/present selection, validation diagnostics,
  deterministic capability-report ownership, and bounded one-shot
  storage-buffer compute mechanics including shader-module/descriptor/
  pipeline creation, command submission, fence wait, readback, and teardown.
  It also owns persistent device-local buffer sessions, coherent staging
  transfers, persistent descriptors/pipelines, reusable command/fence state,
  dependent-dispatch compute barriers, bounded in-flight timeout recovery, and
  resource accounting. Capability-conditional timestamp query ownership,
  tick-to-nanosecond conversion, and Vulkan-operation GPU elapsed evidence also
  remain here rather than in generic `core_time`. Deterministic export and
  explicit consumption of source-bound precompiled SPIR-V also remain here so
  compiler-free hosts can prove the same kernels without weakening shader
  identity or silently changing build mode. Canonical module/report identity
  is derived from the module `VERSION` file and exposed by the runtime rather
  than duplicated in consumers.
  It does not create SDL surfaces or own swapchains, presentation, render pipelines,
  images, application workload semantics, CPU oracles, app fallback policy, or
  portable performance claims.
- `vk_renderer`: renderer backend implementation details and Vulkan/SDL bridge.
  It owns SDL surface creation, swapchains, graphics presentation, drawing
  resources, out-of-date recovery, and capture/readback while delegating
  instance/device/queue lifecycle to `vk_runtime`. Its legacy device entry
  points and handles remain compatibility wrappers/mirrors during incremental
  adoption.
- `timer_hud`: timing/profiling HUD layer.
- `shape`: shared shape import/export helpers and ShapeLib tooling.
- `sys_shims`: system include compatibility overlays (compile-time concern only).

## Boundary Decisions (Current)

- Vector math:
  - Put generic vec/matrix numeric ops in `core_math`.
  - Put collision-specific double-precision vectors, polygons, projections,
    manifolds, bounded compound descriptors, compound descriptor mass-property
    helpers, and primitive 2D contact queries in `core_collision2d`.
  - Put world/unit placement conversion in `core_space`.
  - Put generic 2D screen/content viewport-camera transforms in `core_viewport2d`.
  - Put canonical 3D editor viewport target/orientation/scale transitions in
    `core_viewport3d`; keep runtime camera/projector representation and gesture
    policy in app adapters.

- Scene vs object ownership:
  - `core_scene` owns app-agnostic scene structure/schema.
  - `core_scene_view` owns read-only scene preview packet vocabulary and
    readback, not canonical scene mutation or live editor routing.
  - App-local object composition or editor-only transient state stays in app code.
  - `core_authored_texture` owns object-bound authored-texture manifest meaning layered above scene/object identity, while `core_scene` continues owning the scene/object envelope and primitive semantics.

- Data interchange:
  - Serialize durable interchange via `core_pack`.
  - Use `core_data` as shared in-memory schema source.
  - Use `core_memdb` as the shared durable queryable memory state and event-replay boundary; keep agent curation policy in CLI/wrapper/skill layers.
  - Use `core_io` for physical IO path operations.

- Execution orchestration:
  - `core_time` owns time measurement only.
  - `core_sched` owns timer data/control only.
  - `core_jobs` owns main-thread budgeted work queue behavior.
  - `core_workers` owns background task execution.
  - `core_wake` owns wait/signal bridge.
  - `core_kernel` owns loop policy and phase order.
  - `core_sim` owns simulation-specific cadence/pass semantics layered above app-domain solvers and optionally above execution-core adapters.
  - `core_sim_trace` owns reusable trace vocabulary for `core_sim` outcomes, while app-specific solver/entity/world lanes stay app-owned.
  - Governance rule: do not flatten `core_kernel`, `core_wake`, `core_workers`, `core_sim`, and `core_sim_trace` into one "runtime owner". Execution-core primitives own infrastructure lifecycle; `core_sim` owns simulation control-plane cadence; `core_sim_trace` owns only the shared trace vocabulary above completed `core_sim` outcomes.

- Theme/font:
  - Preset and token source of truth must stay in `core_theme` / `core_font`.
  - App adapters map tokenized values into local UI draw calls.

## Anti-Overlap Rules

- Do not duplicate generic math helpers in app code if `core_math` already owns them.
- Do not place SDL input routing, mouse-wheel policy, or app pane hit-testing in `core_viewport2d`.
- Do not place SDL input routing, projector construction, picking, scene bounds
  resolution, or authoring arbitration in `core_viewport3d`.
- Do not place projection, occlusion, selection state, pointer routing, gizmo/
  topology/face picking, or renderer policy in `core_screen_pick`.
- Do not place scene-schema types in app-specific UI/render modules.
- Do not place authored-texture manifest field meaning in app-local exporter/loader code once `core_authored_texture` is adopted.
- Do not add compiler include emulation behavior to runtime core libs.
- Do not hardcode theme/font constants in app UI where adapter lookup exists.
- Do not place pane geometry solve or hit-testing semantics in `core_layout`.
- Do not add persistence/file-IO behavior to `core_config`.
- Do not couple `core_action` to platform keycode parsing or UI command widgets.

## 2026-10-05 pane composition foundation

Accepted shared source `86037d7` supplies `kit_pane 0.4.0` and `kit_ui 0.17.0`.
The trio Main Edit adopts stable pane identities, viewport/content clipping and
small host adapters. Orchestra uses shared geometry for leaf chrome/content and
authoring hover; Echo uses navigation/detail/graph clips and pane pointer capture;
DataLab uses picker pane scopes and shared list paint/hit clipping. Core pane,
module, layout transaction and snapshot responsibilities remain unchanged.
Mixed field/button focus order is available as a tested optional primitive;
product mixed-order wiring remains a follow-on. Native SDL caret anchoring is
adopted in the existing trio editable fields. Real macOS SDL window/session tests
qualify the adapter; human IME candidate/commit/cancel and native Linux/Windows
acceptance remain separate. Existing backend versions remain kit_render 0.14.6,
vk_renderer 1.5.0 and vk_runtime 0.6.0; no mutable renderer work was imported.
See [pane composition contract](UI_PANE_COMPOSITION_CONTRACT.md).
Next priority is a reusable pane-host composition/dispatch lifecycle with explicit
splitter transactions and focus takeover, followed by per-program adoption.
Canonical source, app VERSION, stable Desktop bundles and releases are unchanged.

## 2026-10-05 pane host behavior adoption

Accepted shared source `ddc9fee6e17482dcd64cf777d7a105b7ed9b157d` adds `kit_pane 0.5.0`, with
`core_layout 0.2.1` supplying revisions and existing authoring transactions.
The generic pane host adds stable mount/unmount/resize dispatch, pointer ownership,
pane focus invalidation, and takeover cancellation. Drag-sized edits nest inside
an existing authoring draft; hosts restore their own topology/ratios on cancel,
retain domain actions/history, and persist only accepted changes. Shared bounded
header slots reserve title space and register only visible actions through the
existing kit_ui surface. Header labels use the existing centered button painter.
No rendering backend is replaced: kit_render 0.14.6, vk_renderer 1.5.0,
vk_runtime 0.6.0 and kit_ui 0.17.0 remain at their accepted versions.

Orchestra wraps its existing snap/rewrite splitter controller and adds a MODULE
header slot opening the existing picker. Echo isolates nested metadata,
relationships and body input/paint clips, fixes parent-span ratio clamping,
commits preferences on accepted release, restores all four ratios on Escape,
focus loss or takeover, and adds a GRAPH-header REFRESH action. DataLab keeps its
actual viewer canvas with source-control/header overlays, gives those regions
stable ownership and SDL clipping, uses a RECENT DIRECTORIES header slot, and
wraps its existing authoring projection drag in a nested layout transaction.
DataLab's fixed authoring projection remains a projection; this does not turn
all profile viewers into a generic movable pane tree.

The proving scope remains the three retained Main Edit lanes. Canonical program
source and VERSION, production bundles, release/Registry and remote hosts are
unchanged. Fullscreen lifecycle qualification is next; docking, generalized
pane provider insertion/persistence, product-wide mixed field/button traversal,
human OS IME candidate/commit/cancel acceptance, native Linux/Windows and other
programs remain separate. See the pane host contract and migration guide.

## 2026-10-05 fullscreen/window lifecycle candidate

The proving-trio slice uses committed `kit_ui 0.18.0` optional SDL window observation/mapping and `vk_renderer 1.6.0` fence/acquire/recovery corrections. Hosts retain event loops, window lifetimes, domain cancellation and persistence; `vk_runtime 0.6.0`, `kit_render 0.14.6` and `kit_pane 0.5.0` retain their responsibilities. The shared unit and macOS native standalone lifecycle/retained-texture gates pass; per-program adoption requires its own actual-loop/native proof. See `docs/UI_WINDOW_LIFECYCLE_CONTRACT.md`. The accepted branch is `codex/ui-window-lifecycle-20261005`, isolated from unrelated uncommitted shared renderer/mesh work; version equality alone is insufficient, so exact source pins remain required.
