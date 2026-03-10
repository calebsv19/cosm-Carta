# Shims vs Runtime Libs Boundary

This file separates two different concerns in the ecosystem.

## System Include Shims (`shared/sys_shims`)

Purpose:

- Parse/compile compatibility for C-style system headers.
- Controlled include behavior for compiler/analysis/test pipelines.

Non-goals:

- Runtime UI behavior.
- App data model ownership.
- Visual theming or font management.

## Runtime Shared Libs (`shared/core/core_*`, `shared/kit/kit_*`, etc.)

Purpose:

- Shared runtime behavior, data models, rendering helpers, and API contracts across apps.

Examples:

- `core_theme` + `core_font`: runtime theme/font presets and tokenized style lookup.
- `core_pack`: interchange format for serialized data.
- `core_scene`: shared scene-level data schema.

## Integration Rule

- Compiler/include concerns must stay in `sys_shims`.
- Runtime concerns must stay in core/kit libs.
- App adapters may consume both, but must not merge their responsibilities.

## Testing Rule

- Shim tests: parser/compile smoke + parity + subset compile checks.
- Runtime shared-lib tests: unit tests + adapter tests + app smoke tests.
