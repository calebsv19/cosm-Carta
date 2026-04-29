# core_pane_module

Shared pane-module registry and binding validation primitives for workspace hosts.

## Scope

1. Register v1 internal module descriptors with stable identities.
2. Validate descriptor capability/hook compatibility.
3. Validate module bindings against known pane leaves and registry entries.

## Boundary

1. No plugin binary loading/runtime isolation.
2. No arbitrary key/value module config payload parsing.
3. No host render/input loop ownership.

## Status

Initial scaffold (`v0.1.0`) aligned to pane/module contract Phase 1.
