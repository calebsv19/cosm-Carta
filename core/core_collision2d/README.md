# core_collision2d

`core_collision2d` is the shared, UI-free 2D collision contract.

Version: `0.2.0`

## Scope

The bootstrap surface owns deterministic, app-neutral 2D collision primitives:

- double-precision vectors
- AABBs
- circle, axis-aligned box, and convex polygon descriptors
- polygon geometry helpers
- contact manifolds
- primitive shape-vs-shape contacts for circle/circle, axis-aligned box/box,
  and convex polygon/polygon
- bounded compound descriptors over primitive shapes, with validation, local
  AABB, area/mass, center-of-mass, and inertia helpers

## Boundaries

This module does not own:

- app scenario names or fixture catalogs
- room/floor/wall convenience contacts
- generated-mask descriptors, fixture catalogs, or decomposition policy
- compound contact generation or selected-part attribution
- rigid-body material/state ownership, integration, or impulse solving
- CLI routes, string summary schemas, render/review artifacts, workers,
  package behavior, Visualizer publication, or runtime default policy

## Dependencies

`core_collision2d` is standalone C11 plus the C math library. It does not
depend on `core_sim`, rendering/UI libraries, or Ball Bounce app code.

## Verification

```bash
make -C shared/core/core_collision2d test
```

The test target runs the standalone API test, the Ball Bounce parity harness
seed test, and a deterministic hardening harness for circle contact edge cases,
primitive metamorphic checks, box/box fixture edges, and polygon/polygon
fixture edges.

## Change Notes

- `0.2.0`: Added the first additive compound descriptor surface: bounded
  primitive parts, init/validate helpers, local AABB computation, area/mass,
  center-of-mass, and inertia helpers. Compound contacts, generated-mask
  fixtures, and selected-part attribution remain deferred.
- `0.1.3`: Hardened deterministic polygon/polygon contact tests for separated,
  near-separated, touching, overlapping, mixed-winding, translated, swapped,
  rotated, duplicate-vertex, and collinear fixtures without changing public API
  or runtime behavior.
- `0.1.2`: Hardened deterministic box/box contact tests for axis tie-breaks,
  swapped body order, containment/min-axis behavior, and invalid-input
  rejection without changing public API or runtime behavior.
- `0.1.1`: Added the shared collision parity harness seed that pins the first
  Ball Bounce shape, geometry, manifold, and primitive-contact fixture
  expectations as typed shared tests without linking Ball Bounce. Added
  deterministic hardening tests for the P14-S3 circle default-cutover readiness
  lane without changing the public API or runtime behavior.
- `0.1.0`: Bootstrap shared collision API and standalone tests for the first
  extraction surface.
