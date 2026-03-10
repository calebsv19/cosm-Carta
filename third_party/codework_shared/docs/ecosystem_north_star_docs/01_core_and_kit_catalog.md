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
**Role:** Scene handoff contract resolver for cross-program imports.
**Responsibilities:**
- `scene_bundle.json` path/type resolution
- Source detection (`manifest`, `.pack`, `.vf2d`)
- Optional bundle metadata path resolution

**Boundary:**
- Resolves *what to load*
- Does not decide placement math or rendering behavior

---

### core_space (ACTIVE)
**Role:** Shared spatial conversion contract.
**Responsibilities:**
- Unit-to-world/world-to-unit transforms
- Import fit/scale normalization
- Author-window span conversion

**Boundary:**
- Defines *where/how to place*
- Does not parse scene bundles or own asset formats

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

### core_memdb (BOOTSTRAP)
**Role:** Shared durable memory database contract.
**Responsibilities (initial target):**
- SQLite-backed connection lifecycle
- Query/statement execution helpers
- Schema version tracking and migration entrypoints
- Durable storage boundary for future memory tooling and console flows

**Boundary:**
- Storage/query contract only
- No UI, graph rendering, or app-specific memory curation policy
- Higher-level console behavior stays in kits/apps layered above this core

**Current status note:**
- The scaffolded module now exists at `shared/core/core_memdb/`
- The design and rollout docs live under `shared/docs/memory_db_system/`
- The current implementation is intentionally a placeholder until the SQLite backend is wired

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

### kit_ui (ACTIVE)
**Role:** Shared UI primitives (`timer_hud` seed path).
**Notes:**
- Primary consumer of `core_theme` and `core_font` contracts.
- Owns renderer adapters that convert theme/font tokens into backend-specific calls.
- Current shared controls include buttons, checkboxes, sliders, scrollbars, and segmented selectors.

### kit_pane (BOOTSTRAP)
**Role:** Shared pane-shell presentation kit for `core_pane`-driven workspaces.
**Notes:**
- Owns pane chrome visuals (border/header/title/splitter states).
- Owns authoring-mode structural overlay affordances.
- Does not own pane topology solve, module lifecycle, or workspace persistence policy.

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

---

## Shared Infrastructure (Non-core / Non-kit)

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
