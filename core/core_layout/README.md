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
- Initial scaffold (`v0.1.0`) with state machine tests.
