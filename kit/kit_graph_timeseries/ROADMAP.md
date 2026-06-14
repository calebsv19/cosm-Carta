# kit_graph_timeseries Roadmap

`kit_graph_timeseries` is currently a shared plotting helper kit for time-series line graphs on top of `kit_render`.

## Implemented Now

- view computation from series sample buffers
- zoom helper
- nearest-point hover inspection for a single selected series
- line plot rendering with axes, grid, legend, and hover overlay helpers
- render-stride guidance for dense-series decimation
- split math/draw implementation files
- manual Vulkan validation harness

## Deferred

1. style-aware or multi-series hover inspection
2. panning helpers
3. multi-plot dashboard composition helpers
4. spectrum/heatmap variants
5. broader app adoption lanes such as DataLab shared draw/hover migration

## Hardening Notes

- Version `0.2.2` truth-locks borrowed series/label ownership, default-padding hover expectations, and non-finite rejection.
- The baseline hardening pass also removes the unused combined implementation file to keep scanners and agents aligned with the actual build graph.
