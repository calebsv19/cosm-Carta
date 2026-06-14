# kit_graph_struct

`kit_graph_struct` is the shared structural-graph presentation kit layered on top of `kit_render`.

It owns graph-shaped layout, route, label-placement, hit, viewport, and simple draw helpers for generic node/edge structures. It does not own app semantics, retained graph models, richer graph editing UX, persistence, or renderer/backend behavior.

## Current Surface

Implemented now:

- layered tree layout
- layered DAG layout with bounded crossing-reduction sweeps
- text-measured node width sizing through optional callback hooks
- straight and orthogonal edge-route helpers with boundary attachment
- edge-label placement helpers for direct and routed edges
- node, edge-route, and edge-label hit helpers
- viewport pan, zoom, centering, and focus helpers
- simple graph draw submission through `kit_render`

## Contract

`kit_graph_struct` owns:

- generic node/edge identity and layout helpers
- deterministic same-layer ordering by label/id
- graph viewport math over shared layout results
- simple render-command submission for nodes and straight center-to-center edges

`kit_graph_struct` does not own:

- Memory Console or other app-specific graph semantics
- retained selection/focus stores
- routed-edge draw fidelity or semantic edge badges in host UIs
- persistence formats or graph import/export
- Vulkan/SDL backend policy beyond the optional manual validation harness

## Input Rules

- node ids must be unique for layout and draw helpers
- layout arrays used by focus/route helpers must carry unique `node_id` values
- `edges` must be non-null whenever `edge_count > 0`
- queued node labels are borrowed strings; they must stay valid through frame submission because `kit_render` text commands borrow the pointer
- viewport helpers require finite pan/zoom state; zoom must stay positive
- layout/style and routed-label option floats are expected to be finite

## Current Behavior Notes

- tree layout and DAG layout both stay intentionally simple; cycle-heavy input falls back to bounded layered placement rather than full graph-theory correctness
- hit helpers treat right and bottom rect boundaries as inclusive
- `kit_graph_struct_draw(...)` intentionally remains minimal: it does not draw routed-edge geometry or edge-label overlays
- routed edge-label density currently supports only `show all` and `overlap cull`

## Build

```sh
make -C shared/kit/kit_graph_struct
```

## Test

```sh
make -C shared/kit/kit_graph_struct test
```

## Validation Harness

The Vulkan harness is still a manual validation path:

```sh
make -C shared/kit/kit_graph_struct clean validation-harness KIT_RENDER_ENABLE_VK=1
./shared/kit/kit_graph_struct/build/kit_graph_struct_vk_validation
```

It is useful for visual checks, but it is not yet an automated parity gate.
