# Scene Bundle v1

`scene_bundle.json` is the cross-program handoff file for loading scenes between apps.

## Minimal shape

```json
{
  "bundle_type": "physics_scene_bundle_v1",
  "bundle_version": 1,
  "profile": "physics",
  "fluid_source": {
    "kind": "manifest",
    "path": "../../../physics_sim/export/volume_frames/M Air/manifest.json"
  },
  "scene_metadata": {
    "camera_path": "../../../ray_tracing/Configs/scene_config.json",
    "light_path": "../../../ray_tracing/Configs/config.json",
    "asset_mapping_profile": "physics_to_ray_v1"
  }
}
```

## Required fields

- `bundle_type`: must contain `scene_bundle`.
- `bundle_version`: integer schema version (`1`).
- `fluid_source.path`: relative or absolute path to one of:
  - `manifest.json`
  - `.pack`
  - `.vf2d`

## Optional fields

- `profile`: producer app profile (`physics`, etc).
- `fluid_source.kind`: hint (`manifest`, `pack`, `vf2d`).
- `scene_metadata.camera_path`: optional camera source path for downstream app.
- `scene_metadata.light_path`: optional light source path for downstream app.
- `scene_metadata.asset_mapping_profile`: optional import mapping profile id.
- `notes`: free-form text.

## Resolution rules

- Relative paths are resolved from the directory containing `scene_bundle.json`.
- Absolute paths are used directly.

## RayTracing behavior

- If a selected scene path is a `scene_bundle.json`, RayTracing resolves `fluid_source.path` first.
- Then normal import flow is used with existing fallback behavior.
