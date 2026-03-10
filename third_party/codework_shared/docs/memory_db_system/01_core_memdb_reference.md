# core_memdb Reference

Status: implemented shared module (Phase 1 + Phase 2 complete, Phase 10 + Phase 11 additive lanes active)

## Purpose
`core_memdb` is the shared core module for durable, queryable memory storage.

Its role is to provide a stable data-access boundary for:
- SQLite-backed storage
- schema version tracking and migration entrypoints
- prepared query execution
- transaction-safe writes
- integration with `mem_cli` and `mem_console`

## Intended Placement
- module path: `shared/core/core_memdb/`
- docs path: `shared/docs/memory_db_system/`

## Boundary (Initial)
- owns database connection lifecycle, query helpers, and migration orchestration
- should remain UI-free and app-domain neutral
- should not own render logic, widget behavior, or app-specific memory curation policy
- should return explicit errors and preserve caller-controlled ownership semantics

## Adjacent Shared Dependencies (Expected)
- `core_base` for result/error types, allocation wrappers, and hashing helpers
- `core_io` for optional file/path helpers where needed
- optional higher-layer consumers may include `kit_ui`, `kit_graph_struct`, and app-local tooling, but those stay outside `core_memdb`

## Planned Downstream Surfaces
- a standalone `mem` CLI for inspection, import, rollup, and maintenance flows
- an optional memory console UI/graph surface layered above shared kits
- later app integration where the runtime/threading model already supports safe background work

## Current Implemented Surface
- public API is live in `shared/core/core_memdb/include/core_memdb.h`
- implementation is live in `shared/core/core_memdb/src/core_memdb.c`
- vendored SQLite amalgamation is under `shared/core/core_memdb/external/`
- schema target is currently `v6`, with built-in upgrades from `v1` through `v5`
- baseline tables exist (`mem_meta`, `mem_item`, `mem_tag`, `mem_item_tag`, `mem_link`, `mem_item_fts`)
- `mem_item` now includes additive scope fields (`workspace_key`, `project_key`, `kind`) and `mem_item_scope_idx`
- append-only operations audit is now tracked in `mem_audit`
- append-only event lane is now tracked in `mem_event` (event-id/type/session/timestamp indexed)
- `mem_link` now has traversal indexes (`from`, `to`, `kind`) and unique same-kind edge constraint (`from_item_id`, `to_item_id`, `kind`)
- `core_memdb` stays log-free and returns `CoreResult`

## Current Constraints and Gaps
- hard uniqueness on `fingerprint` is not enforced yet (index exists, dedupe is currently write-path policy)
- tag/link workflows are schema-present but do not yet have a richer shared helper layer
- graph-mode query/edit API helpers are not yet introduced
- async DB job wrappers are not yet part of `core_memdb`

## Related Planning Docs
- broader system architecture and rollout plan live in:
  - `02_memory_db_north_star.md`
  - `03_memory_db_implementation_plan.md`
- Phase 1 execution and audit:
  - `04_phase1_core_memdb_detailed_plan.md`
  - `05_phase1_core_memdb_completion_audit.md`
- Phase 2 execution and audit:
  - `06_phase2_anti_bloat_detailed_plan.md`
  - `07_phase2_anti_bloat_completion_audit.md`
