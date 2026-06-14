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

Initial scaffold (`v0.1.1`) aligned to pane/module contract Phase 1.

## Contract Notes

1. Descriptor strings (`module_key`, `display_name`) and callback pointers are borrowed host-owned values; the registry stores them by pointer and does not copy backing storage.
2. `CORE_PANE_MODULE_PROVIDER_EXTERNAL` is public vocabulary but intentionally rejected by registration in the current scaffold.
3. `CORE_PANE_MODULE_CAP_FOCUS_REQUIRED`, `default_config_variant`, binding `config_variant`, and binding `runtime_flags` are currently stored/carried only; this module does not apply extra validation or behavior for them yet.
4. `core_pane_module_validate_bindings(...)` now treats `binding_count == 0` as a valid no-op even when bindings and pane id arrays are omitted.

## Recent Changes (`v0.1.1`)

1. Truth-locked borrowed descriptor lifetime, external-provider rejection, and current config/focus/runtime pass-through semantics.
2. Hardened lookup outputs and empty-binding validation behavior so failure and no-op paths are deterministic.
3. Expanded tests across registry-full, lookup-not-found, key-boundary, empty-binding, and binding-argument edge cases.
