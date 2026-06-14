# core_authored_texture

Shared authored-texture manifest contract semantics for cross-app texture export/runtime handoff.

## Scope
- Manifest schema-version vocabulary
- Binding-kind vocabulary
- Emitted-output-kind vocabulary
- Supported authored-texture primitive vocabulary
- Face-role vocabulary and primitive-specific completeness rules
- Semantic-net layout/slot/orientation vocabulary and adjacency guards
- JSON-free manifest-contract validation helpers for exporter/loader adapters

## Boundaries
- No JSON parsing or writing
- No PNG/image IO
- No editor/runtime UI behavior
- No scene-envelope ownership (`core_scene` still owns scene/object semantics)
- No scene writeback helpers or runtime material sampling behavior

## Status
- Current bootstrap (`v0.1.2`) with semantic enums, primitive/face completeness helpers, semantic-net validation helpers, and a narrow manifest-contract validator.
- Bridge-first adoption is now live in:
  - `drawing_program` authored-texture export
  - `ray_tracing` authored-texture loader validation
- Current shared ownership is intentionally limited to manifest meaning and validation:
  - schema/version vocabulary
  - binding/output/primitive vocabulary
  - face-role semantics
  - primitive-specific completeness rules
  - semantic-net layout/slot/orientation vocabulary
  - semantic-net corner/edge/adjacency validation
  - JSON-free manifest-contract validation
- JSON parsing/writing, image IO, and app UX remain local to the host apps until a later lane proves a wider shared adapter is worth the rollout cost.

## Current Contract (v0.1.3)
- Supported schema versions are exactly `V1`, `V2`, and `V5`.
- Parse helpers are exact-token and case-sensitive today.
- Supported primitive kinds are exactly:
  - `PLANE`
  - `RECT_PRISM`
- Supported binding kinds are exactly:
  - `SEPARATE_FACES`
- Supported output kinds are exactly:
  - `LEGACY_FLATTENED`
  - `FLATTENED_ONLY`
  - `BASE_PLUS_OVERLAY`
- `core_authored_texture_manifest_contract_validate(...)` only validates the current shared schema/binding/output/surface matrix:
  - `V1` and `V2` require `LEGACY_FLATTENED` with legacy surfaces only
  - `V5` allows either `FLATTENED_ONLY` with base surfaces only or `BASE_PLUS_OVERLAY` with both base and overlay surfaces
- `core_authored_texture_semantic_net_validate(...)` is contract validation only:
  - plane nets require `PLANE` layout, `FRONT` slot/face, known orientation, and all corner/edge ids unset
  - prism nets require `PRISM_CROSS` layout, slot-to-face equality, known orientation, unique corner/edge ids, unique non-self adjacent roles, and current shared range limits
- Current validation does not own manifest files, exported image sets, texture project persistence, runtime scene material application, or editor/runtime UX.
