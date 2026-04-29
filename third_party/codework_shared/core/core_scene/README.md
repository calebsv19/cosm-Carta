# core_scene

Shared app-neutral scene contract helpers for cross-program handoff.

Current scope:
- Typed shared scene-root contract helpers:
  - space-mode vocabulary (`2d` / `3d`)
  - canonical scene-root metadata validation (`scene_id`, mode intent/default, unit kind, world scale)
- Typed shared object contract helpers:
  - canonical object-kind vocabulary for the first supported authoring objects
  - canonical primitive payload validation for:
    - `plane_primitive`
    - `rect_prism_primitive`
  - object-contract validation layered on top of `core_object`
- Existing path/source helpers for scene bundle ingestion.
- Shared detection for bundle/source file types used by app-specific loaders.
- Shared bundle resolver (`core_scene_bundle_resolve`) for:
  - `fluid_source.path`
  - optional `scene_metadata.camera_path`
  - optional `scene_metadata.light_path`
  - optional `scene_metadata.asset_mapping_profile`

Dependencies:
- `core_base`
- `core_object`
- `core_units`

Status:
- additive typed-scene bootstrap is now in place for the first `LD3D-SC1` shared-scene promotion slice.
- exporter remap and compile-lane normalization are intentionally left to later slices.
