# core_mesh_preview

Shared runtime mesh-preview contract for viewport-safe mesh visualization.

## Scope
- `core_mesh_preview_runtime_v1` sidecar payloads derived from
  `mesh_asset_runtime_v1` documents
- App-neutral local bounds, source vertex/triangle counts, source asset
  metadata, preview mode, sampled payload counts, and drawable local-space
  preview geometry
- Feature-edge, triangle-sample, point-cloud, and bounds-proxy preview
  generation with bounded budgets where drawable samples are emitted
- File-backed preview save/load helpers, runtime-file build/save helpers,
  metadata-only loading, and preview-file probing
- Renderer-neutral coherent indexed LOD construction from canonical runtime
  meshes with host-selected triangle budgets
- Backward-compatible read support for the first
  `line_drawing_mesh_runtime_preview_v1` sidecars

## Boundaries
- No UI, selection, renderer, hitbox, camera, or scene placement policy
- No STL parsing or authoring-to-runtime compile ownership
- No GPU buffers, BVHs, solver proxies, collision meshes, SDFs, or retopology
- No mutation of source runtime mesh documents

## Current Contract (v0.5.0)
- Supported schema variant:
  - `core_mesh_preview_runtime_v1`
- Supported preview modes:
  - `feature_edges_v1`
  - `triangle_samples_v1`
  - `point_cloud_v1`
  - `bounds_proxy_v1`
- Preview payloads preserve:
  - `asset_id`
  - `source_asset_id`
  - `runtime_path`
  - runtime asset schema variant
  - optional source format/unit/scale hints
  - local bounds
  - bounds center and extent
  - max span
  - bounding sphere center and radius
  - source vertex and triangle counts
  - source feature-edge count
  - preview vertex, edge, and triangle counts
  - maximum preview budget
  - sampling strategy
  - coverage ratio
  - local-space edge endpoints, sampled triangles, sampled points, or
    bounds-only metadata depending on preview mode
- Runtime-file helpers:
  - `core_mesh_preview_build_from_runtime_file(...)`
  - `core_mesh_preview_save_for_runtime_file(...)`
- Metadata/probe helpers:
  - `core_mesh_preview_load_metadata_only(...)`
  - `core_mesh_preview_probe_file(...)`
- Coherent indexed LOD helpers:
  - `core_mesh_preview_build_lod_mesh(...)`
  - `core_mesh_preview_lod_mesh_init(...)`
  - `core_mesh_preview_lod_mesh_free(...)`

## Status
- v0.5.0 promotes LineDrawing's proven vertex-cluster LOD builder into an
  additive renderer-neutral API. Hosts choose triangle budgets and retain all
  projection, shading, renderer-cache, interaction, and quality-settle policy.
- RayTracing is the second proving host: its scene editor consumes the coherent
  LOD through app-native Vulkan triangle/line meshes, direct
  Bounds/Wire/Solid/Material controls, and app-owned picking and frame-slot
  caches. Final render geometry, material/light/camera policy, and BVHs remain
  outside this module.
- v0.4.0 adds the S3 file/performance path for hosts that only have runtime
  mesh file paths: runtime-file build/save helpers, metadata-only JSON sidecar
  loading that does not allocate drawable arrays, file probing with size and
  schema/readability flags, and shared tests for runtime-file, metadata-only,
  probe, and legacy sidecar reads. Compact binary payloads remain deferred
  until multi-tier JSON sidecars settle.
- v0.3.0 adds deterministic mode-specific builders and JSON round-trip
  support for feature-edge, sampled-triangle, point-cloud, and bounds-only
  preview payloads. The existing `core_mesh_preview_build_runtime_payload`
  entry point remains the feature-edge compatibility path, while
  `core_mesh_preview_build_runtime_payload_with_mode` exposes the tiered
  preview contract for future RayTracing and PhysicsSim adoption.
- v0.2.0 hardens the initial preview sidecar contract with precise source vs
  preview counters, derived bounds/span/sphere metadata, sample-strategy
  metadata, budget/coverage fields, and backward-compatible reads for the
  v0.1.0/LineDrawing sidecar shape.
- Initial shared module promoted from the `line_drawing` STL/runtime mesh
  preview sidecar path so high-triangle imported meshes can use bounded
  viewport-safe preview data without forcing full triangle rendering.

## Roadmap
- S1 contract hardening should make source counts, preview counts, bounds,
  spans, scale hints, sampling strategy, and fallback/degradation state
  explicit while preserving backward sidecar reads.
- S2 added the shared feature-edge, sampled-triangle, point-cloud, and
  bounds-only preview-mode contract. App-specific selection, hitbox, renderer,
  thumbnail, and diagnostics policies remain follow-on adoption work.
- S3 added runtime-file generation/probe APIs and metadata-only reads. Compact
  binary payloads remain a later decision after JSON sidecar budgets and tier
  shape settle.
- LineDrawing remains the first proving host and RayTracing now proves a second
  renderer-native consumer. PhysicsSim adoption should remain deferred until a
  concrete overlay slice is selected, with final-render BVHs and
  solver/collision truth kept out of this core module.
- Private execution plan:
  `docs/private_program_docs/shared/active/2026-06-14_core_mesh_preview_upgrade_plan.md`
