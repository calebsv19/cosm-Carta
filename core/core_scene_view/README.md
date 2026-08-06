# core_scene_view

`core_scene_view` owns the renderer-free scene-view packet vocabulary used by
apps to exchange preview/readback metadata without sharing editor behavior.

Version: `0.2.0`

## Scope

- schema family/variant constants for scene-view packets
- preview quality, degraded reason, display flags, and pick-id vocabulary
- compact JSON readback validation for packet metadata
- compact packet summary derivation from validated readback metadata

## Boundary

- Does not own rendering, viewport input, picking policy, material sampling, or
  editor mutation.
- Does not create, delete, move, resize, or rewrite scene objects.
- App producers still own packet construction/serialization until more hosts
  prove the contract.
- App consumers still own local object/face mapping and UI behavior.

## Version Notes

- `0.2.0`: adds a renderer-free `CoreSceneViewPacketSummary` helper derived
  from `CoreSceneViewPacketReadback`. The summary carries read-only display
  booleans, projected/complete state, counts, first/last face-group ids, alpha,
  preview quality, and degraded reason. It does not add labels, rendering,
  picking, camera behavior, packet serialization, or mutation authority.

## Current Proof

- RayTracing produces `ray_tracing_scene_view_packet_v0` JSON locally.
- LineDrawing consumes the same JSON through a read-only fixture.
- PhysicsSim consumes the same JSON through an app-local read-only readout
  fixture.
