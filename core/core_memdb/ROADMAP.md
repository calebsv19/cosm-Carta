# core_memdb Roadmap

Current milestone:
- Phase 1 and Phase 2 are implemented and audited complete
- replay parity/apply tooling now covers:
  - full-field drift checks (`event-replay-check`)
  - deterministic projection rebuild into target DB (`event-replay-apply`)
  - legacy payload backfill/repair (`event-backfill`)
- event-first write cutover is now implemented across all mutation lanes:
  - `add`, `pin`, `canonical`, `rollup`, `link-add`, `link-update`, `link-remove`
- shared boundary is now truth-locked more explicitly:
  - `core_memdb` owns the SQLite-backed C lifecycle/statement/transaction/migration surface
  - CLI policy, agent wrappers, nightly maintenance scripts, and host mutation paths remain higher-layer lanes

Next implementation steps:
- decide when to promote fingerprint enforcement from indexed lookup to hard uniqueness
- decide whether any repeated SQL in `mem_cli` should become narrow helper functions
- decide whether `batch-add` needs richer failure taxonomy and targeted retry policies
- decide whether neighbor retrieval needs bounded depth-2 traversal or should remain strictly one-hop
- decide whether to enforce link-kind canonical set at schema layer in addition to CLI write guards
- add graph hit-test selection handoff and richer link controls in `mem_console`
- add focused link/tag helper APIs in `core_memdb` only if repeated shared SQL duplication starts to grow enough to justify a narrower C helper boundary
- extend migrations beyond the current v6 event-dual-write schema as new lanes are added
- extend replay tooling from deterministic apply/rebuild outputs into snapshot+cursor artifacts and unattended restore workflows
- keep `event-backfill` compatibility checks aligned with evolving payload schema (prevent regressions in legacy upgrade path)

Recent completed step:
- `0.28.2` completed the first CLI policy hardening follow-on:
  - `show` now hides archived rows unless `--include-archived` is passed
  - session-budget enforcement now covers the existing mutation lanes that already accept `--session-id`
  - CLI smoke coverage now pins archived visibility plus cross-command budget exhaustion so the behavior is explicit instead of command-local folklore

Later milestones:
- stricter dedupe fingerprint enforcement and follow-on rollup policy tuning
- richer FTS-backed retrieval helpers
- DB-backed graph neighborhood query helpers for console/IDE use
- optional async query helpers for UI integration
