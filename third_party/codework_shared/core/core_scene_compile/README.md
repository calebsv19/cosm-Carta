# core_scene_compile

Shared authoring-to-runtime scene compiler for the CodeWork scene pipeline.

## Purpose
Compile `scene_authoring_v1` JSON into deterministic `scene_runtime_v1` JSON that downstream apps such as `ray_tracing`, `physics_sim`, and `line_drawing` can consume.

## Current Scope (v0.4.0)
- owns the shared authoring-to-runtime normalization boundary only:
  - validates core authoring contract keys and semantic lanes,
  - emits deterministic runtime JSON with normalized canonical lanes,
  - validates canonical primitive payloads for the current primitive object kinds,
  - preserves unknown extension namespaces from authoring input.
- validates current root and reference gates:
  - `space_mode_default` must be `2d` or `3d`,
  - `unit_system` must be string,
  - `world_scale` must be a positive number,
  - object/material uniqueness and `material_ref.id` resolution,
  - hierarchy parent/child object reference integrity,
  - additive fallback generation for missing light/camera ids.
- emits runtime scene envelope with `compile_meta` including the current normalization marker.
- emits deterministic normalized runtime lanes:
  - `objects`, `hierarchy`, `materials`, `lights`, `cameras`,
  - stable ordering by ID (and parent/child pair for hierarchy).
- validates canonical primitive payloads for known primitive object kinds:
  - `object_type=plane_primitive` requires a valid `objects[].primitive`,
  - `object_type=rect_prism_primitive` requires a valid `objects[].primitive`,
  - malformed primitive payloads are rejected before runtime handoff,
  - validated canonical `primitive` payload remains intact in `scene_runtime_v1`.
- validates staged shared geometry-reference semantics for reusable assets:
  - existing `geometry_ref.id` handling remains backward-compatible,
  - `geometry_ref.kind` now accepts shared vocabulary from `core_scene`,
  - `object_type=mesh_asset_instance` requires `geometry_ref.kind=mesh_asset`,
  - `mesh_asset_instance` must remain `full_3d` and carries no primitive payload.
- includes helper surfaces around the compiler boundary:
  - `tools/scene_contract_diff.c` performs semantic diff checks for runtime scene drift,
  - `include/core_scene_overlay_merge_shared.h` centralizes overlay metadata validation and shared writeback merge guards for app bridges.

## Parser Contract (current)
- the compiler uses a bounded internal string-slice parser for the current scene contract; it is not a general JSON engine.
- top-level key lookup matches the first unescaped key occurrence only.
- duplicate-key semantics are not normalized; the first matching top-level key wins.
- escaped key names are not canonicalized during lookup.
- the file-to-file helper is a whole-file `core_io` wrapper around the in-memory compile API.

## Helper Surface Contract (current)
- `scene_contract_diff` is a verification/tooling helper, not the retained runtime-scene owner.
- `core_scene_overlay_merge_shared.h` is a bridge helper for overlay metadata validation, namespace gating, producer-clock guards, and canonical `space_mode_default` conflict handling.
- app hosts still own:
  - retained runtime-scene storage,
  - renderer/editor/solver behavior,
  - asset loading/import UX,
  - app-specific overlay merge policy beyond the shared guardrails here.

## Non-Goals (current)
- full hierarchy flattening and graph solve,
- parser replacement or general-purpose JSON normalization,
- binary/pack output generation,
- app-specific override merge policy,
- retained runtime-scene ownership, renderer behavior, or editor UX.

## 2026-05-25 Update (v0.4.0)
- extended the shared scene compiler to recognize `mesh_asset_instance` and shared `geometry_ref.kind` vocabulary.
- kept legacy `shape_asset` geometry references backward-compatible while adding staged validation for `mesh_asset`.

## 2026-05-16 Update (v0.3.1)
- truth-locked the bounded parser and helper-surface boundary.
- added malformed compile-path, file-wrapper, overlay-merge, and semantic-diff edge coverage.

## 2026-04-12 Update (v0.3.0)
- implemented `SC3` primitive-contract hardening for the trio shared-scene lane.
- runtime normalization now explicitly validates canonical primitive payloads through `core_scene` instead of relying on generic object JSON pass-through.

## 2026-04-01 Update (v0.2.0)
- implemented NP-3 normalization/validation hardening for trio next-phase rollout.
- runtime output now includes normalized `hierarchy` lane (preserved/validated from authoring).
