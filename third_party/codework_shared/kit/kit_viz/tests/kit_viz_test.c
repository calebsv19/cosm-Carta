#include "kit_viz.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int test_stats(void) {
    const float values[6] = { 0.0f, 1.0f, 2.0f, -1.0f, 0.5f, 0.0f };
    KitVizFieldStats stats;
    CoreResult r = kit_viz_compute_field_stats(values, 3, 2, &stats);
    if (r.code != CORE_OK) {
        fprintf(stderr, "test_stats: expected ok, got %d\n", (int)r.code);
        return 1;
    }
    if (fabsf(stats.min_value + 1.0f) > 1e-6f) return 1;
    if (fabsf(stats.max_value - 2.0f) > 1e-6f) return 1;
    if (fabsf(stats.mean_value - 0.41666666f) > 1e-5f) return 1;
    return 0;
}

static int test_heatmap_rgba(void) {
    const float values[4] = { 0.0f, 1.0f, 2.0f, 3.0f };
    uint8_t rgba[16];
    memset(rgba, 0, sizeof(rgba));
    CoreResult r = kit_viz_build_heatmap_rgba(values,
                                              2,
                                              2,
                                              0.0f,
                                              3.0f,
                                              KIT_VIZ_COLORMAP_GRAYSCALE,
                                              rgba,
                                              sizeof(rgba));
    if (r.code != CORE_OK) {
        fprintf(stderr, "test_heatmap_rgba: expected ok, got %d\n", (int)r.code);
        return 1;
    }
    if (rgba[0] != 0 || rgba[1] != 0 || rgba[2] != 0 || rgba[3] != 255) return 1;
    if (rgba[12] != 255 || rgba[13] != 255 || rgba[14] != 255 || rgba[15] != 255) return 1;
    return 0;
}

static int test_vector_segments(void) {
    const float vx[9] = { 0.0f, 1.0f, 2.0f, -1.0f, 0.0f, 1.0f, -2.0f, -1.0f, 0.0f };
    const float vy[9] = { 0.0f, 0.5f, 0.0f, -0.5f, 0.0f, 0.5f, 0.0f, -0.5f, 0.0f };
    KitVizVecSegment segs[9];
    size_t seg_count = 0;
    CoreResult r = kit_viz_build_vector_segments(vx, vy, 3, 3, 2, 0.5f, segs, 9, &seg_count);
    if (r.code != CORE_OK) {
        fprintf(stderr, "test_vector_segments: expected ok, got %d\n", (int)r.code);
        return 1;
    }
    if (seg_count != 4) {
        fprintf(stderr, "test_vector_segments: expected 4 segs, got %zu\n", seg_count);
        return 1;
    }
    if (fabsf(segs[0].x0 - 0.5f) > 1e-6f || fabsf(segs[0].y0 - 0.5f) > 1e-6f) return 1;
    if (fabsf(segs[3].x0 - 2.5f) > 1e-6f || fabsf(segs[3].y0 - 2.5f) > 1e-6f) return 1;
    return 0;
}

static int test_waveform_sampling(void) {
    const float mins[4] = {-1.0f, -0.5f, -0.25f, 0.0f};
    const float maxs[4] = {1.0f, 0.5f, 0.25f, 0.0f};
    float out_mins[4] = {0};
    float out_maxs[4] = {0};

    CoreResult r = kit_viz_sample_waveform_envelope(mins,
                                                    maxs,
                                                    4,
                                                    0.0,
                                                    0.5,
                                                    4,
                                                    out_mins,
                                                    out_maxs,
                                                    4);
    if (r.code != CORE_OK) {
        fprintf(stderr, "test_waveform_sampling: expected ok, got %d\n", (int)r.code);
        return 1;
    }

    if (fabsf(out_mins[0] + 1.0f) > 1e-6f) return 1;
    if (fabsf(out_maxs[0] - 1.0f) > 1e-6f) return 1;
    if (fabsf(out_mins[1] + 0.75f) > 1e-6f) return 1;
    if (fabsf(out_maxs[1] - 0.75f) > 1e-6f) return 1;
    if (fabsf(out_mins[2] + 0.5f) > 1e-6f) return 1;
    if (fabsf(out_maxs[2] - 0.5f) > 1e-6f) return 1;
    return 0;
}

static int test_polyline_segments(void) {
    const float xs[4] = {0.0f, 1.0f, 2.5f, 2.5f};
    const float ys[4] = {0.0f, 0.5f, 0.5f, 1.0f};
    KitVizVecSegment segs[3];
    size_t seg_count = 0;

    CoreResult r = kit_viz_build_polyline_segments(xs, ys, 4, segs, 3, &seg_count);
    if (r.code != CORE_OK) {
        fprintf(stderr, "test_polyline_segments: expected ok, got %d\n", (int)r.code);
        return 1;
    }
    if (seg_count != 3) return 1;
    if (fabsf(segs[0].x0 - 0.0f) > 1e-6f || fabsf(segs[0].y0 - 0.0f) > 1e-6f) return 1;
    if (fabsf(segs[2].x1 - 2.5f) > 1e-6f || fabsf(segs[2].y1 - 1.0f) > 1e-6f) return 1;
    return 0;
}

int main(void) {
    if (test_stats() != 0) return 1;
    if (test_heatmap_rgba() != 0) return 1;
    if (test_vector_segments() != 0) return 1;
    if (test_waveform_sampling() != 0) return 1;
    if (test_polyline_segments() != 0) return 1;

    puts("kit_viz tests passed");
    return 0;
}
