# Physics Trio Scene Project v1 Contract

Status: stable local interoperability contract

Schema: `codework_scene_project_v1`

Programs: LineDrawing, PhysicsSim, RayTracing

Last updated: 2026-07-18

## Purpose

A scene project is the durable unit exchanged by the Physics Trio. It keeps
editable scene state, compiled runtime state, portable assets, simulation
caches, render intent, and export lineage under one movable directory. Programs
must discover attached state from this directory instead of reconstructing
unrelated workspace paths.

This document is the stable public contract. App docs may describe their own
commands and schemas, but must not redefine cross-program ownership or relocate
v1's required root files.

## Required Root

Every v1 project has this minimum shape:

```text
<scene-project>/
  scene_project.json
  scene_authoring.json
  scene_runtime.json
  object_manifest.json
  assets/
    mesh_assets/
  line_drawing/
  physics_sim/
    runs/
  ray_tracing/
    runs/
  worker_export/
```

`scene_authoring.json` and `scene_runtime.json` are mandatory root files for
v1. `scene_project.json` may contain pointers to them, but consumers must not
use those pointers to relocate either file in this schema version. Flexible
authoring/runtime locations require a later compatible contract or schema.

The initial manifest written by LineDrawing contains:

```json
{
  "schema": "codework_scene_project_v1",
  "project_name": "example",
  "created_by": "line_drawing",
  "created_at": "2026-07-18T00:00:00Z",
  "updated_at": "2026-07-18T00:00:00Z",
  "authoring_scene": "scene_authoring.json",
  "runtime_scene": "scene_runtime.json",
  "object_manifest": "object_manifest.json",
  "mesh_assets_dir": "assets/mesh_assets",
  "active_cache": "physics_sim/active_cache_manifest.json",
  "active_render_request": "ray_tracing/render_request.json"
}
```

All portable pointers are project-relative. Absolute paths, empty path
segments, `.` segments, `..` traversal, and symlink escapes outside the project
must be rejected on write and on portable export.

## Ownership

| State | Normal writer | Other programs |
|---|---|---|
| `scene_project.json` and project scaffold | LineDrawing | Read; update only through an explicitly defined project-state operation |
| `scene_authoring.json` | LineDrawing | Read only |
| `scene_runtime.json` | LineDrawing through `core_scene_compile` | Read only |
| `object_manifest.json` and `assets/mesh_assets/` | LineDrawing | Read only |
| `physics_sim/active_cache_manifest.json`, `physics_sim/runs/`, `assets/vf3d/`, and `assets/physics/` | PhysicsSim | Read only |
| `ray_tracing/render_request.json`, render runs, and review output | RayTracing | Read only |
| `worker_export/` snapshots | Exporting program/tool | Derived snapshot only; never canonical truth |

No downstream program may silently mutate LineDrawing authoring truth.
PhysicsSim must not own render settings. RayTracing must not become the solver
or canonical authoring host.

Shared libraries keep their existing boundaries:

- `core_scene` owns app-neutral scene structures and schema validation.
- `core_scene_compile` owns authoring-to-runtime compilation.
- `core_mesh_asset` and related mesh compile/preview modules own reusable mesh
  document contracts.
- `core_headless_job` owns the outer job envelope, not app-specific project,
  cache, or render-request policy.

The v1 scene-project policy remains cross-program coordination plus app-local
adapters; it does not introduce a new shared runtime module.

## Lifecycle

1. LineDrawing creates or updates the authored scene, compiles the paired
   runtime scene, and writes portable mesh sidecars under the project root.
2. PhysicsSim resolves the root pair, reads runtime state, and writes an
   immutable run plus an explicit active-cache manifest under the same root.
3. RayTracing resolves the project, reads the selected cache, and owns the
   project render request. It preserves unknown request fields during
   supported writeback.
4. A worker export snapshots the selected scene, assets, cache frame window,
   and render request with hashes and lineage. It is a transport artifact, not
   the source project.
5. Imported results and receipts attach to the project without replacing its
   authoring/runtime truth.

Loose runtime-scene inputs and the original scene-only worker export remain
supported compatibility paths. They do not acquire project mutation rights.

## Derived-State Rules

- Recompiling may replace `scene_runtime.json` only from the current
  `scene_authoring.json`.
- Active cache slots may change only through an explicit PhysicsSim cache
  update or retained-run promotion.
- `physics_sim/runs/<run-id>/` and `ray_tracing/runs/<run-id>/` are immutable by
  default after completion.
- Scratch render frames may be overwritten; retained runs and prepared worker
  exports may not be silently overwritten.
- Missing optional cache or render state means “not produced yet,” not an
  invitation for another program to fabricate ownership.
- A failed write must not leave a partially promoted active pointer. Writers
  should stage, validate, then publish the new manifest or active selection.

## Compatibility And Evolution

The schema string is the compatibility boundary.

Additive v1 changes may:

- add optional manifest fields or program-owned subdirectories;
- add new retained-run metadata;
- add optional assets whose absence has a defined fallback;
- preserve and round-trip unknown fields where an app already supports
  read-modify-write behavior.

A new schema or explicitly documented compatibility revision is required to:

- relocate either required root scene file;
- change which program normally writes canonical state;
- change a pointer from project-relative to another addressing model;
- make an optional downstream artifact mandatory;
- reinterpret an existing field incompatibly.

Before a program version adopts a contract change, its change must land with:

1. an updated shared contract and app-specific docs;
2. a deterministic fixture covering the old and new accepted shape;
3. producer, consumer, and relocation tests;
4. preserved loose-scene and scene-only compatibility unless their removal is
   separately versioned and announced;
5. a coordinated source/version matrix proving the exact trio combination.

Program versions remain independent. A LineDrawing, PhysicsSim, or RayTracing
version bump does not automatically authorize a project-schema change, remote
registration, package build, deployment, or release.

## Stability Checklist

Future changes are ready only when all applicable answers are yes:

- Does the project still move to a different absolute directory without edits?
- Are both required root scene files present and semantically paired?
- Does LineDrawing remain the only normal authoring writer?
- Does PhysicsSim write only project-local cache/run state?
- Does RayTracing keep render intent project-relative and reject escapes?
- Are unknown supported request fields preserved on writeback?
- Does a worker package contain hashes and source lineage while remaining a
  disposable snapshot?
- Do missing optional downstream artifacts produce a clear, non-destructive
  state?
- Do the three exact source versions pass the focused dataflow and broad trio
  regression gates together?

The workspace drift guard is:

```sh
python3 -m unittest tests.test_physics_trio_scene_project_v1_contract
```

The guard intentionally binds this document to the current producer and
consumer constants. If it fails after a legitimate evolution, update the
contract and fixtures in the same change rather than weakening the check.

## Current Boundary

The local v1 communication and dataflow contract is complete and suitable as
the baseline for future program versions. Remaining Physics Trio work concerns
separately authorized worker-profile registration, builds, promotion, release,
deployment, and live readback. Those production operations do not redefine
the scene-project contract and must not be inferred from local source readiness.
