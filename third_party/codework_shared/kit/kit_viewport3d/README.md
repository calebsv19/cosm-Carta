# kit_viewport3d

`kit_viewport3d` is an optional renderer-neutral presentation helper for 3D
editor viewports.

## v0.1.0 contract

- semantic object-outline palette with stable object accents plus selected and
  hover emphasis;
- one-pixel covered-surface silhouette detection;
- relative depth-discontinuity and object-owner boundary detection;
- filled-surface and outline-only composition;
- float or double depth-buffer input without owning either buffer.

## Boundary

The kit borrows caller-owned CPU color, depth, and optional owner buffers. It
does not own camera/navigation state, projection, rasterization, renderer or GPU
resources, cache lifetime, selection/picking, input gestures, scene objects, or
overlay visibility policy. `core_viewport3d` remains the pure navigation owner;
apps remain responsible for deciding when and where this presentation helper is
used.

RayTracing and LineDrawing are the first source-level proving hosts.

## Build and test

```sh
make -C shared/kit/kit_viewport3d test
```
