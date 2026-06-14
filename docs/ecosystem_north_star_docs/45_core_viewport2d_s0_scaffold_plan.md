# core_viewport2d S0 Scaffold Plan

## Goal

Create `shared/core/core_viewport2d` as the shared, renderer-agnostic 2D viewport/camera math layer for:
- fit-to-window resets
- screen-to-content/content-to-screen transforms
- drag-pan state updates
- cursor-anchor zoom behavior

The first proving host is `datalab`, specifically BMP/image inspection for large render outputs that exceed the visible window.

## Why A New Core Module

`core_space` already owns placement and unit/world conversion semantics.
It should not become the owner of app-facing interactive viewport state.

`map_forge` already demonstrates the strongest cursor-pivot behavior, but it is map-domain code:
- it depends on world/projection semantics
- it carries camera target/smoothing behavior
- its zoom units are not generic image-space units

`drawing_program` already has useful local transform vocabulary, but its current wheel zoom updates zoom alone and does not preserve the canvas point under the cursor.

So the reusable contract should be:
- more generic than `map_forge`
- more correct for cursor-pivot zoom than current `drawing_program`
- narrower than either app's full input/render behavior

## Ownership Boundary

`core_viewport2d` owns:
- viewport state (`pan_x`, `pan_y`, `zoom`, bounds)
- fit-to-window math
- screen/content transform conversion
- pan-by-delta updates
- anchor-preserving zoom state transitions

`core_viewport2d` does not own:
- SDL event parsing
- mouse button policy
- drag-start/end gesture policy
- renderer backends or texture upload
- map projection
- world/import placement semantics

## S0 Scope

This slice establishes only the shared scaffold and the minimum proving math contract:
- `VERSION`
- `README`
- public header
- source implementation
- baseline tests
- shared-doc registration

S0 intentionally does not:
- wire DataLab runtime usage yet
- refactor DrawingProgram
- replace MapForge camera ownership
- add smoothing, inertia, or bounds clamping policy

## Initial API Shape

The S0 API should stay small and pure:
- initialize/validate viewport state
- clamp zoom
- convert screen -> content
- convert content -> screen
- pan by screen delta
- zoom at a screen anchor
- reset to fit a content rectangle inside a view rectangle

This is enough to prove the contract that DataLab needs first.

## DataLab Adoption Plan

`datalab` should be the first host because its image/BMP path is currently the simplest and most clearly deficient:
- it already loads BMP/image content into `drawing_rgba`
- it currently renders through a single fit rect
- it does not yet carry viewport state, so adoption can be explicit rather than tangled with legacy behavior

Planned adoption order after S0:
1. Add DataLab viewport state fields and reset-to-fit behavior.
2. Route image/sketch render derive through `core_viewport2d`.
3. Add wheel-anchor zoom and drag-pan in the DataLab image lane.
4. Validate behavior on oversized BMP outputs.
5. Reassess extraction opportunities for DrawingProgram and MapForge.

## Validation Gates For S0

1. Build/compile gate:
   - `make -C shared/core/core_viewport2d test`
2. Targeted test gate:
   - lock pivot preservation for anchor zoom
   - lock fit-to-window math for oversized content
3. Broad regression/smoke gate:
   - none required beyond the shared-module scaffold in S0

## Exit Condition

S0 is complete when:
- the shared module exists with a tested pure-math API
- shared docs describe its purpose and boundary
- DataLab is named as the first proving host
- no app integration has been started yet
