/*
 * core_space.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_space.h"

#include <math.h>

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

CoreResult core_space_desc_validate(const CoreSpaceDesc *desc) {
    if (!desc) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "desc is null" };
        return r;
    }
    if (desc->grid_w <= 0 || desc->grid_h <= 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid grid size" };
        return r;
    }
    if (desc->cell_size <= 0.0f) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "cell_size must be > 0" };
        return r;
    }
    if (desc->author_window_w <= 0 || desc->author_window_h <= 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid author window" };
        return r;
    }
    if (desc->desired_fit <= 0.0f) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "desired_fit must be > 0" };
        return r;
    }
    return core_result_ok();
}

CoreResult core_space_desc_default_from_grid(int grid_w,
                                             int grid_h,
                                             float origin_x,
                                             float origin_y,
                                             float cell_size,
                                             CoreSpaceDesc *out_desc) {
    if (!out_desc) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "out_desc is null" };
        return r;
    }
    out_desc->grid_w = grid_w;
    out_desc->grid_h = grid_h;
    out_desc->origin_x = origin_x;
    out_desc->origin_y = origin_y;
    out_desc->cell_size = (cell_size > 0.0f) ? cell_size : 1.0f;
    out_desc->author_window_w = 1200;
    out_desc->author_window_h = 800;
    out_desc->desired_fit = 0.25f;
    return core_space_desc_validate(out_desc);
}

CoreResult core_space_compute_span_from_window(int window_w,
                                               int window_h,
                                               float *out_span_x,
                                               float *out_span_y) {
    if (!out_span_x || !out_span_y) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "output pointer is null" };
        return r;
    }
    if (window_w <= 0 || window_h <= 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid window size" };
        return r;
    }

    float min_dim = (float)((window_w < window_h) ? window_w : window_h);
    if (min_dim <= 0.0f) min_dim = 1.0f;
    *out_span_x = 0.5f * ((float)window_w / min_dim);
    *out_span_y = 0.5f * ((float)window_h / min_dim);
    return core_result_ok();
}

float core_space_import_pos_to_unit(float pos_raw, float span_cfg) {
    if (span_cfg <= 0.0f) span_cfg = 1.0f;
    {
        float min_v = 0.5f - span_cfg;
        float max_v = 0.5f + span_cfg;
        float t = (pos_raw - min_v) / (max_v - min_v);
        return clamp01(t);
    }
}

float core_space_unit_to_world_x(const CoreSpaceDesc *desc, float unit_x) {
    if (!desc) return 0.0f;
    {
        float ux = clamp01(unit_x);
        float extent = (float)(desc->grid_w > 1 ? (desc->grid_w - 1) : 1) * desc->cell_size;
        return desc->origin_x + ux * extent;
    }
}

float core_space_unit_to_world_y(const CoreSpaceDesc *desc, float unit_y) {
    if (!desc) return 0.0f;
    {
        float uy = clamp01(unit_y);
        float extent = (float)(desc->grid_h > 1 ? (desc->grid_h - 1) : 1) * desc->cell_size;
        return desc->origin_y + uy * extent;
    }
}

float core_space_world_to_unit_x(const CoreSpaceDesc *desc, float world_x) {
    if (!desc) return 0.0f;
    {
        float extent = (float)(desc->grid_w > 1 ? (desc->grid_w - 1) : 1) * desc->cell_size;
        if (extent <= 0.0f) return 0.0f;
        return clamp01((world_x - desc->origin_x) / extent);
    }
}

float core_space_world_to_unit_y(const CoreSpaceDesc *desc, float world_y) {
    if (!desc) return 0.0f;
    {
        float extent = (float)(desc->grid_h > 1 ? (desc->grid_h - 1) : 1) * desc->cell_size;
        if (extent <= 0.0f) return 0.0f;
        return clamp01((world_y - desc->origin_y) / extent);
    }
}

float core_space_fit_scale(float import_scale,
                           float desired_fit,
                           float asset_max_dim,
                           int grid_w,
                           int grid_h,
                           float cell_size) {
    float gmin = (float)((grid_w < grid_h) ? grid_w : grid_h);
    if (gmin <= 0.0f) gmin = 1.0f;
    if (import_scale <= 0.0f) import_scale = 1.0f;
    if (desired_fit <= 0.0f) desired_fit = 0.25f;
    if (asset_max_dim <= 0.0001f) asset_max_dim = 1.0f;
    if (cell_size <= 0.0f) cell_size = 1.0f;

    {
        float norm = (import_scale * desired_fit) / asset_max_dim;
        return norm * gmin * cell_size;
    }
}

CoreResult core_space_import_to_world(const CoreSpaceDesc *desc,
                                      const CoreSpaceImport *in,
                                      CoreSpaceWorldTransform *out) {
    float span_x = 0.0f;
    float span_y = 0.0f;
    CoreResult vr;
    if (!desc || !in || !out) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    vr = core_space_desc_validate(desc);
    if (vr.code != CORE_OK) return vr;

    vr = core_space_compute_span_from_window(desc->author_window_w,
                                             desc->author_window_h,
                                             &span_x,
                                             &span_y);
    if (vr.code != CORE_OK) return vr;

    {
        float unit_x = core_space_import_pos_to_unit(in->pos_x_raw, span_x);
        float unit_y = core_space_import_pos_to_unit(in->pos_y_raw, span_y);
        out->x = core_space_unit_to_world_x(desc, unit_x);
        out->y = core_space_unit_to_world_y(desc, unit_y);
        out->rotation_deg = in->rotation_deg;
        out->scale = core_space_fit_scale(in->scale,
                                          desc->desired_fit,
                                          in->asset_max_dim,
                                          desc->grid_w,
                                          desc->grid_h,
                                          desc->cell_size);
    }

    return core_result_ok();
}
