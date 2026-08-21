# Viewport3D Shared Contract State

Last updated: 2026-07-18

This reference records the stable shared boundary currently proven by the
RayTracing and LineDrawing 3D editors, with PhysicsSim participating only in
screen-space object selection.

## Current shared layers

| Layer | Version | Shared ownership | Proven hosts |
| --- | --- | --- | --- |
| `core_viewport3d` | `0.1.0` | Double-precision effective target, radian orientation, right/screen-down/forward basis, pixels-per-world-unit scale, pan, anchored zoom, orbit, frame, reset, resize, fit-scale, validation, and invalid-input nonmutation | RayTracing, LineDrawing |
| `kit_viewport3d` | `0.1.0` | Stable object accents and CPU color/depth/owner-buffer composition for silhouettes, relative depth edges, and owner boundaries | RayTracing, LineDrawing |
| `core_screen_pick` | `0.1.0` | Projected-candidate storage, 32-pixel hashed grid, 28-pixel default capture radius, deterministic nearest/ranked queries, and transactional rebuilds | RayTracing, LineDrawing, PhysicsSim |

The layers are deliberately independent. Apps may use navigation without the
presentation kit, presentation without shared picking, or picking without
adopting the 3D navigation state.

## Coordinate and ranking contracts

`core_viewport3d` uses a right-handed world with `+Z` up and logical screen
`+Y` down. Orientation is azimuth/elevation in radians and scale is logical
pixels per world unit. It owns a module-local double vector type because the
generic `core_math` surface remains float-only.

`core_screen_pick` receives one app-projected eligible anchor per object in
viewport-local logical pixels. Results rank by squared screen distance, then
greater view depth inside the distance tie epsilon, then lower stable key.
Apps decide candidate eligibility and interpret the opaque payload.

## Host adapters

RayTracing keeps its durable target, angle conversion, projector and dynamic
zoom-domain state. Its adapter maps those values into `core_viewport3d`, and
its whole-object picker projects authored primitive and mesh-instance origins.
Exact mesh, triangle, face, and material selection remain local.

LineDrawing keeps `FreeViewCamera`, degree orientation, Grid scale and view
offsets. Its adapter derives the effective 3D target, applies shared
transitions, and converts back without changing authoring arbitration. Its
whole-object picker indexes projected visual centers on the existing hitbox
rebuild lifecycle; handles, gizmos, and topology retain priority.

PhysicsSim has not adopted `core_viewport3d`. It uses `core_viewport2d` for the
retained 2D editor and uses `core_screen_pick` only for projected object-body
nearest/ranked queries. Its 3D orbit camera, hit-stack arbitration, imports,
emitters, boundaries, drag policy, and repeat-click cycling remain local.

## Explicitly app-owned

- platform events, gesture/button policy, pointer capture, and modal routing;
- camera/projector matrices and renderer-specific camera storage;
- scene and selection bounds resolution, frame-target choice, and padding policy;
- visibility, clipping, occlusion, picking rays, GPU picking, and BVHs;
- selection state, hover state, drag lifecycle, handles, gizmos, topology, and face picking;
- rasterization, GPU resources, texture upload, cache invalidation, adaptive quality, materials, overlays, and final-render cameras;
- scene mutation, persistence, authoring arbitration, and product-specific UI.

`core_scene_view` remains read-only packet vocabulary and does not become a
camera, input, or selection owner.

## Future extraction decision table

| Candidate | Current decision | Evidence required before extraction |
| --- | --- | --- |
| Generic double vector in `core_math` | Deferred | At least one additional generic double-precision consumer whose API would otherwise duplicate the same operations; `core_viewport3d` alone is insufficient. |
| Projector or camera-pose ABI | Deferred | Two hosts must expose the same renderer-neutral input/output representation without importing renderer matrices or app zoom policy. |
| Gesture mapping | Keep app-local | Equivalent gestures are insufficient; pointer capture, modal, gizmo, pane, and authoring arbitration must also match across hosts. A future optional kit is preferable to core ownership. |
| Bounds/selection framing policy | Keep app-local | Apps must first prove identical eligibility, bounds, fallback, and padding semantics over equivalent scenes. The shared core may continue receiving only the resolved target and scale. |
| Multi-anchor or bounds-aware screen picking | Deferred | Real scenes must prove one projected origin is inadequate in the same way across multiple hosts, with deterministic fixtures and a clear stable-key policy. |
| Occlusion-aware or GPU picking | Keep app-local | Renderer/backend behavior and visibility policy currently differ. Do not add them to `core_screen_pick`. |
| Picker rebuild/cache scheduling | Keep app-local | Projection invalidation and frame lifecycle differ. The core owns only transactional index replacement and query semantics. |
| Outline thickness, dilation, guide hierarchy, or GPU composition | Deferred | RayTracing and LineDrawing must first prove identical renderer-neutral semantics. Palette and guide-alpha tuning should remain local until then. |
| Combined viewport mega-module | Rejected | Navigation meaning, optional presentation, and projected selection are separate capabilities and should remain composable modules. |
| PhysicsSim `core_viewport3d` adoption | Optional later audit | Prove that its 3D orbit/target/scale semantics match the canonical effective-target contract without disturbing its established 2D editor path or solver/editor arbitration. |

## Resume checklist

1. Read this reference and the three module README/public headers.
2. Inspect current app adapters and current worktree drift; do not assume the
   2026-07-17 verification baseline is still current.
3. Run standalone shared tests before proposing API growth.
4. Prove the missing behavior with equivalent cross-app fixtures before
   extending a shared contract.
5. Prefer additive API changes and one-host-at-a-time rollout.
6. Keep integration commits separate from managed subtree refresh commits.
7. Re-run focused, broad, package, installed-app, docs, and Atlas gates in that
   order when runtime behavior changes.
