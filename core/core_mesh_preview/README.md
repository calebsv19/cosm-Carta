# core_mesh_preview

Shared runtime mesh-preview contract for viewport-safe mesh visualization.

## Scope
- `core_mesh_preview_runtime_v1` sidecar payloads derived from
  `mesh_asset_runtime_v1` documents
- App-neutral local bounds, source vertex/triangle counts, source asset
  metadata, preview mode, sampled edge count, and drawable local-space edges
- Feature-edge preview generation with a bounded maximum edge budget
- File-backed preview save/load helpers
- Backward-compatible read support for the first
  `line_drawing_mesh_runtime_preview_v1` sidecars

## Boundaries
- No UI, selection, renderer, hitbox, camera, or scene placement policy
- No STL parsing or authoring-to-runtime compile ownership
- No GPU buffers, BVHs, solver proxies, collision meshes, SDFs, or retopology
- No mutation of source runtime mesh documents

## Current Contract (v0.1.0)
- Supported schema variant:
  - `core_mesh_preview_runtime_v1`
- Supported preview mode:
  - `feature_edges_v1`
- Preview payloads preserve:
  - `asset_id`
  - `source_asset_id`
  - `runtime_path`
  - local bounds
  - source vertex and triangle counts
  - source feature-edge count
  - sampled edge count
  - local-space edge endpoints

## Status
- Initial shared module promoted from the `line_drawing` STL/runtime mesh
  preview sidecar path so high-triangle imported meshes can use bounded
  viewport-safe preview data without forcing full triangle rendering.
