# core_memdb Roadmap

Current milestone:
- Phase 1 and Phase 2 are implemented and audited complete
- replay parity/apply tooling now covers:
  - full-field drift checks (`event-replay-check`)
  - deterministic projection rebuild into target DB (`event-replay-apply`)
  - legacy payload backfill/repair (`event-backfill`)
- event-first write cutover is now implemented across all mutation lanes:
  - `add`, `pin`, `canonical`, `rollup`, `link-add`, `link-update`, `link-remove`

Next implementation steps:
- decide when to promote fingerprint enforcement from indexed lookup to hard uniqueness
- extend session-budget controls beyond `add` (cover additional write commands where needed)
- decide whether any repeated SQL in `mem_cli` should become narrow helper functions
- decide whether `batch-add` needs richer failure taxonomy and targeted retry policies
- decide whether neighbor retrieval needs bounded depth-2 traversal or should remain strictly one-hop
- decide whether to enforce link-kind canonical set at schema layer in addition to CLI write guards
- add graph hit-test selection handoff and richer link controls in `mem_console`
- add focused link/tag helper APIs in `core_memdb` only if SQL duplication starts to grow
- decide whether archived rows should be hidden from `show` or remain directly inspectable
- extend migrations beyond the current v6 event-dual-write schema as new lanes are added
- extend replay tooling from deterministic apply/rebuild outputs into snapshot+cursor artifacts and unattended restore workflows
- keep `event-backfill` compatibility checks aligned with evolving payload schema (prevent regressions in legacy upgrade path)

Later milestones:
- stricter dedupe fingerprint enforcement and follow-on rollup policy tuning
- richer FTS-backed retrieval helpers
- DB-backed graph neighborhood query helpers for console/IDE use
- optional async query helpers for UI integration
