# core_memdb

`core_memdb` is the shared-core boundary for the local-first Memory DB system.

Current state:
- Phase 1 foundation complete
- Phase 2 anti-bloat controls complete
- SQLite amalgamation vendored and compiled into the module

Implemented capabilities:
- SQLite-backed lifecycle + statement API (`open`, `close`, `exec`, `prepare`, `step`, `reset`, `finalize`)
- integer/text/f64/null bind helpers and integer/text column helpers
- explicit transaction helpers
- migration entrypoint with built-in schema handling
- schema bootstrap and version tracking via `mem_meta`
- current schema target `v6` (with built-in upgrades from `v1` through `v5`)
- baseline tables for items, tags, links, FTS, and append-only event rows

Current status:
- the SQLite amalgamation is now vendored under `external/` and compiled into the module archive
- the core DB API runs against real SQLite handles
- `mem_cli` supports `add`, `list`, `find`, `show`, `pin`, `canonical`, and `rollup`
- `mem_cli` now includes bounded retrieval via `query` for agent-oriented fetch flows
- `mem_cli add` now supports scoped metadata (`workspace_key`, `project_key`, `kind`)
- `mem_cli query` now supports scoped filters (`--workspace`, `--project`, `--kind`)
- `mem_cli add` now supports session write-budget flags (`--session-id`, `--session-max-writes`)
- `mem_cli` now includes `batch-add`, `health`, `audit-list`, and `event-list`
- `mem_cli` now includes `event-replay-check` for bounded replay/projection drift verification against event history
- `event-replay-check` now validates full row parity (`mem_item` fields + `mem_link` fields), not only structural IDs/flags
- `mem_cli` now includes `event-replay-apply` to deterministically rebuild a target projection DB from event history and verify source-vs-target parity
- `add` now runs an event-first write path (`NodeCreated`/`NodeBodyUpdated` appended first, then projection apply + FTS sync + audit)
- `pin`/`canonical` now run an event-first write path (append `NodePinnedSet`/`NodeCanonicalSet` first, then apply projection from payload)
- `rollup` now runs an event-first write path (`NodeMerged` + `NodeMetadataPatched` appended first, then projection apply + FTS sync + audit in one transaction)
- `link-add`/`link-update`/`link-remove` now run event-first transactional write paths (append `Edge*` event first, then projection apply + audit)
- `mem_cli` now includes `event-backfill` to seed missing baseline events for pre-`v6` rows/links and restore replay parity
- `mem_cli` command lanes are now split into focused modules (`mem_cli_cmd_read`, `mem_cli_cmd_write_item`, `mem_cli_cmd_write_link`, `mem_cli_cmd_event`) with a thin top-level router for lower agent context overhead
- node/link event payloads now use replay-complete JSON payloads for projection fidelity (`add`, `rollup`, `link-add`, `link-update`, backfill)
- append-only audit rows now cover all write commands (`add`, `pin`, `canonical`, `rollup`, `link-add`, `link-update`, `link-remove`) plus `health`
- write commands now dual-write event records into `mem_event` (`Node*` and `Edge*` event classes)
- `batch-add` now supports stricter failure/retry policy flags (`--max-errors`, `--retry-attempts`, `--retry-delay-ms`)
- retrieval/read commands now support `--format text|tsv|json` for machine-readable agent parsing
- `mem_cli` now also supports baseline `mem_link` workflows (`link-add`, `link-list`, `link-update`, `link-remove`)
- `mem_cli` now supports bounded graph neighborhood retrieval via `neighbors`
- nightly maintenance runner scripts now exist for manual/dry-run operations:
  - `tools/mem_nightly_reader.sh` (read-only snapshot + summary + plan)
  - `tools/mem_nightly_pruner.sh` (bounded apply/dry-run executor from plan)
  - `tools/mem_nightly_run.sh` (orchestrates reader + pruner + final report)
- Codex-driven wrappers now exist for intelligent reader/pruner passes:
  - `tools/mem_nightly_reader_codex.sh`
  - `tools/mem_nightly_pruner_codex.sh`
  - `tools/mem_nightly_run_codex.sh`
- nightly reader now emits canonical operation lanes by default:
  - `operations.link_additions = { enabled, items: [] }`
  - `operations.link_updates = { enabled, items: [] }`
- nightly pruner keeps backward compatibility for legacy plan shapes:
  - array form (`operations.link_additions[]`, `operations.link_updates[]`)
  - object form with lane toggles (`operations.link_additions.items[]`, `operations.link_updates.items[]`)
- pruner codex wrapper now runs strict apply-time plan validation before mutation:
  - validates required schema and canonical lane keys for apply mode
  - verifies budgets are non-negative and not below approved operation counts
- nightly apply policy now refuses zero-op apply by default:
  - override only with `--allow-empty-apply` when intentional
- nightly pruner now supports iterative chunked rollup execution:
  - uses `operations.rollup.chunk_max_items` for smaller rollup blocks
  - applies scoped rollup filters from `operations.rollup.scope`
- nightly pruner now supports post-rollup graph connection pass:
  - uses `operations.connection_pass` to link newly created rollup blocks
  - supports chain linking, optional anchor linking, min-degree checks, and bounded neighbor-link propagation for rollup nodes
  - connection results are reported in `pruner_apply_report.json` under planned/applied operations
- nightly reader now policy-gates rollup recommendation by scope pressure:
  - requires `active_nodes_in_scope > min_active_nodes_before_rollup` (default 40)
  - requires `stale_candidates_in_scope >= min_stale_candidates_before_rollup` (default 4)
  - prevents forced daily compression when scope pressure is low
  - stale candidate derivation excludes `kind=rollup` rows so plan counts match `mem_cli rollup` default scope
- codex run orchestrator now supports locked apply and parity artifacts:
  - `--locked-apply` to apply existing reviewed run dir without reader/review mutation
  - emits `health_after.json` and `run_report.json`
- codex run orchestrator now records per-run suggestion memory for diagnostics:
  - writes one suggestion node per run to configurable bucket (`--suggestion-project`, `--suggestion-kind`)
  - links new suggestion nodes to prior suggestions (`related`) for graph-based issue clustering
  - emits `codex_rollup_improvement.md` only for major signals by default (or always with `--always-write-suggestion-md`)
- one-line isolated test harness now exists for full rollup-cycle validation:
  - `tools/mem_nightly_rollup_cycle_test.sh` (copies main DB -> test DB, dry run, auto-approve test plan, locked apply, checksum guard on main DB)
- one-line official main-db rollup harness now exists for intentional real apply:
  - `tools/mem_nightly_rollup_cycle_main_apply.sh` (dry pass, auto-approve scoped/chunked rollup + connection pass, locked apply on main DB)
- rollup command now supports scoped and chunked compression:
  - `mem_cli rollup --before <ns> [--workspace <key>] [--project <key>] [--kind <value>] [--limit <n>]`
- rollup summary bodies are now synthesized for readability:
  - writes one concise rollup paragraph (bounded) plus a compact per-item coverage list
  - avoids raw full-body dumps for each merged row
- rollup now excludes prior `kind=rollup` rows by default when `--kind` is not provided:
  - pass `--kind rollup` explicitly only when intentional recursive rollup is desired
- link constraints are hardened:
  - canonical link kind allowlist at CLI write surface
  - self-loop writes rejected
  - DB-level unique edge constraint per `(from_item_id, to_item_id, kind)`
  - link traversal indexes (`from`, `to`, `kind`)
- dedupe behavior is update-over-insert by fingerprint in CLI write flows
- rollup is transactional and archives eligible rows
- `make -C shared/core/core_memdb test` runs both C-level and CLI smoke coverage

Dependencies:
- `core_base`
- vendored SQLite amalgamation (module-local, no system SQLite dependency)

Build commands:
- `make -C shared/core/core_memdb`
- `make -C shared/core/core_memdb tools`
- `make -C shared/core/core_memdb test`

Agent helper wrapper:
- `shared/core/core_memdb/tools/mem_agent_flow.sh`
- `retrieve` maps to bounded `query` (default `--limit 24`)
- `retrieve-canonical` maps to canonical lane (`--canonical-only`, default `--limit 8`)
- `retrieve-pinned` maps to pinned lane (`--pinned-only`, default `--limit 8`)
- `retrieve-recent` maps to recent lane (default `--limit 24`)
- `retrieve-search` enforces `--query` and defaults to `--limit 24`
- `write` maps to `add`
- `write-linked` maps to `add` + `link-add` (uses created id for immediate graph connectivity)
- scoped flags pass through on both retrieval and write:
  - retrieval filters: `--workspace`, `--project`, `--kind`
  - write metadata: `--workspace`, `--project`, `--kind`
- budget and ops passthrough:
  - write budget: `--session-id`, `--session-max-writes`
  - `batch-write` forwards to `batch-add`
  - `health` forwards to `health`
  - passthrough now includes `event-list`, `event-replay-check`, `event-replay-apply`, and `event-backfill`
  - `neighbors` passthrough available for bounded graph retrieval

Planned outputs:
- `build/libcore_memdb.a`
- `build/mem_cli`
- `build/core_memdb_test`

Current gaps / next focus:
- decide whether to promote fingerprint dedupe from policy to hard unique constraint
- decide if session-budget enforcement should expand beyond `add` into additional write commands
- evaluate optional per-row failure classification for `batch-add` retries
- evaluate whether graph neighbor retrieval should support optional depth-2 traversal with strict bounds
- keep migration steps explicit as schema versions advance
- decide long-term archived-row default behavior for `show`
- maintain full-field replay parity as event schema evolves (additive compatibility checks per new event type)
- formalize snapshot + cursor artifact flow on top of `event-replay-apply` (outside SQLite projection)
- add CLI-level replay/apply fixtures for long-lived multi-session datasets

Current CLI surface:
- `mem_cli add --db <path> --title <text> --body <text> [--stable-id <id>] [--workspace <key>] [--project <key>] [--kind <value>] [--session-id <id>] [--session-max-writes <n>]`
- `mem_cli batch-add --db <path> --input <tsv_path> [--workspace <key>] [--project <key>] [--kind <value>] [--session-id <id>] [--session-max-writes <n>] [--continue-on-error] [--max-errors <n>] [--retry-attempts <n>] [--retry-delay-ms <ms>]`
- `mem_cli list --db <path> [--format text|tsv|json]`
- `mem_cli find --db <path> --query <text> [--format text|tsv|json]`
- `mem_cli query --db <path> [--query <text>] [--limit <n>] [--offset <n>] [--pinned-only] [--canonical-only] [--include-archived] [--workspace <key>] [--project <key>] [--kind <value>] [--format text|tsv|json]`
- `mem_cli show --db <path> --id <rowid> [--format text|tsv|json]`
- `mem_cli health --db <path> [--format text|json]`
- `mem_cli audit-list --db <path> [--session-id <id>] [--limit <n>] [--format text|tsv|json]`
- `mem_cli event-list --db <path> [--session-id <id>] [--event-type <type>] [--limit <n>] [--format text|tsv|json]`
- `mem_cli event-replay-check --db <path> [--limit-events <n>] [--format text|json]`
- `mem_cli event-replay-apply --db <source_path> --out-db <target_path> [--limit-events <n>] [--format text|json]`
- `mem_cli event-backfill --db <path> [--session-id <id>] [--dry-run] [--format text|json]`
- `mem_cli pin --db <path> --id <rowid> --on|--off [--session-id <id>]`
- `mem_cli canonical --db <path> --id <rowid> --on|--off [--session-id <id>]`
- `mem_cli rollup --db <path> --before <timestamp_ns> [--workspace <key>] [--project <key>] [--kind <value>] [--limit <n>] [--session-id <id>]`
- `mem_cli link-add --db <path> --from <item_id> --to <item_id> --kind <text> [--weight <real>] [--note <text>] [--session-id <id>]`
- `mem_cli link-list --db <path> --item-id <item_id>`
- `mem_cli neighbors --db <path> --item-id <item_id> [--kind <text>] [--max-edges <n>] [--max-nodes <n>] [--format text|tsv|json]`
- `mem_cli link-update --db <path> --id <link_id> --kind <text> [--weight <real>] [--note <text>] [--session-id <id>]`
- `mem_cli link-remove --db <path> --id <link_id> [--session-id <id>]`
