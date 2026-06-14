# kit_viz

`kit_viz` is a small visualization helper kit for `core_data`-style field arrays.

## Current API
- `kit_viz_compute_field_stats(...)`
- `kit_viz_build_heatmap_rgba(...)`
- `kit_viz_build_vector_segments(...)`
- `kit_viz_build_polyline_segments(...)`
- `kit_viz_sample_waveform_envelope(...)`

## Boundary

`kit_viz` owns:
1. field-stat derivation over borrowed scalar arrays
2. heatmap RGBA buffer generation over borrowed scalar arrays
3. vector/polyline segment derivation over borrowed scalar arrays
4. waveform envelope resampling helpers over borrowed min/max buckets

`kit_viz` does not own:
1. renderer submission or backend policy
2. dataset/schema ownership
3. chart styling policy beyond the current fixed colormap vocabulary
4. axis labels, legend text, hover semantics, or app-specific diagnostics meaning

## Current Contract

1. All array inputs are borrowed caller-owned storage.
2. All builders write into caller-owned output buffers only.
3. Invalid input returns `CORE_ERR_INVALID_ARG`.
4. Field stats reject zero-sized arrays, null pointers, and non-finite source values.
5. Heatmap generation currently supports only `KIT_VIZ_COLORMAP_GRAYSCALE` and `KIT_VIZ_COLORMAP_HEAT`.
6. Heatmap, vector, polyline, and waveform helpers reject non-finite control values instead of propagating `NaN` or `Inf`.
7. `kit_viz_build_vector_segments(...)` uses `stride == 0` as `1`.
8. `kit_viz_build_polyline_segments(...)` treats fewer than two points as a valid empty result.
9. `kit_viz_sample_waveform_envelope(...)` fills out-of-range pixels with zeroed min/max values instead of extrapolating.

## Build
```sh
make -C shared/kit/kit_viz
```

## Test
```sh
make -C shared/kit/kit_viz test
```
