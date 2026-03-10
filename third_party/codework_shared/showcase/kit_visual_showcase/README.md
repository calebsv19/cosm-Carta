# kit_visual_showcase

`kit_visual_showcase` is a shared demo app that exercises the currently active shared visual kits together.

It is not a reusable kit. It is the integration proving ground for:

- `kit_render`
- `kit_ui`
- `kit_graph_timeseries`
- `kit_graph_struct`

## Purpose

This app provides one place to validate cross-kit behavior instead of relying only on isolated per-kit harnesses.

## Current Views

1. `Render`: shared primitive rendering and clipping
2. `UI`: shared widget composition and interaction
3. `Time Series`: shared time-series plotting and hover overlays
4. `Struct Graph`: shared node/edge layout, pan, zoom, and selection

## Build

```sh
make -C shared/showcase/kit_visual_showcase
```

## Run

```sh
./shared/showcase/kit_visual_showcase/build/kit_visual_showcase
```

## Expected Behavior

1. the window opens at `1440x900` and the content scales with the current window size
2. the left rail is now a concise showcase navigator:
- four buttons switch between the four kit views
- one global checkbox toggles overlay labels
- no slider, event-log, or test-specific clutter lives in the rail
3. each view shows its own representative API-focused info panel inside the content area
4. interaction is handled inside the active view:
- `UI` view has live widget interaction
- `Time Series` view zooms on mouse wheel over the plot
- `Struct Graph` view zooms on wheel, pans on drag, selects on click, and exposes `Tree` / `DAG` plus `Focus Selected`

Press `Esc` or close the window to exit.
