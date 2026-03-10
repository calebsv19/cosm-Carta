# core_scene

Shared scene-connection helpers for cross-program handoff.

Current scope:
- Path/source helpers for scene bundle ingestion.
- Shared detection for bundle/source file types used by app-specific loaders.
- Shared bundle resolver (`core_scene_bundle_resolve`) for:
  - `fluid_source.path`
  - optional `scene_metadata.camera_path`
  - optional `scene_metadata.light_path`
  - optional `scene_metadata.asset_mapping_profile`

This module is intentionally small and dependency-light so apps can build higher-level import adapters on top.
