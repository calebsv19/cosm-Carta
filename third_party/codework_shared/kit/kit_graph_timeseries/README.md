# kit_graph_timeseries

`kit_graph_timeseries` is the shared time-series visualization kit built on top of `kit_render`.

It provides reusable plotting helpers for line-series visualization without tying plotting behavior to any single app.

## Current Scope (Phase 2)

This module now provides:

- view-range computation from sample data
- line plot rendering for one or more series
- plot clipping
- optional axes and grid lines
- simple legend labels
- hover inspection helper for nearest-point lookup
- hover crosshair and marker overlay drawing
- view zoom helper
- render-stride guidance for lightweight decimation of dense sample sets

The current scope is intentionally focused on practical plotting fundamentals. It is the first reusable graphing layer for DataLab-style and metrics-style visualization.

## Boundary

`kit_graph_timeseries` owns:

- plotting transforms from data space to plot space
- generic line-series rendering
- generic plot presentation helpers
- graph-specific hover inspection and overlay helpers
- simple draw-time decimation guidance

`kit_graph_timeseries` does not own:

- file formats
- dataset storage policy
- app-specific analysis transforms
- pane lifecycle
- domain-specific meaning of the data

## Progress

Implemented now:

1. shared plot and series contracts
2. automatic view computation from sample buffers
3. line plot rendering through `kit_render`
4. grid and axes support
5. hover inspection helper
6. hover overlay drawing
7. zoom helper
8. decimation/stride helper for dense series
9. Vulkan validation harness
10. internal split between math helpers and draw helpers so apps can reuse plotting math incrementally
11. first app adoption slice landed: DataLab now reuses shared stride guidance for trace decimation

## Planned Growth

1. add panning helpers
2. add multi-plot dashboard helpers
3. expand toward spectrum and heatmap variants
4. support richer inspector composition for app-level tools

## Build

```sh
make -C shared/kit/kit_graph_timeseries
```

The static library is rebuilt from a clean archive member list each time so removed objects do not linger after internal refactors.

## Test

```sh
make -C shared/kit/kit_graph_timeseries test
```

## Validation Harness

Build the Vulkan validation harness:

```sh
make -C shared/kit/kit_graph_timeseries clean validation-harness KIT_RENDER_ENABLE_VK=1
```

Run it:

```sh
./shared/kit/kit_graph_timeseries/build/kit_graph_timeseries_vk_validation
```

Expected behavior:

1. a left control panel with series selection, overlay toggle, and detail slider
2. a right plot panel with a dense time-series graph
3. mouse wheel zoom changes the visible plot range
4. overlay mode shows a crosshair and hover marker that follows the nearest point
5. the info readout reflects the current render stride for the active detail level

Press `Esc` or close the window to exit.
