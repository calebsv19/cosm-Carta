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

Bootstrap foundation for Phase 15C pane standardization (`v0.1.2`).

## Recent Changes (`v0.1.2`)

1. Fixed splitter drag accumulation so repeated drag deltas apply from current node ratio (no stale-hit tug-of-war behavior).
2. Updated drag tests to validate deterministic multi-step drag without mutating hit snapshots between steps.

## Previous Changes (`v0.1.1`)

1. Added deterministic invalid-graph rejection for cyclic/self/duplicate child references.
2. Hardened solve/hit/drag paths against non-finite inputs.
3. Expanded tests to cover graph validation and deterministic drag sequence behavior.
