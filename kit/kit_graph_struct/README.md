# kit_graph_struct

`kit_graph_struct` is the shared structural graph visualization kit built on top of `kit_render`.

It provides reusable node/edge visualization helpers for generic structures without tying rendering to any specific app domain.

## Current Scope (Phase B)

This module now provides:

- generic node and edge contracts
- simple layered tree-style layout
- layered DAG layout
- layered DAG crossing-reduction sweeps for improved row ordering
- viewport pan/zoom transform input
- node hit-testing
- node focus helper for viewport recentering
- node/edge drawing with hover and selection states
- centered layered row placement for improved visual balance
- deterministic same-layer ordering by label/id for more stable refresh behavior
- additive edge route computation helpers (straight and orthogonal modes with boundary attachment)
- text-fit node width sizing through layout style controls
- optional measured node-label width callback in `KitGraphStructLayoutStyle` for font-accurate node sizing
- computed edge-label layout rectangles with collision-lane offsets
- edge-label placement that avoids node-rectangle overlap
- additive routed edge-label placement with density controls (show-all, overlap-cull, zoom-threshold hide)
- additive edge-route and edge-label hit-test helpers for app interaction contracts
- additive viewport helper utilities for pan, zoom, and layout centering

The scope is still intentionally narrow. This is a reusable structural-graph foundation, not a force-directed or debugger-specific system.

## Boundary

`kit_graph_struct` owns:

- generic node/edge visualization
- layout from graph data into renderable rectangles
- generic node selection hit-testing
- structural graph presentation helpers
- simple viewport focus helpers

`kit_graph_struct` does not own:

- app-specific object semantics
- debugger meaning
- persistence formats
- pane lifecycle

## Progress

Implemented now:

1. shared node, edge, viewport, and layout contracts
2. layered tree-style layout helper
3. layered DAG layout helper
4. node hit-testing helper
5. node focus helper
6. node/edge drawing through `kit_render`
7. Vulkan validation harness
8. edge-label layout helper for app-side semantic edge badges

## Planned Growth

1. add radial layout
2. add expand/collapse hooks
3. add stronger focus/select sets for larger graphs
4. deepen integration into the shared showcase app
5. expand validation fixtures and timing instrumentation for layout/routing audits

## Build

```sh
make -C shared/kit/kit_graph_struct
```

## Test

```sh
make -C shared/kit/kit_graph_struct test
```

## Validation Harness

Build the Vulkan validation harness:

```sh
make -C shared/kit/kit_graph_struct clean validation-harness KIT_RENDER_ENABLE_VK=1
```

Run it:

```sh
./shared/kit/kit_graph_struct/build/kit_graph_struct_vk_validation
```

Expected behavior:

1. a left info panel with graph notes and controls
2. a right structural graph canvas
3. the `Tree` and `DAG` segmented control switches the layout mode
4. `Fixture: ...` cycles graph fixtures (`TREE`, `DAG`, `CHAIN`, `STAR`, `CYCLE`, `MIXED`)
5. mouse wheel zoom adjusts graph scale
6. dragging with the left mouse button pans
7. clicking a node selects it and `Focus Selected` recenters it

Press `Esc` or close the window to exit.
