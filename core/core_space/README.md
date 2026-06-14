# core_space

Shared spatial conversion contract for cross-app scene imports.

## Scope
- Keep import placement/scale/world conversion math consistent across apps.
- Provide a small API for unit<->world conversion and import transform resolution.

## Current Contract
- `CoreSpaceDesc` owns grid size, origin, cell size, author-window dimensions, and desired fit ratio for import placement math.
- `core_space_desc_default_from_grid(...)` seeds:
  - `cell_size = 1.0f` when the requested value is non-positive
  - `author_window_w = 1200`
  - `author_window_h = 800`
  - `desired_fit = 0.25f`
- `core_space_desc_validate(...)` requires positive grid dimensions, positive finite `cell_size`, positive author-window dimensions, and positive finite `desired_fit`. `origin_x` and `origin_y` must also be finite.
- `core_space_compute_span_from_window(...)` derives import span from the smaller author-window axis and leaves output values unchanged on failure.
- Unit/world mapping clamps unit-space inputs into `[0, 1]`.
- One-cell grids are supported intentionally. Their unit/world extent collapses to a single cell-size span anchored at the configured origin.
- `core_space_fit_scale(...)` is a normalization helper with deterministic fallback defaults for non-positive or non-finite scale inputs.
- `core_space_import_to_world(...)` validates descriptor state, rejects non-finite import payloads, and leaves the output transform unchanged on failure.

## Ownership Boundary
- `core_space` owns grid/window/import placement math only.
- Projection, camera behavior, viewport gestures, scene parsing, asset loading, object insertion, renderer transforms, persistence, and 3D frame semantics remain host-owned.

## Status
- Patch-hardened shared spatial contract with boundary-condition unit tests.
