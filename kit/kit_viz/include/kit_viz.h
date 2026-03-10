#ifndef KIT_VIZ_H
#define KIT_VIZ_H

#include <stddef.h>
#include <stdint.h>

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KitVizFieldStats {
    float min_value;
    float max_value;
    float mean_value;
} KitVizFieldStats;

typedef struct KitVizVecSegment {
    float x0;
    float y0;
    float x1;
    float y1;
} KitVizVecSegment;

typedef enum KitVizColormap {
    KIT_VIZ_COLORMAP_GRAYSCALE = 0,
    KIT_VIZ_COLORMAP_HEAT = 1
} KitVizColormap;

CoreResult kit_viz_compute_field_stats(const float *values,
                                       uint32_t width,
                                       uint32_t height,
                                       KitVizFieldStats *out_stats);

CoreResult kit_viz_build_heatmap_rgba(const float *values,
                                      uint32_t width,
                                      uint32_t height,
                                      float min_value,
                                      float max_value,
                                      KitVizColormap colormap,
                                      uint8_t *out_rgba,
                                      size_t out_rgba_size);

CoreResult kit_viz_build_vector_segments(const float *vx,
                                         const float *vy,
                                         uint32_t width,
                                         uint32_t height,
                                         uint32_t stride,
                                         float scale,
                                         KitVizVecSegment *out_segments,
                                         size_t max_segments,
                                         size_t *out_segment_count);

CoreResult kit_viz_build_polyline_segments(const float *xs,
                                           const float *ys,
                                           uint32_t point_count,
                                           KitVizVecSegment *out_segments,
                                           size_t max_segments,
                                           size_t *out_segment_count);

CoreResult kit_viz_sample_waveform_envelope(const float *mins,
                                            const float *maxs,
                                            uint32_t bucket_count,
                                            double bucket_start,
                                            double bucket_scale,
                                            uint32_t pixel_width,
                                            float *out_mins,
                                            float *out_maxs,
                                            size_t out_count);

#ifdef __cplusplus
}
#endif

#endif
