# core_object

Shared app-neutral object identity and transform contract for scene interchange.

## Scope
- Canonical object identity/type fields with copied bounded storage
- Dimensional mode (`plane_locked` / `full_3d`) and plane-lock policy
- Transform plus visibility/lock/select flags
- Deterministic validation and dimensional-rule enforcement helpers

## Dependencies
- `core_base`

## Status
- Patch-hardened bootstrap contract with boundary-condition unit tests.

## Current Contract
- `core_object_init(...)` produces deterministic defaults, but it does not assign a valid identity. Callers must still set both `object_id` and `object_type` before validation or interchange use.
- `object_id` and `object_type` each allow up to 63 bytes of caller text plus the terminating null byte.
- Valid dimensional modes are `CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED` and `CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D`.
- Valid locked planes are `CORE_OBJECT_PLANE_XY`, `CORE_OBJECT_PLANE_YZ`, and `CORE_OBJECT_PLANE_XZ`.
- `core_object_enforce_dimensional_rules(...)` only projects the position field onto the selected plane:
  - `XY` forces `position.z = 0`
  - `YZ` forces `position.x = 0`
  - `XZ` forces `position.y = 0`
- Plane enforcement does not modify rotation or scale.
- `core_object_validate(...)` requires non-empty identity fields, a valid dimensional mode, finite transform values, and strictly positive scale components.

## Ownership Boundary
- `core_object` owns app-neutral identity, transform, dimensional-mode, and validation semantics.
- Scene schemas, primitive payloads, renderer geometry, app object stores, texture semantics, and runtime meaning remain host-owned.
