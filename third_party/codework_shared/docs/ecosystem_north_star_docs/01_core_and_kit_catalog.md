# Ecosystem Core & Kit Catalog
## Purpose
This document is the canonical catalog of shared ecosystem components.
It describes what exists now, what is planned next, and how responsibilities stay separated.

This is not an implementation guide.

---

## Design Law
**Core defines meaning. Kits define expression. Apps define purpose.**

---

## Core Libraries (UI-free, stable, foundational)

### core_base (FOUNDATIONAL)
**Role:** Lowest-level shared utilities.
**Responsibilities:**
- Memory allocation wrappers
- String slices and owned strings
- Error/result types
- Hashing and IDs
- Path helpers
- Endianness/platform helpers

**Boundary:**
- No app domain concepts
- No serialization formats
- No rendering/UI

---

### core_io (FOUNDATIONAL)
**Role:** Unified byte and file IO abstractions.
**Responsibilities:**
- File read/write helpers
- Stream reader/writer callbacks
- Buffer ownership/lifecycle helpers

**Boundary:**
- Bytes only
- No schema/data semantics
- No rendering/UI

---

### core_data (CRITICAL)
**Role:** Canonical in-memory data representation.
**Responsibilities:**
- Scalars, arrays, typed tables
- 2D scalar/vector fields
- Typed metadata dictionary
- Dataset container APIs

**Boundary:**
- In-memory structure and meaning only
- No on-disk container policy

---

### core_pack (CRITICAL)
**Role:** Versioned binary interchange container (`.pack`).
**Responsibilities:**
- Chunked read/write APIs
- Version and compatibility handling
- Partial reads and indexed lookup
- Optional codec hooks

**Boundary:**
- Transport/container only
- No app-specific scene/layout semantics

---

### core_scene (ACTIVE)
**Role:** Shared typed scene contract owner plus scene-handoff resolver for cross-program imports.
**Responsibilities:**
- Shared scene-root metadata contract helpers
- Shared object-kind vocabulary and first primitive payload semantics
- Validation helpers for canonical authoring object contracts
- `scene_bundle.json` path/type resolution
- Source detection (`manifest`, `.pack`, `.vf2d`)
- Optional bundle metadata path resolution

**Boundary:**
- Owns app-agnostic scene structure/schema and first-class object semantics
- Resolves *what to load*
- Does not decide placement math, rendering behavior, or app-specific overlay policy

---

### core_scene_compile (BOOTSTRAP)
**Role:** Shared compile boundary between scene authoring and runtime handoff.
**Responsibilities:**
- Compile `scene_authoring_v1` payloads into `scene_runtime_v1`
- Emit deterministic runtime envelope order and compile metadata
- Bind compiler/normalization versions and SHA-256 authoring/dependency inputs
- Publish create-only authoring/runtime/receipt bundles, including an optional
  app-owned package entrypoint, through one atomic directory rename
- Derive and materialize content-addressed dependency payload paths inside the
  atomic publication transaction
- Verify package-entrypoint and dependency-payload bytes, digests, and no-follow containment
- Validate canonical primitive payloads for known primitive object kinds
- Preserve unknown extension namespaces while stripping authoring-only lanes from runtime contract

**Boundary:**
- No app UI/editor behavior
- No renderer/solver policy ownership
- No app-specific override semantics
- No dependency discovery, dependency-kind interpretation, or
  simulation/render promotion policy

---

### core_scene_view (BOOTSTRAP)
**Role:** Shared renderer-free scene-view packet vocabulary and readback
contract.
**Responsibilities:**
- Scene-view schema family and packet variant constants
- Preview quality, degraded reason, display flag, and pick-id vocabulary
- Compact JSON readback validation for packet metadata
- Shared readback structs for producer/consumer fixture parity
- Compact summary derivation from validated packet readback metadata

**Boundary:**
- Owns packet meaning only
- Does not own rendering, viewport input, picking policy, material sampling,
  editor mutation, or app face/object mapping
- Does not create, delete, move, resize, or rewrite scene objects
- Producers and consumers keep app-local rendering/editor behavior until more
  hosts prove the contract

---

### core_mesh_asset (BOOTSTRAP)
**Role:** Shared reusable mesh-asset contract owner.
**Responsibilities:**
- `mesh_asset_authoring_v1` contract helpers
- `mesh_asset_runtime_v1` contract helpers
- Primitive-seed authoring payload validation
- Imported-mesh/STL source metadata validation
- Runtime vertex, triangle, surface-group, bounds, and topology validation
- Runtime mesh asset JSON load/save helpers

**Boundary:**
- Owns reusable object and runtime mesh asset meaning
- Does not own scene placement, editing UX, triangulation, render acceleration,
  solver voxelization, collision proxies, or SDF generation

---

### core_mesh_compile (BOOTSTRAP)
**Role:** Shared compile-boundary owner for mesh asset authoring to runtime mesh handoff.
**Responsibilities:**
- Staged `mesh_asset_instance` scene-reference helpers
- Authored-source compile responsibility validation
- Bounded ASCII and binary STL imported-mesh to runtime-mesh compile proof
  with a `1000000` triangle proof-scale ceiling
- File-backed runtime mesh output for bounded imported-mesh proofs
- Runtime mesh emission contract flags
- Surface-group preservation contract
- Imported-mesh source metadata requirement checks

**Boundary:**
- Owns app-neutral compile responsibility semantics only
- Owns bounded STL parsing, triangle-count rejection, and indexed weld lookup
  for imported-mesh compile proofs
- Does not own mesh repair, retopo, LOD/streaming, host import UX, scene
  envelopes, render meshes, or solver runtime-form derivation

---

### core_mesh_preview (BOOTSTRAP)
**Role:** Shared viewport-safe runtime mesh preview contract owner.
**Responsibilities:**
- `core_mesh_preview_runtime_v1` sidecar payload helpers
- Bounded feature-edge preview generation from `mesh_asset_runtime_v1`
- Runtime mesh preview metadata for source counts, local bounds, source asset
  ids, preview mode, and sampled drawable edge payloads
- Renderer-neutral coherent indexed LOD construction with caller-selected
  triangle budgets
- File-backed preview save/load helpers

**Boundary:**
- Owns app-neutral preview data meaning and bounded payload generation
- Does not own UI selection, hitboxes, camera projection, renderer state,
  scene placement, mesh repair, retopo, GPU buffers, BVHs, solver proxies,
  collision meshes, or SDF generation
- Proven consumers: LineDrawing retains its CPU depth-raster/cache policy;
  RayTracing retains its native Vulkan editor rendering, mode controls,
  picking, overlays, final geometry, and BVH authority.

---

### core_space (ACTIVE)
**Role:** Shared spatial conversion contract.
**Responsibilities:**
- Unit-to-world/world-to-unit transforms
- Import fit/scale normalization
- Author-window span conversion
- Canonical right-handed Z-up meter-frame meaning
- Validated legacy right-handed Y-up conversion for vectors, orientation
  matrices, quaternions, planes, AABBs, and axis-aligned half extents

**Boundary:**
- Defines *where/how to place*
- Owns app-neutral coordinate-frame meaning and rigid conversion only
- Does not parse scene bundles or own asset formats
- Does not own projection, cameras, renderer/solver interpretation,
  persistence, or application frame-selection policy

---

### core_viewport2d (BOOTSTRAP)
**Role:** Shared 2D viewport/camera interaction math contract.
**Responsibilities:**
- Screen-to-content/content-to-screen transforms
- Fit-to-window reset for oversized or undersized 2D content
- Screen-space pan deltas and cursor-anchor zoom state updates

**Boundary:**
- Owns pure 2D viewport state and transform math only
- No SDL/input event ownership
- No map projection semantics, scene import placement, or renderer backend coupling

---

### core_viewport3d (BOOTSTRAP)
**Role:** Shared renderer-neutral 3D editor viewport navigation contract.
**Responsibilities:**
- Double-precision effective viewport-center target and scale state
- Canonical radian orientation and camera right/screen-down/forward basis
- Camera-basis pan, anchor-preserving zoom, orbit, frame, reset, and resize transitions
- Projected-extents fit-scale resolution with host-selected padding
- Invalid-input rejection without output mutation

**Boundary:**
- Depends only on `core_base`; uses a domain-specific double vector ABI
- No SDL/input, projector matrices, selection/bounds resolution, picking,
  rendering, authoring, persistence, geometry, or BVH ownership
- RayTracing and LineDrawing remain responsible for thin runtime adapters

---

### core_screen_pick (BOOTSTRAP)
**Role:** Shared renderer-neutral projected-origin selection index.
**Responsibilities:**
- Uniform hashed-grid storage for viewport-local logical-pixel candidates
- Deterministic nearest and bounded ranked queries
- Default 28-pixel capture radius with distance, frontmost-depth, and stable-key ordering
- Transactional rebuilds that preserve the prior valid index on invalid input or allocation failure

**Boundary:**
- Depends only on `core_base`; callers own projection and candidate eligibility
- No SDL/input, camera, visibility/occlusion, scene identity, selection state,
  handles/gizmos, topology/face picking, dragging, rendering, or authoring ownership
- LineDrawing, RayTracing, and PhysicsSim retain thin adapters and their existing
  higher-priority interaction arbitration

---

### core_units (BOOTSTRAP)
**Role:** Canonical unit vocabulary and conversion contract.
**Responsibilities:**
- Unit identifiers and parser helpers
- Unit-to-unit conversion helpers
- World-scale conversion helpers for scene/object interchange

**Boundary:**
- No scene schema ownership
- No app-specific policy
- No rendering/UI coupling

---

### core_object (BOOTSTRAP)
**Role:** App-neutral object identity and transform contract.
**Responsibilities:**
- Stable object identity/type metadata
- Dimensional mode contract (`plane_locked`/`full_3d`)
- Transform + basic object flag validation helpers

**Boundary:**
- No full scene container ownership (`core_scene` owns scene envelope)
- No app namespace overlay ownership
- No solver/render runtime ownership

---

### core_authored_texture (BOOTSTRAP)
**Role:** Shared authored-texture manifest and indexed-atlas contract owner for cross-app texture export/runtime handoff.
**Responsibilities:**
- Authored-texture schema-version vocabulary
- Binding-kind and emitted-output-kind vocabulary
- Supported primitive and face-role vocabulary
- Primitive-specific face completeness rules
- JSON-free manifest-contract validation helpers
- Exact indexed source-slot and palette-entry validation
- Fixed-size atlas-cell rectangle, identifier, output-kind, bounds, overlap,
  lookup, and revision semantics

**Boundary:**
- Owns authored-texture manifest and generic indexed-interchange meaning only
- No JSON parsing or file/image IO
- No scene-envelope ownership (`core_scene` owns scene/object semantics)
- No renderer/editor/runtime UI behavior

---

### core_pane (BOOTSTRAP)
**Role:** Shared split-pane geometry and interaction semantics.
**Responsibilities (initial):**
- Pane-tree split solve into leaf rectangles
- Ratio normalization with min-size constraints
- Splitter hit-test metadata
- Splitter drag ratio updates without renderer coupling

**Boundary:**
- Pane semantics only
- No renderer/UI framework dependencies
- No app-specific pane policy or preset file parsing

---

### core_layout (BOOTSTRAP)
**Role:** Shared workspace-layout transaction semantics.
**Responsibilities (initial):**
- Runtime/authoring mode state token
- Draft/active revision tracking
- Apply/cancel transaction lifecycle
- Rebuild-intent signaling after apply

**Boundary:**
- No pane geometry solve or splitter math (`core_pane` owns that)
- No module-host lifecycle policy
- No rendering/UI dependencies

---

### core_pane_module (BOOTSTRAP)
**Role:** Shared pane-module descriptor registry and binding validation semantics.
**Responsibilities (initial):**
- Register internal module descriptors with stable identities
- Validate descriptor capability-to-hook compatibility
- Validate pane-to-module bindings against known leaf pane IDs and registry entries

**Boundary:**
- No plugin binary loading or sandboxing
- No host runtime loop ownership
- No arbitrary key/value module config persistence in v1

---

### core_pane_snapshot (BOOTSTRAP)
**Role:** Shared pane snapshot schema struct and validation semantics.
**Responsibilities (initial):**
- Define v1 snapshot metadata/node/binding record structs
- Validate snapshot schema/meta fields before host import
- Validate pane graph and module-binding invariants deterministically

**Boundary:**
- No `core_pack` read/write ownership
- No JSON serializer ownership
- No host runtime/layout mutation policy ownership

---

### core_config (BOOTSTRAP)
**Role:** Lightweight typed runtime configuration table.
**Responsibilities (initial):**
- Fixed-capacity key/value storage
- Typed scalar values (`bool`, `int64`, `double`, `string`)
- Deterministic upsert/read semantics

**Boundary:**
- No schema graph modeling (`core_data` owns rich structures)
- No file/persistence policy (`core_pack`/app policy own that)
- No action routing semantics

---

### core_headless_job (BOOTSTRAP)
**Role:** Shared outer job, workflow, event, result, artifact, and
worker-capability semantic contract for cross-program compute.
**Responsibilities:**
- Backward-compatible bundle/report vocabulary and typed records
- Platform-v1 job, append-only event, terminal result, content/provenance
  artifact, ordered workflow, and worker-capability vocabulary
- Closed job-state, event-kind, outcome, and transition semantics
- JSON-free validation for identity, relative paths, UTC timestamps, SHA-256,
  workflow order, attempt/claim/lease references, and capability records

**Boundary:**
- Owns only cross-program execution-contract meaning
- No JSON parsing/writing, filesystem creation, coordinator persistence,
  transport, or scheduler dispatch
- No program-specific scene payload semantics

---

### core_action (BOOTSTRAP)
**Role:** Action identity and trigger-binding registry.
**Responsibilities (initial):**
- Register action metadata (`id`, `label`)
- Bind trigger tokens to action IDs
- Resolve trigger to stable action identity

**Boundary:**
- No platform keycode decoding (adapter/app layer owns that)
- No command execution/runtime policy
- No UI command palette rendering concerns

---

### core_trace (ACTIVE)
**Role:** Shared timeline/event instrumentation foundation.
**Responsibilities:**
- Time-series samples and markers
- Session lifecycle/finalization
- Trace pack import/export contract

**Boundary:**
- Diagnostics/timeline semantics only
- No UI or renderer coupling

---

### core_sim_trace (BOOTSTRAP)
**Role:** Optional `core_sim` to `core_trace` control-plane adapter.
**Responsibilities (initial):**
- Standard `core_sim.*` trace lanes for frame index, frame dt, tick count,
  pass count, reason bits, accumulator, and advanced simulation time
- Standard frame/reason markers for tick, render, clamp, single-step, and pass
  failure outcomes
- Headless/agent-analysis vocabulary that simulation hosts can reuse before
  adding app-specific domain lanes

**Boundary:**
- Does not add a `core_trace` dependency to base `core_sim`
- Does not own app entity/world snapshots, solver metrics, replay execution,
  UI, `core_data`, or `core_pack`
- Apps add domain-specific lanes beside the shared control-plane lanes

---

### core_sim (BOOTSTRAP)
**Role:** Shared simulation control-plane foundation.
**Responsibilities (initial):**
- Fixed-step accumulator policy
- Pause/play/single-step control state
- Max ticks per frame clamp
- Ordered simulation pass execution
- Deterministic per-frame outcome reporting
- Host adapter diagnostics: status names, pass-order validation, pass-outcome initialization, and frame reason bits
- UI-free frame summaries, reason-name extraction, and stage-timing derivation for optional diagnostics/artifact adapters
- Step 3 artifact records: public version string, deterministic pass-order hash,
  artifact run-header initialization, and frame-record extraction

**Boundary:**
- Owns simulation orchestration semantics only
- Does not own physics equations, entity/world storage, scenario formats, rendering, UI, platform input, or worker/job/scheduler ownership
- Current proving-host shapes are fixed-step (`gravity_orbit_sim`), entity/group pass order (`behavior_sim`), solver/substep (`physics_sim`), and progressive/render-frame orchestration (`ray_tracing`)
- May layer with `core_time`, `core_kernel`, `core_sched`, `core_jobs`, `core_workers`, `core_queue`, `core_wake`, and optional sibling adapters such as `core_sim_trace`

---

### core_memdb (ACTIVE)
**Role:** Shared durable memory database contract.
**Responsibilities:**
- SQLite-backed connection lifecycle
- Query/statement execution helpers
- Schema version tracking and migration entrypoints
- Durable storage boundary for future memory tooling and console flows

**Boundary:**
- Storage/query contract only
- No UI, graph rendering, or app-specific memory curation policy
- Higher-level console behavior stays in kits/apps layered above this core

**Current status note:**
- The active SQLite/event implementation lives at `shared/core/core_memdb/`; current module version is `0.31.1`. Budget-rejected link mutations preserve readable source-item audit metadata without emitting mutation events or changing the link projection.
- The CLI and agent wrapper provide bounded scoped retrieval, event-first mutation/replay, graph links, stable-id-only maintained projections, and atomic Lane Head V1 handoffs.
- Design, operating, and rollout docs live under `shared/docs/memory_db_system/`.

---

### core_math (BOOTSTRAP)
**Role:** Shared numerical primitives layer.
**Responsibilities (initial):**
- Core vector operations
- Basic geometric helper operations
- Deterministic math helpers used by multiple cores/kits

**Boundary:**
- Generic math primitives only
- Scene contracts stay in `core_scene`
- Import/world placement policy stays in `core_space`

---

### core_collision2d (BOOTSTRAP)
**Role:** Shared UI-free 2D collision contract.
**Responsibilities:**
- Double-precision 2D vectors and AABBs
- Circle, axis-aligned box, and convex polygon descriptors
- Polygon geometry helpers
- Contact manifold records
- Primitive contact generation for circle/circle, axis-aligned box/box, and
  convex polygon/polygon

**Boundary:**
- Owns app-neutral collision shape/query semantics only
- Does not own rigid-body integration, impulse solving, mass/inertia, or
  fixed-step cadence
- Does not own room/floor/wall convenience contacts, named fixtures, summary
  strings, CLI routes, visual review artifacts, workers, packages, or runtime
  default policy

---

### core_rigid2d (BOOTSTRAP)
**Role:** Shared UI-free 2D rigid-body contract layered on `core_collision2d`.
**Responsibilities:**
- Material records and rigid-body state descriptors
- Shape-based mass and inertia helpers
- Dynamic/static body initialization and validation
- Minimal host-called body integration
- Deterministic normal, angular, and friction contact solver primitives
- Standalone typed parity harness over the first Ball Bounce rigid-body and
  solver oracle values

**Boundary:**
- Owns rigid-body state and contact response primitives only
- Depends on `core_collision2d` for vectors, shapes, and manifolds
- Does not own fixed-step accumulation, broadphase/contact discovery, worlds,
  scenarios, summary strings, CLI routes, visual review artifacts, workers,
  packages, or runtime default policy

---

### core_theme (BOOTSTRAP)
**Role:** Canonical semantic UI token model and theme preset registry.
**Responsibilities:**
- Semantic color tokens (surface/text/accent/status) without renderer coupling
- Spacing/radius/scale token groups for consistent layout behavior
- Preset identifiers and override merge rules (`daw_default`, `ide_gray`, `light`, `dark`)
- Serialization/deserialization of theme token bundles

**Boundary:**
- Data contract only; no SDL/Vulkan drawing code
- No widget or layout behavior ownership
- No font file IO ownership

---

### core_font (BOOTSTRAP)
**Role:** Font family/token registry and fallback policy contract.
**Responsibilities:**
- Font family IDs, style/weight tokens, and fallback chain resolution
- Shared font manifest parsing and license metadata exposure
- Runtime selection contract (`ui`, `mono`, `display`) independent of renderer
- Pack/basic-pack descriptors for optional asset sync/install tooling

**Boundary:**
- Registry and policy only; no renderer text draw code
- No app-specific text layout rules
- No theme color behavior ownership

---

### core_time (EXECUTION CORE, v1.0)
**Role:** Canonical monotonic time contract.
**Responsibilities:**
- Monotonic timestamp reads
- Duration compare/add/diff helpers
- Time conversion helpers for runtime/trace alignment

**Boundary:**
- No sleeping or timers
- No scheduler or threading logic

---

### core_queue (EXECUTION CORE, v1.0)
**Role:** Queue primitives for runtime data passing.
**Responsibilities:**
- Bounded SPSC-style ring queue baseline
- Mutex-based queue baseline for cross-thread paths
- Explicit ownership and capacity semantics

**Boundary:**
- No workers, scheduling, or wake policy
- No job semantics

---

### core_sched (EXECUTION CORE, v1.0)
**Role:** Non-blocking deadline timer scheduler.
**Responsibilities:**
- Register/cancel one-shot and repeating timers
- Query next deadline
- Fire due timers from caller-driven loop

**Boundary:**
- No sleep/block behavior
- No thread or wake logic

---

### core_jobs (EXECUTION CORE, v1.0)
**Role:** Main-thread job queue with budgeted execution.
**Responsibilities:**
- Enqueue function/context jobs
- Run jobs up to per-tick budget
- Emit queue execution statistics

**Boundary:**
- No worker thread ownership
- No implicit blocking or sleep behavior

---

### core_workers (EXECUTION CORE, v1.0)
**Role:** Fixed-size worker pool abstraction.
**Responsibilities:**
- Initialize/join bounded worker set
- Submit background tasks
- Push completion messages through queue boundary

**Boundary:**
- No UI/shared-state mutation policy ownership
- No advanced scheduler policy (work-stealing, etc.)

---

### core_wake (EXECUTION CORE, v1.0)
**Role:** Cross-thread wake signaling abstraction.
**Responsibilities:**
- Wait/signal API independent from UI frameworks
- Condvar backend for headless paths
- External backend hooks for adapter-driven GUI wake

**Boundary:**
- No kernel policy ownership
- No direct UI framework dependency in core contract

---

### core_kernel (EXECUTION CORE, v1.0)
**Role:** Runtime orchestration spine above execution cores.
**Responsibilities:**
- Policy-driven loop phase order
- Module lifecycle callbacks
- Timer/job/worker/wake orchestration

**Boundary:**
- No renderer/UI ownership
- No app-domain behavior ownership

---

## Execution Core Build Order
1. `core_time`
2. `core_queue`
3. `core_sched`
4. `core_jobs`
5. `core_workers`
6. `core_wake`
7. `core_kernel`

---

## Kit Libraries (Optional, UI-capable)

### kit_viz (CRITICAL)
**Role:** Visualization helper kit.
**Responsibilities:**
- Field stats and heatmap conversion
- Vector/polyline segment builders
- Waveform envelope sampling helpers

**Boundary:**
- Presentation helpers only
- Depends on core contracts

---

### kit_render (ACTIVE)
**Role:** Rendering abstraction (`vk_renderer` seed path).
**Notes:**
- Owns backend-agnostic command-frame recording/submission, backend attach/adopt
  boundaries, shared theme/font text policy resolution, and renderer-adjacent
  external text helpers.
- Does not own widget behavior, app hit testing, event loops, persistence, or
  optional plain-SDL UI bridge drawing.

### kit_ui (ACTIVE)
**Role:** Shared UI primitives (`timer_hud` seed path).
**Notes:**
- Primary consumer of `core_theme` and `core_font` contracts.
- Owns immediate-mode layout, evaluation, and draw helpers layered onto
  `kit_render`; it does not own renderer lifecycle.
- Current shared controls include buttons, checkboxes, sliders, scrollbars,
  segmented selectors, rounded/compact button appearances, HUD button-row
  layout, alpha-aware HUD style fields, nested corner/inset math, and optional
  SDL rounded-surface adapters for plain SDL hosts.
- App-specific actions, playback/session behavior, active theme persistence,
  and host text/cache ownership stay app-owned.

### kit_pane (BOOTSTRAP)
**Role:** Shared pane-shell presentation kit for `core_pane`-driven workspaces.
**Notes:**
- Owns pane chrome visuals (border/header/title/splitter states).
- Owns authoring-mode structural overlay affordances.
- Does not own pane topology solve, module lifecycle, workspace persistence policy, or app build/dependency hygiene needed to keep pane/session struct changes coherently rebuilt.

### kit_workspace_authoring (BOOTSTRAP)
**Role:** Shared host-agnostic authoring interaction glue kit.
**Notes:**
- Owns entry-chord and trigger mapping helpers for authoring/runtime mode routes.
- Owns callback-driven action execution + text-size step adapter helpers.
- Publishes host-attach guidance for theme preset + text zoom handoff, including top-level picker/shell parity requirements so theme changes are visible outside authoring overlay.
- Does not own renderer/window policy, module-picker model ownership, or pane topology semantics.

### kit_runtime_diag (BOOTSTRAP)
**Role:** Shared runtime diagnostics contract helpers.
**Notes:**
- Owns app-neutral frame-stage timing math helpers.
- Owns app-neutral input-frame cumulative totals helpers.
- Does not own program input policy, routing behavior, or render behavior.

### kit_viewport3d (BOOTSTRAP)
**Role:** Optional renderer-neutral 3D editor viewport presentation helpers.
**Notes:**
- Owns semantic object-outline palette roles and CPU buffer composition for
  silhouettes, relative depth discontinuities, and object-owner boundaries.
- Accepts borrowed float or double depth buffers and optional object-owner
  buffers; supports filled-surface and outline-only composition.
- Does not own `core_viewport3d` navigation state, projection, rasterization,
  renderer/GPU resources, cache lifetime, picking, scene state, input, or
  app overlay visibility policy.
- RayTracing and LineDrawing are the first source-level proving hosts.

### kit_graph (ACTIVE)
**Role:** Shared graph visualization kit.
**Notes:**
- Initial implementation path starts with `shared/kit/kit_graph_timeseries`.
- `kit_graph_timeseries` now includes dense-series stride guidance and reusable hover overlays.
- `kit_graph_struct` now provides layered tree and DAG layouts plus viewport focus helpers.
- Owns reusable plotting and later structural graph presentation, not app-specific graph meaning.

### kit_audio (PLANNED)
**Role:** Shared audio routing and DSP glue.

### kit_sim (PLANNED)
**Role:** Shared simulation-loop and replay helpers.
**Notes:**
- Should remain a kit-level optional layer above `core_sim` for diagnostics,
  replay presentation, scaffold examples, and tooling helpers.
- Should not own the core loop ABI or pull renderer/UI dependencies into
  `core_sim`.

---

## Shared Infrastructure (Non-core / Non-kit)

### vk_runtime (ACTIVE FOUNDATION)
**Role:** SDL-independent Vulkan device/runtime foundation.
**Responsibilities (S1-S4 plus platform portability):**
- Vulkan loader and API negotiation
- Portability enumeration and validation/debug policy
- Headless or staged presentation physical-device, queue-family, feature,
  extension, memory, driver, and subgroup discovery
- Explainable graphics/compute/transfer/present queue-role selection
- Headless or surface-bound logical-device creation
- Deterministic `codework_gpu_capability_report_v1` JSON
- Canonical `VERSION`-derived library and report identity
- Typed runtime and per-device rejection diagnostics
- One-shot host-visible storage-buffer allocation and mapping
- Sequential storage-buffer descriptor and compute-pipeline creation
- Command recording, bounded fence wait, readback, and reverse teardown
- Versioned shader identity and deterministic CPU/GPU parity evidence
- Explicit coherent staging and device-local buffer roles
- Persistent buffer, descriptor/pipeline, command-pool/buffer, and fence state
- Dependent dispatch chains with explicit compute memory barriers
- Final-only readback, bounded timeout recovery, and resource accounting
- Persistent timestamp query pairs on timestamp-capable compute queues
- Valid-bit wrap handling and conversion through device timestamp period
- Separated CPU, host-copy, submit/wait, GPU, transfer, and wall timing
- Seven-size deterministic parity sweep and explicit crossover evidence
- Deterministic export and explicit consumption of source-bound precompiled
  SPIR-V for compiler-free proof hosts
- Prebuilt-bundle identity, safe-path, and tamper validation before dispatch
- Compiler-free Linux llvmpipe S2-S4 portability evidence

**Boundary:**
- Vulkan-specific non-core infrastructure
- No SDL surface creation, swapchains, presentation, render pipelines, or UI;
  caller-provided Vulkan surfaces are accepted only for present suitability
- No application workload semantics, CPU-oracle ownership, or fallback policy
- No images, semaphores, cross-queue transfers, allocator suballocation,
  application workload policy, or renderer presentation
- No portable speedup guarantee; timing profiles remain device/driver specific
- `vk_renderer 1.3.0` remains the presentation owner and consumes this
  lifecycle through compatibility wrappers; no application rollout yet

### vk_renderer (ACTIVE PRESENTATION INFRASTRUCTURE)
**Role:** SDL/Vulkan presentation backend beneath `kit_render`.
**Responsibilities:**
- SDL window-surface creation and ownership handoff into `vk_runtime`
- Swapchain, render-pass, framebuffer, graphics pipeline, and synchronization
  lifecycle
- Primitive, texture, mesh, and line drawing plus capture/readback
- Compatibility mirrors for existing public Vulkan handles and entry points
- Out-of-date/suboptimal recovery through bounded swapchain recreation

**Boundary:**
- Uses `vk_runtime 0.6.0` for instance/device/queue lifecycle
- Does not own headless compute policy, application drawing semantics, or CPU
  fallback/oracle policy
- Local `1.3.0` proof is not program adoption; each consumer still requires a
  separate shared-subtree and build-link update

---

### sys_shims (BOOTSTRAP)
**Role:** Local shim layer for system include compatibility and controlled extensions.
**Responsibilities:**
- Provide a dedicated namespace for local stdlib wrappers/extensions
- Keep shim API opt-in and dependency-light
- Act as a long-term staging area for your custom language include compatibility

**Boundary:**
- Not a replacement for libc implementation
- Must remain additive and explicit

---

## Applications Built on the Ecosystem
- DataLab
- PhysicsSim
- DAW
- RayTracing
- IDE
- LineDrawing
- MapForge
- fisiCs

## Rounded UI Rendering First Slice (2026-10-04)

`vk_renderer 1.4.0` adds `vk_renderer_fill_rounded_rect(...)`, drawable-scale
coverage geometry, and frame-fence retirement of replaced vertex storage.
`kit_render 0.14.5` honors recorded positive corner radii through that primitive.
`kit_ui 0.11.3` fixes sibling runtime linkage in live harnesses and adds the
bounded `test-rounded-vk` real-image gate; its existing button API is unchanged.
Widget appearance remains kit-owned, renderer resources stay renderer-owned,
and actions/layout/persistence remain app-owned. No UI dependency was added to
core modules. WorkspaceSandbox and MemConsole Main Edit are the first contained
adopters; their canonical/release identities are unchanged.

HUD follow-up shared source `51b331a` supplies `kit_workspace_authoring 0.5.2`.
Its top-level HUD draw helper consumes the existing `kit_ui` compact rounded
appearance/state resolver and measures centered captions with the renderer's
vertical midpoint. Hosts calling this UI helper link `kit_ui` before `kit_render`;
input/runtime-only authoring helpers remain usable without a UI runtime.
WorkspaceSandbox Main Edit adopts it for top-level controls and unifies its
floating HUD and Font/Theme adapter. Other hosts retain their pinned snapshots.

## 2026-10-04 Direct SDL Button Appearance

`kit_ui 0.12.0` adds `kit_ui_sdl_draw_button_spec_appearance` as an optional
SDL expression of the existing button spec/theme/appearance contracts. It
reuses the shared state resolver, nested rounded outline/fill model, and
measured caption callbacks. `test-sdl-appearance` covers 1x/2x pixels, state
precedence, caption coordinates, clipping, narrow controls and invalid inputs.
The generic archive/default tests retain no SDL dependency. DataLab Main Edit
is the first host of this adapter; its product behavior, theme persistence,
event routing, renderer resources and lifecycle stay app-owned.


## Render Fidelity Contract Update (2026-10-04)

`kit_render 0.14.6` validates command-local translation/signed scale and native
texture UV/tint fields before recording and before whole-frame submit. Its
optional Vulkan bridge uses `vk_renderer 1.5.0` floating textured-quad and
draw-transform primitives. The renderer owns GPU drawing/resource/presentation
state; `vk_runtime 0.6.0` remains the device/queue lifecycle owner. There is no
new core library or UI dependency in domain/headless execution.

The common captured-image oracle covers 1x/2x geometry, UV crop/reversal, tint,
clips, text placement, invalid stream repair, buffer growth and resize.
orChestra/eCho Main Edit link their real native archives; DataLab Main Edit
links its native renderer objects in the harness and retains shared SDL UI
appearance. See [the command contract](../RENDER_COMMAND_FIDELITY.md); focus,
keyboard/capture, pane composition, native Linux and wider adoption remain
separate contracts.


## Button interaction cohort (2026-10-04)

Shared `ad3b83b` supplies `kit_ui 0.13.1` and
`kit_workspace_authoring 0.6.1`. The optional caller-owned context standardizes
press-origin capture, visual-order Tab/Shift-Tab focus and Enter/Space activation
on release. Its generic archive remains independent of SDL; the optional SDL
event/marker adapter and shared Font/Theme registration stay in the kits.
Controls have half-open bounds and stable semantic IDs; repeat, cancellation,
hidden/disabled owners, clipped controls and invalid registration are covered.
The context retains no widget tree, actions, labels, layout or persistence.

The retained Main Edit cohort uses separate managed imports and host integrations:
- workspace_sandbox: `d9e07d9` — common Font/Theme authoring controls.
- mem_console: `99f91dd` — common Font/Theme authoring controls.
- datalab: `ea732ad` — common Font/Theme authoring controls.

Each host routes normalized events to the kit and dispatches the returned action
through its existing domain adapter. Apply/Cancel, custom-theme popup/rename,
picker/text entry, coordinates, lifecycle and persistence stay host-owned.
DataLab remains the SDL UI appearance reference; native image/Vulkan presentation
retain their own paths. Normal-runtime HUD/inspector input and pane editing are
not migrated by this slice. Shared and host replays, product/headless regression,
native lifecycle and isolated Main Edit package gates pass on the local Mac.
Native orChestra/eCho captured focus-marker pixels at 1x/2x and the SDL reference
marker/coordinate checks pass. Human interactive acceptance and native Linux
qualification remain distinct.

See [the interaction contract](../UI_INTERACTION_CONTRACT.md). The next slice should
extend this contract to ordinary HUD/inspector controls in the same trio, followed
by bounded text-entry/modal and pane/layout composition. Canonical app source,
app VERSION, stable Desktop bundles and release state remain outside this
Development rollout.


eCho Font/Theme follow-up `f919d51` separates the text-size chip from the
increment control identity and clips its shorter readout before Reset. Full
eCho regression and real panel native focus captures pass again. The shared
module pin remains `ad3b83b`; this small host cleanup adds no shared API or
program version change.


## Complete trio button surface adoption (2026-10-04)

The retained orChestra, eCho and DataLab Main Edit lanes now adopt `kit_ui 0.14.1`
optional semantic surface snapshots across their inventoried runtime, authoring,
modal and auxiliary-picker button surfaces. The shared source pin is `b1c67d7`;
`kit_workspace_authoring 0.6.1`, `kit_render 0.14.6`, `vk_renderer 1.5.0` and
`vk_runtime 0.6.0` retain their preceding contracts.

The snapshot bridges normalized input and immediate drawing: 256 visible controls,
full-width domain/value keys, opaque identity stable across row reordering,
visible clipping, transactional registration and a bounded 32-activation FIFO.
Direct hosts collect per event; drawing-time hosts claim one action per frame and
redraw while pending. Modal scopes remove background targets while swallowing
old owned releases. Hosts own actions, text/caret/IME, label lifetime, coordinates,
pane/scrollbar gestures, domain state and persistence. No Core UI dependency or
mandatory SDL dependency was introduced.

orChestra covers top controls, ingest HUD/root actions/authoring shortcuts,
Font/Theme and module assignment. eCho covers left browse/project/item controls,
graph settings/actions, relationships, legend filters, DB modal and authoring.
DataLab covers playback, Recent roots, common/custom authoring and picker roots/
artifacts. eCho copies transient captions into a UI-frame arena; DataLab remains
the SDL UI reference while native image rendering/Vulkan presentation stay distinct.

Generic/SDL state tests, production-linked host replays, exact accepted-source
and tamper checks, ordinary orChestra/eCho native focus output at 1x/2x with zero
validation messages, and DataLab SDL pixel/output mapping are proved. Product,
lifecycle and package/installed identity gates remain separately reported by each
host. Human workflow review, text-edit/IME and modal-focus composition, pane
composition, native Linux and wider/canonical release rollout are next boundaries.
See [the interaction contract](../UI_INTERACTION_CONTRACT.md) and each host's
`docs/ui_interaction.md`; this Development checkpoint changes no app VERSION.

Accepted-source verification compares vendored module bytes with immutable Git
snapshot bytes. `--require-current-canonical` additionally checks mutable upstream
state for a deliberate adoption audit. Concurrent upstream renderer work is neither
reset nor silently imported into these accepted programs.


## 2026-10-05 bounded text editing and modal focus

`kit_ui 0.15.1` at accepted shared `0cc23aa` provides optional caller-owned bounded UTF-8 editing, Shift selection, clipboard/preedit normalization and one-modal semantic button focus restoration. Orchestra Main Edit adopts ingest input root; Echo Main Edit adopts search/title/body/DB path/edge limit/relationship target; DataLab Main Edit adopts picker root/filter and viewer modal focus. Minimum for these APIs is 0.15.1; no portfolio-wide minimum or renderer change is implied. `kit_workspace_authoring 0.6.1`, `kit_render 0.14.6`, `vk_renderer 1.5.0`, `vk_runtime 0.6.0` remain unchanged. Stable canonical app versions remain outside this Development adoption. Text presentation/native IME, panes and other programs remain incremental follow-ons. See `UI_TEXT_FOCUS_CONTRACT.md` in shared docs.


## 2026-10-05 shared text presentation

`kit_ui 0.16.0` at accepted shared `e469445` supplies optional caller-owned measured text presentation: scalar-safe rows/hit, caret, selection, transient replacement preedit/underline, horizontal caret-follow and multiline hard wrapping. Queued and optional SDL adapters preserve clipping/storage conventions. Orchestra ingest root, Echo six fields and DataLab picker root/filter adopt it in Main Edit. The API minimum is 0.16.0; no portfolio-wide minimum is implied. Companion/render module versions remain unchanged. Core remains domain-owned and hosts retain eligibility, font/viewport, save/cancel/session/persistence policy. Next mixed traversal/native IME acceptance, then panes/wider rollout. See `UI_TEXT_PRESENTATION_CONTRACT.md`.

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
See [pane composition contract](../UI_PANE_COMPOSITION_CONTRACT.md).
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

## Fullscreen/window lifecycle acceptance — 2026-10-05

Accepted shared source `854b51ff57c756b4565021fe759c27a459efbfd3` supplies `kit_ui 0.18.0` optional SDL window
observation, coordinate mapping, F11 desktop-fullscreen and bounded native proof;
`vk_renderer 1.6.0` resets fences only before submission, consumes suboptimal
acquired images and performs bounded out-of-date recovery. Hosts own their loops,
window/resource lifetimes, layouts, domain state, edit restoration and persistence.
Logical size, actual drawable size and an explicitly bounded render extent are
separate. Resize/move/display/maximize/restore/hide/minimize/focus loss cancel stale
pane/control ownership before consumers. Non-presentable windows defer submissions.

Orchestra and Echo use logical command UI coordinates. DataLab retains SDL drawing
and Vulkan canvas presentation, including a bounded 4096x4096 canvas; large
presentation extents scale that canvas with matched paint/input geometry. Both
DataLab viewer and startup picker adopt the lifecycle; the plain SDL viewer path
also passes. Native macOS actual loops pass all eight stages (initial, resize,
fullscreen, windowed, hidden, shown, minimized, restored), six captures each and
continued rendering, with zero Vulkan validation warnings/errors. DataLab also
passes a 5000x1440 drawable. Unit fault injection proves acquire-out-of-date and
suboptimal fence behavior; production-linked pane tests prove canceled payload
and revision restoration for all ten invalidating event kinds.

Scope is the proving trio retained Main Edit lanes. Canonical/stable programs and
app VERSION remain unchanged. macOS proof does not qualify Linux/Windows,
exclusive fullscreen, external-monitor migration, device-loss recovery, human IME
workflow or every program. The native proof drives SDL desktop fullscreen; it does
not automate clicking macOS's green window control. Next: apply the established
migration recipe to one selected program, then qualify its actual UI/native paths.
Mixed field/button traversal and OS IME sessions remain bounded follow-on work;
generalized docking/provider insertion/persistence needs its own contract.
