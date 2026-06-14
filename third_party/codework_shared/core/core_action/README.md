# core_action

Renderer-agnostic action registry and trigger binding table.

## Scope
- Register action metadata (`id`, `label`) into fixed caller-owned backing arrays
- Bind simple normalized trigger tokens to action ids
- Resolve trigger -> action deterministically

## Boundaries
- No platform keycode decoding (app/kit adapter owns that)
- No command execution policy (app runtime owns that)
- No modifier/chord interpretation or trigger conflict reporting
- No persistence or UI command palette rendering
- No UI command palette rendering

## Current Contract
- `core_action` copies action ids, labels, and trigger strings into caller-provided fixed-capacity backing storage.
- `core_action_resolve_trigger(...)` returns a borrowed `const char *` pointing into the registry's internal action storage; that pointer remains valid only while the registry backing arrays remain alive and unchanged.
- All public APIs return `bool` only. Callers cannot distinguish invalid input, overflow, missing actions, or missing triggers without adding app-local diagnostics.
- Trigger strings are normalized host-owned tokens. `core_action` does not parse platform keycodes or infer modifiers/chords.
- Duplicate action registration is treated as a successful no-op and does not increment `action_count`.
- Rebinding an existing trigger updates the target action in place and does not increment `binding_count`.

## Status
- Patch-hardened scaffold (`v0.1.1`) with boundary-condition tests for registry, binding, and resolve behavior.
