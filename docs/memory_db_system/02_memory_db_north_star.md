# Memory DB North Star

Status: design target

## Purpose
The Memory DB system is the local-first structured knowledge layer for the CodeWork ecosystem.

It is intended to support:
- Codex agents
- CLI tooling
- a standalone UI console
- later IDE integration

It replaces uncontrolled markdown and context loading with:
- atomic memory items
- structured tags
- explicit links
- bounded retrieval
- controlled writes
- versioned schema

## Design Constraints
The system must:
- match existing `core_*` conventions
- avoid global logging in shared core
- use `CoreResult` for shared-core error reporting
- respect current caller-owned memory rules
- fit the current worker/job handoff model
- avoid introducing system-wide dependency sprawl

## Layered Architecture
The system is organized into three layers.

### Layer A: `core_memdb`
Planned location:
- `shared/core/core_memdb/`

Responsibilities:
- SQLite wrapper
- schema creation and migrations
- CRUD over memory items, tags, and links
- FTS-backed search
- transaction boundaries
- zero UI knowledge
- zero global logging
- `CoreResult`-based failure reporting

Planned build outputs:
- `build/libcore_memdb.a`
- `build/mem_cli`

Dependencies:
- `core_base`
- vendored SQLite amalgamation compiled into the module

Architectural boundary:
- owns connection lifecycle, SQL execution helpers, migrations, and durable storage rules
- does not own UI, rendering, graph presentation, or app-specific memory workflows

### Layer B: `mem_cli`
Planned location:
- `shared/core/core_memdb/tools/mem_cli.c`

Pattern target:
- mirrors `shared/core/core_pack/tools/pack_cli.c`

Responsibilities:
- controlled memory writes
- structured subcommand routing
- dedupe enforcement
- rollup generation
- query and formatted output
- auditability through normal shell history

Non-goal:
- no interactive UI

### Layer C: Memory Console UI
Phase 1 host:
- `mem_console/`

Dependencies:
- `libkit_render`
- `libkit_ui`
- `libkit_graph_struct`
- `libcore_memdb`
- `libcore_base`
- `libvkrenderer`

Responsibilities:
- searchable list view
- detail view
- local graph view
- manual add and edit flows
- pinned and canonical toggles
- rollup preview

Initial boundary:
- no IDE integration in the first console release

## Data Model
The system starts with a compact relational schema.

### `mem_meta`
Stores key-value metadata for the database itself.

Required initial key:
- `schema_version`

This follows the existing ecosystem pattern of storing explicit version fields inside the artifact or persistence layer.

### `mem_item`
Primary durable memory record.

Required fields:
- `id`
- `stable_id`
- `title`
- `body`
- `fingerprint`
- `created_ns`
- `updated_ns`
- `pinned`
- `canonical`
- `ttl_until_ns`
- `archived_ns`

### `mem_tag`
Normalized tag table for reusable labels.

### `mem_item_tag`
Join table between memory items and tags.

### `mem_link`
Directed links between memory items for graph traversal.

### `mem_item_fts`
FTS virtual table over:
- `title`
- `body`

Constraint:
- FTS5 is required and must be enabled in the vendored SQLite build

## Core API Direction
The shared header is expected to be `core_memdb.h`.

Initial API surface:
- `core_memdb_open(...)`
- `core_memdb_close(...)`
- `core_memdb_exec(...)`
- `core_memdb_prepare(...)`
- `core_memdb_stmt_step(...)`
- `core_memdb_stmt_finalize(...)`
- `core_memdb_tx_begin(...)`
- `core_memdb_tx_commit(...)`
- `core_memdb_tx_rollback(...)`
- `core_memdb_run_migrations(...)`

API rules:
- return `CoreResult`
- avoid hidden global state
- avoid implicit logging
- use caller-owned buffers by default
- if heap allocation is exposed, document ownership explicitly and use `core_alloc` and `core_free`

## Anti-Bloat Policy
The Memory DB is intended to remain bounded and curated.

### Dedupe
Fingerprint rule:
- `FNV1a64(title + normalized_body + sorted_tags)`

Implementation direction:
- use `core_hash64_fnv1a`
- enforce a unique index on `fingerprint`
- prefer `UPDATE` over duplicate `INSERT`

### Pinned Lane
`pinned = 1` means:
- exempt from TTL
- exempt from rollup
- sorted first in retrieval and UI lists

### Canonical Lane
`canonical = 1` means:
- preferred merge target
- preferred graph root
- stable reference record when duplicates compete

### Rollup
Primary maintenance command:
- `mem rollup --before <timestamp>`

Rollup behavior:
- select unpinned, non-canonical, non-archived items
- generate a summary memory
- mark source items archived
- commit atomically

## Threading Model
Two execution modes are valid.

### Mode A: synchronous
Used by:
- CLI flows
- simple UI flows

Behavior:
- DB calls run on the main thread
- acceptable because operations are expected to be short

### Mode B: asynchronous
Used by:
- advanced UI flows
- future IDE integration

Behavior:
- worker thread executes DB work
- worker publishes immutable result payloads into a queue
- main thread is woken
- main thread consumes the result and renders

Hard rule:
- never render from a worker thread

## UI Direction
Initial console layout uses manual split rectangles.

Layout:
- left pane: searchable list
- right pane: detail pane

Expected `kit_ui` reuse:
- `kit_ui_stack_begin`
- `kit_ui_eval_button`
- `kit_ui_eval_scroll`
- `kit_ui_draw_label`
- `kit_ui_draw_button`

Search input:
- SDL text input
- app-owned text buffer

Graph mode:
- uses `kit_graph_struct_compute_layered_dag_layout`
- uses `kit_graph_struct_draw`
- uses `kit_graph_struct_hit_test`
- starts with egocentric graphs only: selected node plus one-hop links

Constraint:
- avoid a global graph hairball in early versions

## Build Strategy
`core_memdb` should mirror the existing shared-core module pattern.

Build rules:
- vendor SQLite inside `shared/core/core_memdb/`
- compile SQLite into the module archive
- enable FTS5 in that build
- avoid a system SQLite dependency

Pattern targets:
- `shared/core/core_pack/Makefile`
- `shared/showcase/kit_visual_showcase/Makefile`

## Retrieval Model for Codex Agents
On session start, retrieval should be bounded and filtered.

Primary filters:
- project
- kind
- pinned
- FTS query

Preferred results:
- top canonical records
- top recent working records
- optional rollup summaries

Write discipline:
- use structured `mem` CLI commands
- cap writes per session
- prefer update paths over additive spam

## Non-Goals
Not part of the initial rollout:
- Postgres
- distributed storage
- cross-machine sync
- embeddings
- full IDE coupling
- background index daemons

## Long-Term Evolution
After the base system is stable, likely extension paths include:
- embedding storage
- hybrid FTS and vector retrieval
- `core_pack` export for memory bundles
- `kit_graph_timeseries` support for memory/time trends
- ingestion of profiler and compiler event streams

## Final Position
This system is intended to:
- respect the shared `core_*` pattern
- match the current error model
- reuse existing hash utilities
- mirror current CLI tooling conventions
- fit the worker queue handoff model
- integrate cleanly with `kit_graph_struct`
- add capability without premature complexity
