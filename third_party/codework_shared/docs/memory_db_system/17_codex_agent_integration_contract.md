# Codex Agent Integration Contract

Status: active baseline contract for skill-build preparation

## Purpose

Define a stable, bounded interface so Codex agents can read/write Memory DB safely through `mem_cli` without bypassing anti-bloat controls.

## Required Tooling Surface

Primary tool binary:
- `shared/core/core_memdb/build/mem_cli`

Optional wrapper for skill/runtime integration:
- `shared/core/core_memdb/tools/mem_agent_flow.sh`
- linked-write helper:
  - `mem_agent_flow.sh write-linked --db <path> --title <text> --body <text> [add flags...] --link-from <id>...|--link-to <id>... [--link-kind <kind>] [--link-note <text>]`
  - `mem_agent_flow.sh write-hier-linked --db <path> --title <text> --body <text> --workspace <key> --project <key> --kind <value> [--parent-id <id>...] [--related-id <id>...] [--importance low|normal|high|critical] [--max-links <1-5>] [--link-note <text>]`

Required commands:
- bounded retrieval:
  - `mem_cli query --db <path> [--query <fts>] [--limit <n>] [--offset <n>] [--pinned-only] [--canonical-only] [--include-archived] [--workspace <key>] [--project <key>] [--kind <value>] [--format text|tsv|json]`
- focused detail read:
  - `mem_cli show --db <path> --id <rowid> [--format text|tsv|json]`
- write/update:
  - `mem_cli add --db <path> --title <text> --body <text> [--stable-id <id>] [--workspace <key>] [--project <key>] [--kind <value>] [--session-id <id>] [--session-max-writes <n>]`
  - `mem_cli batch-add --db <path> --input <tsv_path> [--workspace <key>] [--project <key>] [--kind <value>] [--session-id <id>] [--session-max-writes <n>] [--continue-on-error] [--max-errors <n>] [--retry-attempts <n>] [--retry-delay-ms <ms>]`
- control lanes:
  - `mem_cli pin --db <path> --id <rowid> --on|--off [--session-id <id>]`
  - `mem_cli canonical --db <path> --id <rowid> --on|--off [--session-id <id>]`
  - `mem_cli item-retag --db <path> --id <rowid> [--workspace <key>] [--project <key>] [--kind <value>] [--include-archived] [--session-id <id>]`
- maintenance:
  - `mem_cli rollup --db <path> --before <timestamp_ns> [--workspace <key>] [--project <key>] [--kind <value>] [--limit <n>] [--session-id <id>]`
  - `mem_cli health --db <path> [--format text|json]`
  - `mem_cli audit-list --db <path> [--session-id <id>] [--limit <n>] [--format text|tsv|json]`
  - `mem_cli neighbors --db <path> --item-id <rowid> [--kind <value>] [--max-edges <n>] [--max-nodes <n>] [--format text|tsv|json]`

## Retrieval Policy (Bounded)

Default retrieval budget per request:
- `limit=24` for broad lookups
- `limit=8` for narrow follow-up lookup

Recommended retrieval sequence:
1. canonical lane:
   - `mem_cli query --db <path> --canonical-only --limit 8`
2. pinned lane:
   - `mem_cli query --db <path> --pinned-only --limit 8`
3. query lane:
   - `mem_cli query --db <path> --query "<fts>" --limit 24`
4. scoped lane when session context is known:
   - `mem_cli query --db <path> --workspace "<workspace>" --project "<project>" --limit 24`
5. detail read only for selected ids:
   - `mem_cli show --db <path> --id <rowid>`

## Write Policy (Controlled)

Agent write guardrails:
- max new writes per session: `<= 6`
- prefer `--stable-id` for durable concepts
- dedupe behavior is update-over-insert by fingerprint
- do not write if retrieval already returns a canonical equivalent
- enforce caps with `--session-id <session>` + `--session-max-writes <n>` on `add`/`batch-add`

When to pin/canonical:
- pin when memory must survive rollup/TTL cleanup
- canonical when memory should be preferred merge/retrieval target
- rollup default excludes `kind=rollup` unless `--kind` is explicitly provided

Graph link policy:
- use canonical kinds only: `supports`, `depends_on`, `references`, `summarizes`, `implements`, `blocks`, `contradicts`, `related`
- links are explicit only; agents must run `link-add` (or `write-linked`) for graph connectivity
- keep neighbor retrieval bounded with explicit `--max-edges` and `--max-nodes`
- hierarchy-first anchor model per project:
  - `scope-<project>`, `plans-<project>`, `decisions-<project>`, `issues-<project>`, `misc-<project>`
  - prefer parent/branch links into pillar lanes before adding lateral links
- dynamic link budget for new nodes:
  - front-loaded target distribution: `1` default, `2` occasional, `3` uncommon, `4` rare, `5` exceptional
  - most nodes should end at `1-2` links; use `3-5` only for high-importance bridge nodes
  - minimum `1` link required (non-isolation invariant)
- fallback anchor:
  - if no high-confidence semantic target exists, link to `misc-<project>`
- cross-project link gate:
  - allow only when explicit implementation/dependency bridge exists across projects
  - avoid broad fan-out cross-project linking
- hierarchy-first helper behavior (`write-hier-linked`):
  - resolves pillar anchors by stable id within `(workspace, project)` scope
  - candidate ordering: `parent` -> `primary pillar` -> `scope pillar` -> explicit related ids -> `misc fallback`
  - front-loaded link count selection:
    - `importance=low` -> 1
    - `importance=normal` -> 1 (2 when parent+primary anchors are both present)
    - `importance=high` -> up to 3 (bounded by `--max-links`)
    - `importance=critical` -> up to 4 (bounded by `--max-links`)
  - hard cap remains `--max-links <= 5`

Light recategorization policy (migration-safe):
- when reorganizing existing memory graphs, prioritize high-confidence relinking first
- prefer adding pillar links over deleting historical links during first pass
- defer ambiguous cases to `misc-<project>` instead of forcing uncertain classification

Nightly rollup orchestration policy (reader/pruner wrappers):
- rollup recommendation is policy-gated, not automatic:
  - `active_nodes_in_scope > min_active_nodes_before_rollup` (default `40`)
  - `stale_candidates_in_scope >= min_stale_candidates_before_rollup` (default `4`)
- keep `operations.rollup.enabled=false` until review confirms policy gate + proposal
- use chunked rollup for readability/traceability:
  - `operations.rollup.chunk_max_items`
  - `operations.rollup.scope = { workspace, project, kind }`
- preserve graph connectivity for rollup nodes through connection pass:
  - `operations.connection_pass = { enabled, link_kind, anchor_kind, anchor_item_id, ensure_min_degree, propagate_neighbor_links, max_neighbor_links_per_rollup, neighbor_scan_max_edges, neighbor_scan_max_nodes }`
- codex nightly orchestrator records one suggestion memory node per run for longitudinal diagnostics:
  - default suggestion bucket: `workspace=codework`, `project=memory_console_codex_suggestions`, `kind=decision`
  - suggestion nodes link to prior suggestion nodes with `related`

## Output Contract

`query` output format:
- for skills, prefer `--format json`
- text mode shape (legacy):
  - `<id> | pinned=<0|1> | canonical=<0|1> | updated_ns=<ns> | <title> [| stable_id=<id>]`

Wrapper retrieval profiles (`mem_agent_flow.sh`):
- `retrieve-canonical`
- `retrieve-pinned`
- `retrieve-recent`
- `retrieve-search`

## Failure Handling

Agent behavior on command failure:
1. do not retry writes blindly
2. retry read/query once only for transient shell/tooling failures
3. surface error text to orchestration layer
4. abort session write phase if DB open/migration fails

## Skill Readiness Gate

Treat skill packaging as ready when:
- retrieval is always bounded through `query`
- write caps are enforced by orchestration prompts/workflow
- docs and smoke checks are current
- demo DB can be reset to empty baseline before test runs
