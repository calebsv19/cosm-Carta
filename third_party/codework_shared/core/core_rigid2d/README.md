# core_rigid2d

`core_rigid2d` is the shared UI-free 2D rigid-body scaffold for deterministic
body descriptors, mass/inertia helpers, minimal integration, and contact solver
primitives.

Current version: `0.1.1`

## Scope

The first surface depends on `core_collision2d` for vectors, shapes, and
contact manifolds. It owns:

- rigid-body material records;
- rigid-body state records using `CoreCollision2DShape` by value;
- circle, box, polygon, and shape inertia helpers;
- dynamic and static body initialization;
- mass reset and validation helpers;
- minimal body integration for host-owned fixed-step loops;
- normal, angular, and friction contact solver primitives.

## Boundaries

`core_rigid2d` does not own broadphase, contact discovery, worlds, scenario
catalogs, fixture names, summary strings, CLI routes, worker behavior, review
artifacts, rendering, UI, package behavior, Visualizer publication, or default
runtime policy.

`core_sim` remains the fixed-step control-plane owner. Hosts call
`core_rigid2d_body_integrate` from their own loop or shared `core_sim` pass;
this module does not accumulate frame time or choose substep policy.

## Build And Test

```sh
make -C shared/core/core_rigid2d test
```

## Change Notes

- `0.1.0`: Initial scaffold with material/body, inertia, integration, and
  contact solver primitives over `core_collision2d`.
- `0.1.1`: Adds a typed parity harness that freezes the first Ball Bounce
  rigid-body, polygon-inertia, normal solver, angular solver, and friction
  solver oracle values without linking Ball Bounce or exporting summary
  strings.
