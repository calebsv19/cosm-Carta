# core_scene_compile

Shared authoring-to-runtime scene compiler for the CodeWork scene pipeline.

## Purpose
Compile `scene_authoring_v1` JSON into deterministic `scene_runtime_v1` JSON that downstream apps such as `ray_tracing`, `physics_sim`, and `line_drawing` can consume.

## Current Scope (v0.8.0)
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
- emits a deterministic runtime scene envelope whose `compile_meta` binds the
  compiler version, normalization version, authoring SHA-256, dependency-set
  SHA-256, and dependency count. Publication time is deliberately excluded.
- exposes canonical dependency-manifest construction, SHA-256/provenance
  results, a create-only atomic directory publisher for
  `scene_authoring.json`, `scene_runtime.json`, `scene_dependencies.json`, and
  `scene_export_receipt.json`, plus a strict consumer verification API.
- derives content-addressed payload paths as
  `dependencies/<kind>/<sha256>`, materializes caller-retained bytes inside the
  atomic staging directory, and rejects missing, mismatched, or symlink-backed
  payloads during consumer verification.
- optionally retains an app-authored `scene_package.json` entrypoint inside
  the same atomic transaction, binds its digest and byte count into the export
  receipt and bundle identity, and verifies it before consumer acceptance.
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

## 2026-08-22 Update (v0.5.0)
- made compiled runtime bytes deterministic for identical authoring and
  dependency inputs by moving publication time out of runtime metadata.
- added authoring, runtime, dependency-set, and bundle SHA-256 provenance.
- added durable staging-file writes followed by one create-only directory
  rename, so consumers never observe a partially published bundle.
- added the `scene_export_receipt_v1` publication receipt; see
  `shared/docs/SCENE_EXPORT_RECEIPT_V1.md`.

## 2026-08-22 Update (v0.6.0)
- added canonical `scene_dependency_manifest_v1` construction and inspection;
  entries sort by dependency kind and stable identity before digesting.
- publication now derives dependency provenance from the manifest and includes
  `scene_dependencies.json` in the atomic directory transaction.
- added strict bundle verification for required artifacts, receipt fields,
  byte counts, dependency counts, artifact digests, runtime provenance,
  compiler compatibility, bundle identity, and an optional externally expected
  bundle digest.

## 2026-08-22 Update (v0.7.0)
- advanced the canonical dependency document to
  `scene_dependency_manifest_v2`, with a deterministic bundle-relative payload
  path on every entry.
- added fail-closed content-addressed payload publication inside the existing
  create-only atomic directory transaction.
- added payload byte-count, digest, missing-file, tamper, and no-follow path
  verification before a bundle can be accepted.

## 2026-08-23 Update (v0.8.0)
- added the optional, backward-compatible package-manifest publication field.
- package manifests are staged atomically with the canonical scene artifacts
  and are receipt- and bundle-digest-bound when present.

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
