# core_layout

Deterministic layout-edit transaction state for workspace shells.

## Scope
- Runtime vs authoring mode state token
- Draft/active revision counters
- Apply/cancel transaction lifecycle
- Pending-change and rebuild intent flags

## Boundaries
- No pane geometry solve (`core_pane` owns that)
- No module attachment policy (`workspace host` owns that)
- No rendering/UI dependencies

## Status
- Metadata hook expansion (`v0.2.1`) with snapshot-aware revision source support and external revision apply helper.

## Contract Notes
- `core_layout_apply_authoring(...)` exits authoring even when no pending changes exist, but that no-op path does not increment revisions or mark rebuild required.
- Revision metadata is lightweight provenance only. `core_layout` stores valid source/schema values but does not validate snapshot contents or pane semantics.
- `core_layout_set_revision_metadata(...)` may preload metadata before a later authoring apply or external revision, and invalid sources are ignored.
- Null helper calls are intentionally no-op or default-safe where the API shape allows it.

## Recent Changes (`v0.2.1`)
- Truth-locked no-op apply behavior, metadata preload semantics, and null-helper expectations.
- Expanded test coverage for snapshot restore provenance, reserved-field clearing, invalid source ignore behavior, and rebuild acknowledgement idempotence.
