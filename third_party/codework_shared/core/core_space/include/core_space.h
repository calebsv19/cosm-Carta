#ifndef CORE_SPACE_H
#define CORE_SPACE_H

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CoreSpaceDesc {
    int grid_w;
    int grid_h;
    float origin_x;
    float origin_y;
    float cell_size;

    int author_window_w;
    int author_window_h;
    float desired_fit;
} CoreSpaceDesc;

typedef struct CoreSpaceImport {
    float pos_x_raw;
    float pos_y_raw;
    float rotation_deg;
    float scale;
    float asset_max_dim;
} CoreSpaceImport;

typedef struct CoreSpaceWorldTransform {
    float x;
    float y;
    float rotation_deg;
    float scale;
} CoreSpaceWorldTransform;

CoreResult core_space_desc_validate(const CoreSpaceDesc *desc);
CoreResult core_space_desc_default_from_grid(int grid_w,
                                             int grid_h,
                                             float origin_x,
                                             float origin_y,
                                             float cell_size,
                                             CoreSpaceDesc *out_desc);

CoreResult core_space_compute_span_from_window(int window_w,
                                               int window_h,
                                               float *out_span_x,
                                               float *out_span_y);

float core_space_import_pos_to_unit(float pos_raw, float span_cfg);

float core_space_unit_to_world_x(const CoreSpaceDesc *desc, float unit_x);
float core_space_unit_to_world_y(const CoreSpaceDesc *desc, float unit_y);

float core_space_world_to_unit_x(const CoreSpaceDesc *desc, float world_x);
float core_space_world_to_unit_y(const CoreSpaceDesc *desc, float world_y);

float core_space_fit_scale(float import_scale,
                           float desired_fit,
                           float asset_max_dim,
                           int grid_w,
                           int grid_h,
                           float cell_size);

CoreResult core_space_import_to_world(const CoreSpaceDesc *desc,
                                      const CoreSpaceImport *in,
                                      CoreSpaceWorldTransform *out);

#ifdef __cplusplus
}
#endif

#endif
