# Codex Skill Packaging Plan

Status: active packaged skill; baseline history-communication pass implemented

## Goal

Convert Memory DB + `mem_cli` into a Codex skill with a predictable command contract and bounded operating model.

## Phase A: Tool Contract Freeze

Checklist:
1. lock command surface used by the skill:
   - `query`, `show`, `add`, `batch-add`, `pin`, `canonical`, `item-retag`, `rollup`, `health`, `audit-list`, `neighbors`, `link-add`
   - wrapper linked write: `mem_agent_flow.sh write-linked`
2. keep outputs line-oriented and stable
   - rollup summary shape should remain stable: concise synthesis paragraph + coverage list
3. ensure `mem_cli` usage text documents every required flag
4. add machine-readable output mode for retrieval/read commands
   - `--format text|tsv|json`

Exit criteria:
- commands compile and run from clean build
- CLI smoke test covers at least one `query` path
- CLI smoke test covers `--format json` and `--format tsv` retrieval/read paths

Current progress:
- Phase A tool contract is now implemented in CLI and smoke tests
- strict chronological `query --order recent` and wrapper `retrieve-recent` behavior are implemented and smoke-tested
- `health` now opens the existing DB read-only without migration or audit noise, and bounded `audit-list`/`event-list` tails can explicitly request `--order recent`
- changed Lane Head upserts now consume the same shared session mutation budget as ordinary item/link writes; exact converged no-ops remain free
- stable-id-only upsert and transactional wrapper `write-lane-head` create/update/link replacement are implemented, adversarially tested, and demo-validated

## Phase B: Skill Prompt Contract

Checklist:
1. define skill prompt rules:
   - retrieve before write
   - bounded limits only
   - enforce write cap per session
   - treat `project` as a durable bucket key that may represent app, site,
     host, or cross-cutting coordination lanes
   - route VPS-wide or machine-wide state into host buckets such as
     `vps_server` or `home_server`
   - keep site/program-specific implementation memories in the owning
     site/program bucket and link them to host buckets when environment context
     matters
   - capture the returned `id=<rowid>` after every `add` and use that exact row id for follow-up commands
   - never guess row ids from insertion order, neighboring ids, or stable-id text
   - nightly rollup recommendation must remain policy-gated (`min_active_nodes_before_rollup`, `min_stale_candidates_before_rollup`)
   - preserve canonical connection-pass shape (including neighbor-link propagation bounds) when rollup is enabled
   - emit one suggestion memory node per codex nightly run so improvement ideas can be graph-clustered over time
   - use one stable canonical lane head only for high-volume active lanes, refreshing it at material state transitions rather than every receipt
   - keep each lane head within the 900-byte current-handoff contract and point to receipt evidence instead of copying it
   - treat exact `unchanged` output as successful write-free convergence; do not retry it as a missing write
2. define default db path policy per project/workspace
3. define id-follow-up flow (`query -> show -> optional add`)

Exit criteria:
- prompt contract mirrors `17_codex_agent_integration_contract.md`
- no direct SQL usage in skill instructions

## Phase C: Validation Harness

Checklist:
1. run:
   - `make -C shared/core/core_memdb test`
2. reset demo DB:
   - `./mem_console/demo/reset_demo_db.sh ./mem_console/demo/demo_mem_console.sqlite`
3. run representative command flow:
   - bounded query
   - add with stable-id and parse returned row id
   - show using that returned row id
   - explicit link creation (`link-add` or `write-linked`)
   - pin/canonical toggle
   - health check
   - audit-list session verification
   - neighbors retrieval with explicit max bounds
   - lane-head create/update identity preservation, canonical visibility, required body fields, and managed latest-link replacement
   - fingerprint-collision isolation, scope collision rejection, duplicate anchor/latest ownership rejection, exact no-op event/audit silence, session-budget atomicity, unrelated-edge conflict rollback, and exact event replay

Exit criteria:
- all commands succeed on a clean DB
- no unbounded retrieval path in examples

## Phase D: Skill Asset Assembly

Skill bundle should include:
- command cheat sheet
- bounded retrieval defaults
- write safety checklist
- failure policy
- example session transcript

## Near-Term Follow-On

After initial skill release:
1. evaluate the hardened Lane Head V1 through fresh-agent orientation in the two existing active lanes
2. add a read-only conformance report only if lane-head/body/link drift appears after the hardened path is in normal use
3. consider project-alias normalization only after the retrieval/write/lane-head baseline proves stable
4. consider legacy phase rollups only after direct continuity links provide a reliable history spine
