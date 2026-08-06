# core_viewport3d

Renderer-neutral 3D editor viewport navigation state and pure transition math.

## Contract

- double-precision effective viewport-center target
- canonical azimuth/elevation orientation in radians
- right-handed world with `+Z` up and screen `+Y` down
- pixels-per-world-unit scale with caller-supplied positive limits
- camera-basis pan, anchor-preserving zoom, orbit, frame, reset, and resize transitions
- projected-extents fit-scale resolution with caller-supplied fill fraction
- invalid-input rejection without output mutation

The canonical basis is:

```text
forward     = ( cos(a) cos(e),  sin(a) cos(e),  sin(e))
right       = ( sin(a),        -cos(a),         0)
screen_down = (-cos(a) sin(e), -sin(a) sin(e),  cos(e))
```

## Dependencies

- `core_base` for `CoreResult`

The module intentionally owns its domain-specific double vector type. The
current `core_math` API remains float-only and does not need an additive
`Vec3d` surface solely for this contract.

## Boundaries

This module does not own input gestures, SDL events, selection or bounds
resolution, renderer/projector matrices, picking, gizmos, overlays, mesh
caches, materials, final render cameras, geometry, BVHs, persistence, or scene
packet vocabulary. Apps translate their runtime camera/projector state through
thin adapters.

## Version

- `0.1.0`: initial standalone contract and deterministic fixtures.
