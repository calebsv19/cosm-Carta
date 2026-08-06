# Shared Lib Ownership Boundaries

This document defines what each shared library owns so behavior does not overlap.

## Core Libs

- `core_base`: error/result types, common primitives, shared low-level utilities.
- `core_io`: filesystem/text/binary IO helpers and load/save boundaries.
- `core_data`: structured in-memory data containers and typed table/object model.
- `core_memdb`: durable memory database connection, query, and migration boundary (scaffolded).
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
- `core_scene_compile`: shared authoring-to-runtime scene compile and normalization boundary.
- `core_scene_view`: renderer-free scene-view packet schema/readback vocabulary, including preview quality, degraded reason, display flags, pick ids, compact JSON readback validation, and compact summary derivation from validated readback metadata.
- `core_mesh_preview`: viewport-safe runtime mesh preview sidecar contract,
  bounded feature-edge payload generation, local bounds/source-count metadata,
  file-backed preview save/load helpers, and renderer-neutral coherent indexed
  LOD construction with host-selected triangle budgets.
- `core_space`: coordinate-space mapping, transforms, and grid/window/world conversion.
- `core_viewport2d`: renderer-agnostic 2D viewport/camera state transitions for fit-to-window, screen/content transforms, drag pan, and cursor-anchor zoom.
- `core_viewport3d`: renderer-agnostic double-precision 3D viewport target,
  canonical orientation/basis, camera-basis pan, anchor zoom, orbit,
  frame/reset/resize transitions, and projected-extents fit-scale math. It
  does not own host camera storage, projector matrices, input, picking,
  rendering, or authoring policy.
- `core_units`: unit vocabulary, unit conversions, and world-scale conversion primitives.
- `core_object`: app-neutral object identity/transform/dimensional-mode validation primitives.
- `core_authored_texture`: authored-texture manifest semantics, binding/output vocabulary, supported primitive/face-role vocabulary, and manifest-contract validation primitives.
- `core_pack`: versioned chunked interchange container (`.pack`).
- `core_layout`: renderer-agnostic layout transaction state (runtime/authoring mode, apply/cancel, revision/rebuild flags).
- `core_config`: lightweight typed runtime configuration table boundary.
- `core_action`: action identity + trigger-binding registry boundary.
- `core_headless_job`: shared outer headless job-envelope/report semantics and validation.
- `core_pane_module`: renderer-agnostic pane-module descriptor registry and binding validation semantics.
- `core_trace`: trace capture/ingest/export primitives.
- `core_sim`: UI-free simulation control-plane semantics for fixed-step accumulation, pause/play/single-step state, max-tick clamping, ordered pass execution, and deterministic frame outcomes.
- `core_sim_trace`: optional `core_sim` to `core_trace` adapter for shared simulation control-plane trace lanes and frame/reason markers.
- `core_pane`: renderer-agnostic pane tree layout semantics (split ratios, constraints, splitter hit/drag math). It does not own app snapshot selection, session fallback policy, or host build dependency hygiene around those structs.
- `core_theme`: tokenized color + spacing presets.
- `core_font`: font roles + font tier/size preset contracts.

## Kit Libs

- `kit_render`: shared render command vocabulary, frame-recording/submission contract, backend attach/adopt boundary, shared theme/font/text policy resolution, and renderer-adjacent external text helpers. It does not own widget behavior, pane semantics, host event loops/window lifetimes, persistence, or app-local layout/cursor policy.
- `kit_ui`: shared immediate-mode widget expression, reusable button/state/style semantics, HUD button-row/readout layout, alpha-aware floating HUD style fields, nested corner/inset math, and optional SDL rounded-surface draw adapters. It does not own app action dispatch, playback/session policy, active theme persistence, event loops, retained focus, pane topology, or renderer lifecycle.
- `kit_viz`: visualization-specific helpers layered on top of core contracts.
- `kit_viewport3d`: optional renderer-neutral 3D viewport presentation helpers
  for semantic object-outline palettes plus CPU color/depth/owner-buffer
  silhouette, depth-discontinuity, and object-boundary composition. It does not
  own projection, rasterization, renderer resources, cache lifetime, picking,
  input, scene objects, or overlay visibility policy.
- `kit_workspace_authoring`: host-agnostic authoring interaction glue (entry chord checks, trigger mapping, callback-driven action/text-step adapters) plus host-attach contract guidance for theme/font state handoff and top-level shell parity expectations.

## Non-Core Shared Modules

- `vk_renderer`: renderer backend implementation details and Vulkan/SDL bridge.
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
  - Use `core_memdb` as the shared durable queryable memory state boundary as implementation fills in.
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
- Do not place scene-schema types in app-specific UI/render modules.
- Do not place authored-texture manifest field meaning in app-local exporter/loader code once `core_authored_texture` is adopted.
- Do not add compiler include emulation behavior to runtime core libs.
- Do not hardcode theme/font constants in app UI where adapter lookup exists.
- Do not place pane geometry solve or hit-testing semantics in `core_layout`.
- Do not add persistence/file-IO behavior to `core_config`.
- Do not couple `core_action` to platform keycode parsing or UI command widgets.
