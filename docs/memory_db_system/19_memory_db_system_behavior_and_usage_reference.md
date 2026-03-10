# Memory DB System Behavior and Usage Reference

Status: current implementation reference (core + CLI + console)

## 1. What the system is today

This is a local-first SQLite memory system with three active layers:

1. shared core DB boundary:
   - `shared/core/core_memdb/`
2. CLI operating surface:
   - `shared/core/core_memdb/tools/mem_cli.c` (router)
   - `shared/core/core_memdb/tools/mem_cli_cmd_read.c`
   - `shared/core/core_memdb/tools/mem_cli_cmd_write_item.c`
   - `shared/core/core_memdb/tools/mem_cli_cmd_write_link.c`
   - `shared/core/core_memdb/tools/mem_cli_cmd_event.c`
3. UI host program:
   - `mem_console/`

Primary purpose:
- bounded memory retrieval
- controlled writes
- anti-bloat lanes (`pinned`, `canonical`, `rollup`, dedupe by fingerprint)
- graph-link representation through `mem_link`

## 2. Runtime architecture map

### Core API and ownership

Public API:
- `shared/core/core_memdb/include/core_memdb.h`

Key symbols:
- open/close:
  - `core_memdb_open`
  - `core_memdb_close`
- query/statement:
  - `core_memdb_exec`
  - `core_memdb_prepare`
  - `core_memdb_stmt_step`
  - `core_memdb_stmt_reset`
  - `core_memdb_stmt_finalize`
  - bind helpers (`_bind_i64/_bind_text/_bind_f64/_bind_null`)
  - column helpers (`_column_i64/_column_text`)
- transactions:
  - `core_memdb_tx_begin`
  - `core_memdb_tx_commit`
  - `core_memdb_tx_rollback`
- migrations:
  - `core_memdb_run_migrations`

Ownership rules:
- statements are explicit and caller-finalized
- text column results are borrowed views (valid until next step/reset/finalize on same statement)
- no hidden global logging in core
- errors are `CoreResult` (code + message)

### Schema and migration behavior

Source:
- `shared/core/core_memdb/src/core_memdb.c`

Schema bootstrap:
- `core_memdb_open` calls migration entrypoint automatically
- base schema creation includes:
  - `mem_meta`
  - `mem_item`
  - `mem_tag`
  - `mem_item_tag`
  - `mem_link`
  - `mem_item_fts` (FTS5)
  - `mem_item_fingerprint_idx`
  - `mem_item_scope_idx`
  - `mem_item.workspace_key`
  - `mem_item.project_key`
  - `mem_item.kind`
  - `mem_audit`
  - `mem_audit_session_idx`
  - `mem_event`
  - `mem_event_event_id_idx`
  - `mem_event_ts_idx`
  - `mem_event_type_idx`
  - `mem_event_session_idx`

Version model:
- `mem_meta.key='schema_version'` as source of truth
- built-in migration path currently includes `v1 -> v2 -> v3 -> v4 -> v5 -> v6`

## 3. Current behavior by subsystem

### 3.1 `mem_cli` behavior

File:
- `shared/core/core_memdb/tools/mem_cli.c`

Live commands:
- `add`
- `batch-add`
- `list`
- `find`
- `query`
- `show`
- `health`
- `audit-list`
- `event-list`
- `event-replay-check`
- `event-replay-apply`
- `event-backfill`
- `pin`
- `canonical`
- `rollup`
- `link-add`
- `link-list`
- `neighbors`
- `link-update`
- `link-remove`

Operational semantics:
- `add`:
  - computes fingerprint from title+body
  - dedupe path is update-over-insert when active duplicate fingerprint exists
  - event-first write path: appends `NodeCreated`/`NodeBodyUpdated` first, then applies projection to `mem_item` and syncs FTS
  - optional scoped metadata: `--workspace`, `--project`, `--kind`
  - optional session budget guardrails: `--session-id`, `--session-max-writes`
  - successful writes append an audit row to `mem_audit`
  - preserves stable-id upgrade semantics on dedupe updates (only fills when existing stable_id is empty)
- `batch-add`:
  - TSV ingest (`title\tbody` or `stable_id\ttitle\tbody[\tworkspace\tproject\tkind]`)
  - reuses `add` semantics row-by-row
  - supports optional `--continue-on-error`
  - supports `--max-errors <n>` (requires `--continue-on-error`) for deterministic fail cutoff
  - supports `--retry-attempts <n>` and `--retry-delay-ms <ms>` for bounded retries
- `list`:
  - excludes archived rows
  - order: pinned desc, updated desc, id asc
- `find`:
  - FTS lookup only
  - excludes archived rows
  - supports `--format text|tsv|json`
- `query` (bounded retrieval surface):
  - supports `--limit` and `--offset`
  - optional lane filters: `--pinned-only`, `--canonical-only`
  - optional FTS `--query`
  - optional scoped filters: `--workspace`, `--project`, `--kind`
  - canonical/pinned-first ordering
  - optional archived inclusion via `--include-archived`
  - supports `--format text|tsv|json`
- `show`:
  - focused detail read for one memory row
  - supports `--format text|tsv|json`
- `health`:
  - checks schema version, required tables/indexes, FTS availability, and SQLite integrity
  - supports `--format text|json`
- `audit-list`:
  - reads append-only `mem_audit` rows
  - supports `--session-id`, `--limit`, and `--format text|tsv|json`
- `event-list`:
  - reads append-only `mem_event` rows
  - supports bounded filtering via `--session-id`, `--event-type`, `--limit`
- `event-replay-check`:
  - replays events into temporary projection tables
  - validates full-field parity against live `mem_item` and `mem_link`
- `event-replay-apply`:
  - replays events from `--db` into projection tables in `--out-db`
  - rebuilds FTS in the target projection
  - validates full-field source-vs-target parity and reports drift counts
- `event-backfill`:
  - seeds/repairs legacy baseline events so replay parity can pass on long-lived DBs
  - supports `--dry-run` and writes an audit entry when applying
- `pin` / `canonical`:
  - boolean lane toggles for active rows
  - event-first write path: append `NodePinnedSet`/`NodeCanonicalSet`, then apply projection update from payload in the same transaction
  - writes append audit entries (action=`pin`/`canonical`)
- `rollup`:
  - one transaction
  - selects eligible non-pinned, non-canonical, non-archived rows before cutoff
  - default selection excludes prior `kind=rollup` rows unless `--kind` is explicitly set
  - event-first write path: append `NodeMerged` (summary row) and per-row `NodeMetadataPatched` archive events first, then apply projection in-transaction
  - creates summary memory using a concise synthesized rollup paragraph plus compact per-item coverage list (instead of raw full-body dumps)
  - archives rolled rows atomically
  - write appends audit entry (action=`rollup`)
- `link-*`:
  - CRUD over `mem_link`
  - validates active endpoint items for add/list workflows
  - enforces canonical link-kind set at CLI write surface (`supports`, `depends_on`, `references`, `summarizes`, `implements`, `blocks`, `contradicts`, `related`)
  - rejects self-loop writes (`from_item_id == to_item_id`)
  - event-first write paths on all mutations (`EdgeAdded`, `EdgeUpdated`, `EdgeRemoved` appended before projection apply, in one transaction)
  - write commands append audit entries (`link-add`, `link-update`, `link-remove`)
- `neighbors`:
  - bounded one-hop neighborhood read for selected item
  - supports optional `--kind` filter and explicit `--max-edges` / `--max-nodes` bounds
  - supports `--format text|tsv|json`

### 3.2 `mem_agent_flow` wrapper behavior

File:
- `shared/core/core_memdb/tools/mem_agent_flow.sh`

Purpose:
- thin stable shell for agent workflows

Mappings:
- `retrieve` -> `mem_cli query` (default `--limit 24` if not set)
- `retrieve-canonical` -> `mem_cli query --canonical-only` (default `--limit 8`)
- `retrieve-pinned` -> `mem_cli query --pinned-only` (default `--limit 8`)
- `retrieve-recent` -> `mem_cli query` (default `--limit 24`)
- `retrieve-search` -> `mem_cli query` with required `--query` (default `--limit 24`)
- `write` -> `mem_cli add`
- `write-linked` -> `mem_cli add` then `mem_cli link-add` (uses created id)
- `batch-write` -> `mem_cli batch-add`
- `health` -> `mem_cli health`
- passthrough for other commands (including `event-replay-apply`)

### 3.3 `mem_console` behavior

Files:
- lifecycle/event loop:
  - `mem_console/src/mem_console.c`
- state/layout/input helpers:
  - `mem_console/src/mem_console_state.c`
- DB access + write actions:
  - `mem_console/src/mem_console_db.c`
- draw orchestration:
  - `mem_console/src/mem_console_ui.c`
- draw sections/panels:
  - `mem_console/src/mem_console_ui_left_section.c`
  - `mem_console/src/mem_console_ui_detail_section.c`
  - `mem_console/src/mem_console_ui_graph_controls.c`
  - `mem_console/src/mem_console_ui_graph_panel.c`

Current UI capabilities:
- searchable list pane with scalable windowed loading
- project-scope quick-filter chips (`ALL PROJECTS` + multi-select project toggles) wired to `mem_item.project_key`
- detail pane with explicit title/body editing modes
- pinned/canonical toggles
- create-from-search action
- graph mode with one-hop neighborhood rendering and node hit-select
- graph mode supports kind-filter segmented control (all/supports/depends_on/references/summarizes/related)
- graph edges render compact kind labels for direct semantic visibility in preview
- runtime theme/font cycling
- app-local UI prefs serialization for theme/font via `core_pack` (`<db_path>.ui.pack`)
- periodic async refresh scaffold:
  - worker-side refresh through shared runtime libs (`core_workers` + `core_queue` + `core_wake`)
  - main-thread-only state apply with stale-result guards
  - idle-loop pacing policy via runtime timeout calculation (reduces active-loop churn)
  - redraw invalidation reason tracking (`input/layout/theme/content/background`) with render-on-demand behavior
  - in-flight refresh intent coalescing (keeps latest pending `(search,selected,offset,project-filter-set,graph-kind-filter)` intent)
  - runtime observability counters surfaced in UI for async refresh lifecycle
- optional kernel bridge mode:
  - launch flag: `--kernel-bridge`
  - wires additive evaluation scaffold over `core_sched` + `core_jobs` + `core_wake` + `core_kernel`
  - keeps existing SDL render/event ownership intact (no full loop cutover yet)

Default DB path behavior:
- no `--db` opens:
  1. `CODEWORK_MEMDB_PATH` (if set)
  2. `~/Desktop/CodeWork/data/codework_mem_console.sqlite` (if `~/Desktop/CodeWork/data` exists)
  3. fallback `mem_console/demo/demo_mem_console.sqlite`
- helper to reset demo DB:
  - `mem_console/demo/reset_demo_db.sh`
  - reset script now seeds connected memories + links for immediate graph verification

### 3.4 Nightly rollup orchestration behavior

Scripts:
- `shared/core/core_memdb/tools/mem_nightly_reader.sh`
- `shared/core/core_memdb/tools/mem_nightly_pruner.sh`
- `shared/core/core_memdb/tools/mem_nightly_run.sh`
- Codex wrappers:
  - `mem_nightly_reader_codex.sh`
  - `mem_nightly_pruner_codex.sh`
  - `mem_nightly_run_codex.sh`

Current policy:
- reader is always read-only and emits a reviewed plan with `operations.rollup.enabled=false` by default
- reader stale-candidate derivation excludes `kind=rollup` rows so proposal counts align with default `mem_cli rollup` scope
- rollup recommendation is gated by pressure thresholds:
  - `active_nodes_in_scope > min_active_nodes_before_rollup` (default `40`)
  - `stale_candidates_in_scope >= min_stale_candidates_before_rollup` (default `4`)
- pruner enforces budgets before apply and rejects zero-op apply by default (unless explicitly overridden)
- rollup apply is chunked/scoped by plan:
  - `operations.rollup.chunk_max_items`
  - `operations.rollup.scope = { workspace, project, kind }`
- connection pass keeps rollup nodes connected after apply:
  - anchor + chain linking
  - optional min-degree fallback
  - optional neighbor-link propagation with bounded scan controls:
    - `propagate_neighbor_links`
    - `max_neighbor_links_per_rollup`
    - `neighbor_scan_max_edges`
    - `neighbor_scan_max_nodes`

## 4. Anti-bloat behavior in current implementation

Dedupe:
- fingerprint index exists
- write-path dedupe currently enforced in CLI/application logic

Pinned lane:
- `pinned=1` rows survive rollup selection

Canonical lane:
- `canonical=1` rows survive rollup selection
- canonical-first retrieval ordering is supported through `query`

Rollup:
- archives only eligible non-pinned/non-canonical/non-archived rows
- default rollup selection excludes `kind=rollup` rows unless explicitly requested
- commits atomically with generated summary item
- summary body is concise and human-readable (rollup paragraph + coverage list), not raw full-body concatenation

## 5. Build and validation reference

Core build/test:
- `make -C shared/core/core_memdb all`
- `make -C shared/core/core_memdb test`

Console build:
- `make -C mem_console clean all`

Demo DB reset:
- `./mem_console/demo/reset_demo_db.sh`

Basic bounded retrieval check:
- `shared/core/core_memdb/build/mem_cli query --db mem_console/demo/demo_mem_console.sqlite --limit 5`

## 6. Recommended agent usage flow (current)

Session read-start:
1. canonical lane:
   - `query --canonical-only --limit 8`
2. pinned lane:
   - `query --pinned-only --limit 8`
3. FTS lane:
   - `query --query "<fts>" --limit 24`
4. scoped lane (when a project/workspace is active):
   - `query --workspace "<workspace>" --project "<project>" --limit 24`

Focused read:
- `show --id <id>` only for selected rows

Write:
- `add --stable-id <id>` for durable concepts
- prefer update-over-new by dedupe semantics
- add graph links explicitly (`link-add`) or use wrapper `write-linked` during create
- cap write count per session in orchestration

Maintenance:
- run `rollup` in explicit maintenance windows, not inside every agent turn

## 7. Tooling still needed for “true agent brain” operation

The system is functional now, but these additions will materially improve reliability for always-on Codex usage:

1. write-budget enforcement helper:
   - now implemented in `add` via `--session-id` + `--session-max-writes`
2. deterministic audit log stream:
   - now implemented via append-only `mem_audit` + `audit-list`
3. batch ingest/update operations:
   - initial implementation: `batch-add`
4. health/check command:
   - now implemented: `health`
5. integration test harness for agent flow:
   - scripted `retrieve -> show -> write -> retrieve` verification with assertions

## 8. Skill handoff references

Skill-contract docs:
- `shared/docs/memory_db_system/17_codex_agent_integration_contract.md`
- `shared/docs/memory_db_system/18_codex_skill_packaging_plan.md`

This document (`19_*`) is the “current behavior truth source” for building the first production skill prompt and command wrapper.
