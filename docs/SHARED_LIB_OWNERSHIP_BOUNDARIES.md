# Shared Lib Ownership Boundaries

This document defines what each shared library owns so behavior does not overlap.

## Core Libs

- `core_base`: error/result types, common primitives, shared low-level utilities.
- `core_io`: filesystem/text/binary IO helpers and load/save boundaries.
- `core_data`: structured in-memory data containers and typed table/object model.
- `core_memdb`: durable memory database connection, query, and migration boundary (scaffolded).
- `core_math`: generic numeric primitives and math helpers.
- `core_time`: monotonic time reads and duration arithmetic (no sleep/scheduler behavior).
- `core_queue`: bounded queue primitives and queue ownership semantics.
- `core_sched`: timer/deadline scheduling data structures and callbacks.
- `core_jobs`: main-thread budgeted job queue execution.
- `core_workers`: fixed-size worker pool and task execution lifecycle.
- `core_wake`: cross-thread wake/wait abstraction for kernel orchestration.
- `core_kernel`: runtime phase orchestration and module lifecycle policy.
- `core_scene`: scene schema and scene-level object grouping/state metadata.
- `core_scene_compile`: shared authoring-to-runtime scene compile and normalization boundary.
- `core_space`: coordinate-space mapping, transforms, and grid/window/world conversion.
- `core_units`: unit vocabulary, unit conversions, and world-scale conversion primitives.
- `core_object`: app-neutral object identity/transform/dimensional-mode validation primitives.
- `core_pack`: versioned chunked interchange container (`.pack`).
- `core_layout`: renderer-agnostic layout transaction state (runtime/authoring mode, apply/cancel, revision/rebuild flags).
- `core_config`: lightweight typed runtime configuration table boundary.
- `core_action`: action identity + trigger-binding registry boundary.
- `core_trace`: trace capture/ingest/export primitives.
- `core_pane`: renderer-agnostic pane tree layout semantics (split ratios, constraints, splitter hit/drag math).
- `core_theme`: tokenized color + spacing presets.
- `core_font`: font roles + font tier/size preset contracts.

## Kit Libs

- `kit_viz`: visualization-specific helpers layered on top of core contracts.

## Non-Core Shared Modules

- `vk_renderer`: renderer backend implementation details and Vulkan/SDL bridge.
- `timer_hud`: timing/profiling HUD layer.
- `shape`: shared shape import/export helpers and ShapeLib tooling.
- `sys_shims`: system include compatibility overlays (compile-time concern only).

## Boundary Decisions (Current)

- Vector math:
  - Put generic vec/matrix numeric ops in `core_math`.
  - Put world/screen/viewport conversion and spatial mapping in `core_space`.

- Scene vs object ownership:
  - `core_scene` owns app-agnostic scene structure/schema.
  - App-local object composition or editor-only transient state stays in app code.

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

- Theme/font:
  - Preset and token source of truth must stay in `core_theme` / `core_font`.
  - App adapters map tokenized values into local UI draw calls.

## Anti-Overlap Rules

- Do not duplicate generic math helpers in app code if `core_math` already owns them.
- Do not place scene-schema types in app-specific UI/render modules.
- Do not add compiler include emulation behavior to runtime core libs.
- Do not hardcode theme/font constants in app UI where adapter lookup exists.
- Do not place pane geometry solve or hit-testing semantics in `core_layout`.
- Do not add persistence/file-IO behavior to `core_config`.
- Do not couple `core_action` to platform keycode parsing or UI command widgets.
