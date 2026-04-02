# core_scene_compile

Shared baseline compiler for CodeWork scene pipeline.

## Purpose
Compile `scene_authoring_v1` JSON into deterministic `scene_runtime_v1` JSON that downstream apps (`ray_tracing`, `physics_sim`) can consume.

## Current Scope (v0.1.2)
- validates minimal authoring contract keys,
- validates canonical ID/reference gates (`object_id`, `material_id`, `material_ref.id`),
- emits runtime scene envelope with `compile_meta`,
- preserves canonical arrays/objects needed by runtime consumers,
- preserves unknown extension namespaces,
- includes `tools/scene_contract_diff.c` semantic diff utility for scene contract drift checks:
  - object-key order-insensitive comparison,
  - id-aware array comparison for canonical lanes (`objects/materials/lights/cameras/constraints`),
  - ignored volatile/runtime lanes (`compile_meta.compiled_at_ns`, `extensions.overlay_merge.*`),
- includes shared writeback-merge helper:
  - `include/core_scene_overlay_merge_shared.h`
  - centralizes overlay metadata validation, namespace/payload gates, producer-clock staleness guards, and canonical `space_mode_default` conflict policy for app bridges.

## Non-Goals (current)
- full hierarchy flattening and graph solve,
- app-specific override merge policy,
- binary/pack output generation.
