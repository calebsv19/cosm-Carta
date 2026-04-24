# core_scene_compile

Shared baseline compiler for CodeWork scene pipeline.

## Purpose
Compile `scene_authoring_v1` JSON into deterministic `scene_runtime_v1` JSON that downstream apps (`ray_tracing`, `physics_sim`) can consume.

## Current Scope (v0.2.0)
- validates core authoring contract keys and semantic lanes:
  - `space_mode_default` must be `2d` or `3d`,
  - `unit_system` must be string,
  - `world_scale` must be a positive number,
- validates canonical ID/reference gates:
  - object/material uniqueness and `material_ref.id` resolution,
  - hierarchy parent/child object reference integrity,
  - light/camera IDs with additive fallback generation when omitted,
- emits runtime scene envelope with `compile_meta` including normalization marker,
- emits deterministic normalized runtime lanes:
  - `objects`, `hierarchy`, `materials`, `lights`, `cameras`,
  - stable ordering by ID (and parent/child pair for hierarchy),
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

## 2026-04-01 Update (v0.2.0)
- implemented NP-3 normalization/validation hardening for trio next-phase rollout.
- runtime output now includes normalized `hierarchy` lane (preserved/validated from authoring).
