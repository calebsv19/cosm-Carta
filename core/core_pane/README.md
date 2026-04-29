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

Bootstrap foundation for Phase 15C pane standardization (`v0.2.0`).

## Recent Changes (`v0.2.0`)

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
