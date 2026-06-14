# core_pane

Shared pane-layout primitives for split-pane geometry and drag updates.

## Scope

1. Solve pane-tree splits into leaf rectangles.
2. Enforce min-size constraints through ratio clamping.
3. Hit-test splitter handles from screen-space points.
4. Apply drag deltas to splitter ratios with bounds safety.

## Boundary

1. No renderer/UI framework coupling.
2. No app-specific pane policies.
3. No file-format parsing or persistence ownership.

## Status

Bootstrap foundation for Phase 15C pane standardization (`v0.3.1`).

## Contract Notes

1. `core_pane_validate_graph(...)` is the only structured diagnostics surface today. `core_pane_solve(...)`, hit-testing, and drag application remain boolean-only.
2. `core_pane_solve(...)` requires a caller-owned output buffer and resets `*out_leaf_count` to `0` before validation or solve failures.
3. `core_pane_collect_splitter_hits(...)` and `core_pane_hit_test_splitter_hits(...)` are geometry helpers over caller-owned hit registries; they do not validate renderer state or cursor policy.
4. `core_pane_apply_splitter_drag(...)` expects a live splitter hit produced for the same node/axis and rejects malformed or stale hit metadata.

## Recent Changes (`v0.3.1`)

1. Truth-locked the current validation-versus-solve contract so diagnostics ownership is explicit.
2. Hardened `core_pane_solve(...)` to clear leaf counts on entry/failure and reject malformed cached-hit drag metadata safely.
3. Expanded tests across validation reports, cached-hit edge cases, and deterministic solve failure behavior.

## Previous Changes (`v0.3.0`)

1. Added deterministic splitter-hit enumeration via `core_pane_collect_splitter_hits(...)`.
2. Added cached-hit testing via `core_pane_hit_test_splitter_hits(...)` so hosts can keep explicit divider-hitbox registries like IDE.
3. Kept collected splitter order aligned with direct tree hit-testing so cached and uncached paths resolve the same divider first.

## Previous Changes (`v0.2.0`)

1. Added explicit graph validation diagnostics via `core_pane_validate_graph(...)` and `CorePaneValidationReport`.
2. Added validation error-string surface (`core_pane_validation_code_string(...)`) for deterministic host diagnostics.
3. Routed solve/hit-test preconditions through shared validation to reduce duplicated failure-path logic.

## Previous Changes (`v0.1.2`)

1. Fixed splitter drag accumulation so repeated drag deltas apply from current node ratio (no stale-hit tug-of-war behavior).
2. Updated drag tests to validate deterministic multi-step drag without mutating hit snapshots between steps.

## Previous Changes (`v0.1.1`)

1. Added deterministic invalid-graph rejection for cyclic/self/duplicate child references.
2. Hardened solve/hit/drag paths against non-finite inputs.
3. Expanded tests to cover graph validation and deterministic drag sequence behavior.
