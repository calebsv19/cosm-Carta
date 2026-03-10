# Codex Skill Packaging Plan

Status: phase-ready scaffold plan

## Goal

Convert Memory DB + `mem_cli` into a Codex skill with a predictable command contract and bounded operating model.

## Phase A: Tool Contract Freeze

Checklist:
1. lock command surface used by the skill:
   - `query`, `show`, `add`, `batch-add`, `pin`, `canonical`, `rollup`, `health`, `audit-list`, `neighbors`, `link-add`
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

## Phase B: Skill Prompt Contract

Checklist:
1. define skill prompt rules:
   - retrieve before write
   - bounded limits only
   - enforce write cap per session
   - nightly rollup recommendation must remain policy-gated (`min_active_nodes_before_rollup`, `min_stale_candidates_before_rollup`)
   - preserve canonical connection-pass shape (including neighbor-link propagation bounds) when rollup is enabled
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
   - `./mem_console/demo/reset_demo_db.sh`
3. run representative command flow:
   - bounded query
   - show
   - add with stable-id
   - explicit link creation (`link-add` or `write-linked`)
   - pin/canonical toggle
   - health check
   - audit-list session verification
   - neighbors retrieval with explicit max bounds

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
1. add optional typed output mode (`--format tsv|json`) if needed for tighter parsing
2. add retrieval profile presets (canonical-first, recent-first)
3. add graph profile presets (link-kind filters and bounded neighbor templates)
