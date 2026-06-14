# core_mesh_asset

Shared reusable 3D mesh-asset contract semantics for the physics trio object-authoring lane.

## Scope
- Typed authored mesh-asset contract helpers for `mesh_asset_authoring_v1`
- Primitive-seed authoring document helpers for the first editable object-asset lane
- Imported-mesh source metadata for STL-backed authored assets
- Typed runtime mesh-asset contract helpers for `mesh_asset_runtime_v1`
- Shared vocabulary for:
  - asset schema variants
  - asset type
  - authoring source mode
  - imported mesh source format
  - primitive seed kind
- Shared validation for:
  - asset identity
  - unit/world-scale pairing
  - pivot/local-frame semantics
  - primitive-seed object payloads
  - imported mesh provenance, unit, orientation, weld, and topology diagnostics
  - runtime mesh counts, payload arrays, local bounds, and surface groups
  - topology expectation flags
- File-backed authoring-document load/save helpers for exporter/importer adapters
- File-backed runtime-document load helpers for `mesh_asset_runtime_v1`
- File-backed runtime-document save helpers for generated runtime mesh assets

## Boundaries
- No mesh editing operations
- No triangulation, extrusion, revolve, or boolean implementation
- No scene-instance ownership (`core_scene` remains the long-term scene envelope owner)
- No render acceleration structures, GPU buffers, solver voxelization, or SDF ownership

## Current Contract (v0.5.2)
- Supported schema variants are exactly:
  - `mesh_asset_authoring_v1`
  - `mesh_asset_runtime_v1`
- Supported asset types are exactly:
  - `solid_mesh`
- Supported primitive seed kinds are exactly:
  - `plane`
  - `rect_prism`
- Supported authoring source modes are exactly:
  - `profile_extrusion`
  - `primitive_seed`
  - `revolve`
  - `imported_mesh`
- Supported imported mesh source formats are exactly:
  - `stl`
- `core_mesh_asset_authoring_contract_validate(...)` currently validates only shared semantic lanes:
  - non-empty `asset_id`
  - known `unit_kind`
  - positive finite `world_scale`
  - known `asset_type`
  - known `source_mode`
  - finite pivot vectors with non-degenerate local frame axes
- `core_mesh_asset_authoring_document_*` now owns the first file-backed authoring lane:
  - `primitive_seed` object payload arrays
  - shared object/transform/flags metadata per primitive
  - plane primitive payloads
  - rect-prism primitive payloads
  - `imported_mesh` source metadata for STL provenance, unit policy,
    scale-to-asset conversion, orientation policy, default surface group,
    vertex welding policy, source-normal policy, and topology diagnostics
  - fixture-compatible save/load for `mesh_asset_authoring_v1`
- `core_mesh_asset_runtime_contract_validate(...)` validates shared runtime semantics:
  - non-empty `asset_id` and `source_asset_id`
  - known `asset_type`
  - positive `vertex_count` and `triangle_count`
  - finite local bounds with `min <= max` on every axis
  - finite pivot vectors with non-degenerate local frame axes
- `core_mesh_asset_runtime_document_*` now owns the first file-backed runtime mesh lane:
  - vertex payload arrays with finite local-space positions
  - triangle payload arrays with index range and non-degeneracy validation
  - local-bounds containment checks for every vertex
  - surface group ids and triangle spans
  - fixture-compatible load/save for `mesh_asset_runtime_v1`

## Status
- Bootstrap contract module for the shared mesh-asset lane.
- First fixtures now cover both the editable `primitive_seed` authoring document lane and runtime mesh payloads ahead of richer mesh-compile integration.
- v0.5.2 keeps the `mesh_asset_runtime_v1` schema stable and switches runtime
  document save to streaming temp-file output so large imported meshes can
  materialize runtime JSON above the previous fixed print-buffer cap.
- v0.5.1 keeps file-save behavior and schema output compatible while avoiding
  cJSON realloc-backed print buffers for fisiCs memory-check allocator
  tracking.
- v0.5.0 adds file-backed `mesh_asset_runtime_v1` save support so generated
  runtime mesh assets can be materialized for downstream app fixture
  consumption.
- v0.4.0 adds the additive `imported_mesh` authoring source lane and the first
  `stl` import-source metadata contract. STL remains an import source feeding
  authored asset truth; runtime triangulation, parser behavior, editor UX, and
  renderer/solver derived forms remain outside this module.
- v0.3.1 keeps the same runtime-document contract and switches runtime JSON
  array parsing to linear cJSON traversal so larger mesh payloads do not pay
  linked-list indexed-access costs.
- v0.3.0 adds runtime-document payload validation and file loading, with tests covering the RayTracing MRT0 low/medium/high sphere fixtures.
