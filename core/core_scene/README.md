# core_scene

Shared app-neutral scene contract helpers for cross-program handoff.

## Current Scope
- Typed shared scene-root contract helpers:
  - space-mode vocabulary (`2d` / `3d`)
  - canonical scene-root metadata validation (`scene_id`, mode intent/default, unit kind, world scale)
- Typed shared object contract helpers:
  - canonical object-kind vocabulary for the first supported authoring objects
  - canonical primitive payload validation for:
    - `plane_primitive`
    - `rect_prism_primitive`
  - canonical non-primitive contract validation for:
    - `mesh_asset_instance`
  - shared `geometry_ref.kind` vocabulary for:
    - `shape_asset`
    - `mesh_asset`
  - object-contract validation layered on top of `core_object`
- Existing path/source helpers for scene-bundle ingestion.
- Shared detection for bundle/source file types used by app-specific loaders.
- Shared bundle resolver (`core_scene_bundle_resolve`) for:
  - `fluid_source.path`
  - optional `scene_metadata.camera_path`
  - optional `scene_metadata.light_path`
  - optional `scene_metadata.asset_mapping_profile`

## Current Contract
- `core_scene_root_contract_init(...)` seeds a deterministic 2D + meters + `world_scale = 1.0` root scaffold, but callers must still assign a non-empty `scene_id`.
- `core_scene_root_contract_validate(...)` accepts only known space modes and known `CoreUnitKind` values, then defers world-scale validation to `core_units`.
- `core_scene_space_mode_parse(...)`, `core_scene_object_kind_parse(...)`, and `core_scene_geometry_ref_kind_parse(...)` expose fixed current vocabulary only and clear their output enums to `UNKNOWN` on failure.
- `core_scene_object_contract_prepare(...)` maps object kinds onto `core_object` identity + dimensional-mode policy:
  - `rect_prism_primitive` promotes to full 3D
  - `mesh_asset_instance` promotes to full 3D
  - all other current supported kinds prepare as plane-locked `XY`
- Primitive payload ownership is narrow:
  - `plane_primitive` requires a plane-locked object plus only plane payload
  - `rect_prism_primitive` requires a full-3D object plus only prism payload
  - `mesh_asset_instance` requires a full-3D object and no primitive payload
  - other non-primitive kinds must not carry primitive payloads
- `core_scene_dirname(...)` and `core_scene_resolve_path(...)` are string helpers only. They preserve dot segments and do not canonicalize or enforce security policy.
- `core_scene_bundle_resolve(...)` uses bounded string scanning, not a full JSON parser. It resolves supported source paths plus optional camera/light/profile metadata, but malformed or ambiguous JSON semantics remain intentionally limited to the current scanner behavior.

## Boundaries
- `core_scene` owns typed scene-root/object/primitive contracts plus scene-bundle source/path resolution helpers.
- `core_object` owns base object identity, transform, dimensional-mode, and primitive-independent validation semantics.
- `core_units` owns scalar unit vocabulary and world-scale validation semantics.
- `core_scene_compile` owns authoring-to-runtime normalization, runtime JSON emission, compile metadata, extension preservation, and overlay/diff helpers.
- Apps own retained scene stores, renderer/editor behavior, asset loading, import UX, overlay/writeback policy, and solver/runtime semantics.

## Dependencies
- `core_base`
- `core_object`
- `core_units`

## Status
- Additive typed-scene bootstrap (`v1.2.0`) now recognizes `mesh_asset_instance` as a shared non-primitive object kind and exposes shared `geometry_ref.kind` vocabulary for `shape_asset` and `mesh_asset`.
