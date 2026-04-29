# core_pane_snapshot

Shared pane snapshot structs and validation helpers for workspace hosts.

## Scope

1. Define v1 snapshot metadata/node/binding record structures.
2. Validate graph and field invariants for snapshot imports.
3. Validate module binding references to leaf pane IDs.

## Boundary

1. No `core_pack` file IO ownership.
2. No JSON serializer ownership.
3. No host runtime policy or module registry loading behavior.

## Status

Initial scaffold (`v0.1.0`) aligned to Phase 1 snapshot contract.
