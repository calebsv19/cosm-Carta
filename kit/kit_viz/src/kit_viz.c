/*
 * kit_viz.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "kit_viz.h"

#include <math.h>
#include <string.h>

static float clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

CoreResult kit_viz_compute_field_stats(const float *values,
                                       uint32_t width,
                                       uint32_t height,
                                       KitVizFieldStats *out_stats) {
    if (!values || !out_stats || width == 0 || height == 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    const size_t count = (size_t)width * (size_t)height;
    float min_v = values[0];
    float max_v = values[0];
    double sum = 0.0;

    for (size_t i = 0; i < count; ++i) {
        const float v = values[i];
        if (v < min_v) min_v = v;
        if (v > max_v) max_v = v;
        sum += (double)v;
    }

    out_stats->min_value = min_v;
    out_stats->max_value = max_v;
    out_stats->mean_value = (float)(sum / (double)count);
    return core_result_ok();
}

static void colormap_grayscale(float t, uint8_t *r, uint8_t *g, uint8_t *b) {
    const uint8_t c = (uint8_t)(t * 255.0f);
    *r = c;
    *g = c;
    *b = c;
}

static void colormap_heat(float t, uint8_t *r, uint8_t *g, uint8_t *b) {
    const float s = clamp01(t);
    if (s < 0.25f) {
        const float k = s / 0.25f;
        *r = 0;
        *g = (uint8_t)(k * 255.0f);
        *b = 255;
        return;
    }
    if (s < 0.5f) {
        const float k = (s - 0.25f) / 0.25f;
        *r = 0;
        *g = 255;
        *b = (uint8_t)((1.0f - k) * 255.0f);
        return;
    }
    if (s < 0.75f) {
        const float k = (s - 0.5f) / 0.25f;
        *r = (uint8_t)(k * 255.0f);
        *g = 255;
        *b = 0;
        return;
    }
    {
        const float k = (s - 0.75f) / 0.25f;
        *r = 255;
        *g = (uint8_t)((1.0f - k) * 255.0f);
        *b = 0;
    }
}

CoreResult kit_viz_build_heatmap_rgba(const float *values,
                                      uint32_t width,
                                      uint32_t height,
                                      float min_value,
                                      float max_value,
                                      KitVizColormap colormap,
                                      uint8_t *out_rgba,
                                      size_t out_rgba_size) {
    if (!values || !out_rgba || width == 0 || height == 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    const size_t px_count = (size_t)width * (size_t)height;
    const size_t needed = px_count * 4u;
    if (out_rgba_size < needed) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "output buffer too small" };
        return r;
    }

    float range = max_value - min_value;
    if (fabsf(range) < 1e-20f) {
        range = 1.0f;
    }

    for (size_t i = 0; i < px_count; ++i) {
        const float v = values[i];
        const float t = clamp01((v - min_value) / range);

        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        if (colormap == KIT_VIZ_COLORMAP_GRAYSCALE) {
            colormap_grayscale(t, &r, &g, &b);
        } else {
            colormap_heat(t, &r, &g, &b);
        }

        const size_t p = i * 4u;
        out_rgba[p + 0] = r;
        out_rgba[p + 1] = g;
        out_rgba[p + 2] = b;
        out_rgba[p + 3] = 255;
    }

    return core_result_ok();
}

CoreResult kit_viz_build_vector_segments(const float *vx,
                                         const float *vy,
                                         uint32_t width,
                                         uint32_t height,
                                         uint32_t stride,
                                         float scale,
                                         KitVizVecSegment *out_segments,
                                         size_t max_segments,
                                         size_t *out_segment_count) {
    if (!vx || !vy || !out_segments || !out_segment_count || width == 0 || height == 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (stride == 0) stride = 1;

    size_t count = 0;
    for (uint32_t y = 0; y < height; y += stride) {
        for (uint32_t x = 0; x < width; x += stride) {
            const size_t idx = (size_t)y * (size_t)width + (size_t)x;
            if (count >= max_segments) {
                *out_segment_count = count;
                CoreResult r = { CORE_ERR_INVALID_ARG, "segment buffer too small" };
                return r;
            }

            const float cx = (float)x + 0.5f;
            const float cy = (float)y + 0.5f;
            const float dx = vx[idx] * scale;
            const float dy = vy[idx] * scale;

            out_segments[count].x0 = cx;
            out_segments[count].y0 = cy;
            out_segments[count].x1 = cx + dx;
            out_segments[count].y1 = cy + dy;
            count++;
        }
    }

    *out_segment_count = count;
    return core_result_ok();
}

CoreResult kit_viz_build_polyline_segments(const float *xs,
                                           const float *ys,
                                           uint32_t point_count,
                                           KitVizVecSegment *out_segments,
                                           size_t max_segments,
                                           size_t *out_segment_count) {
    if (!xs || !ys || !out_segments || !out_segment_count) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (point_count < 2) {
        *out_segment_count = 0;
        return core_result_ok();
    }

    size_t needed = (size_t)point_count - 1u;
    if (max_segments < needed) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "segment buffer too small" };
        return r;
    }

    for (uint32_t i = 0; i + 1u < point_count; ++i) {
        out_segments[i].x0 = xs[i];
        out_segments[i].y0 = ys[i];
        out_segments[i].x1 = xs[i + 1u];
        out_segments[i].y1 = ys[i + 1u];
    }
    *out_segment_count = needed;
    return core_result_ok();
}

CoreResult kit_viz_sample_waveform_envelope(const float *mins,
                                            const float *maxs,
                                            uint32_t bucket_count,
                                            double bucket_start,
                                            double bucket_scale,
                                            uint32_t pixel_width,
                                            float *out_mins,
                                            float *out_maxs,
                                            size_t out_count) {
    if (!mins || !maxs || !out_mins || !out_maxs || bucket_count < 2 ||
        pixel_width == 0 || out_count < (size_t)pixel_width) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if (!(bucket_scale > 0.0)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid bucket scale" };
        return r;
    }

    for (uint32_t px = 0; px < pixel_width; ++px) {
        double pos = bucket_start + (double)px * bucket_scale;
        int bucket = (int)floor(pos);
        if (bucket < 0 || bucket + 1 >= (int)bucket_count) {
            out_mins[px] = 0.0f;
            out_maxs[px] = 0.0f;
            continue;
        }

        double t = pos - (double)bucket;
        float min_a = mins[bucket];
        float max_a = maxs[bucket];
        float min_b = mins[bucket + 1];
        float max_b = maxs[bucket + 1];
        out_mins[px] = (float)((1.0 - t) * (double)min_a + t * (double)min_b);
        out_maxs[px] = (float)((1.0 - t) * (double)max_a + t * (double)max_b);
    }

    return core_result_ok();
}
