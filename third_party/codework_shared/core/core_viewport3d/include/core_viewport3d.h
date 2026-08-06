#ifndef CORE_VIEWPORT3D_H
#define CORE_VIEWPORT3D_H

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CoreViewport3DVec3d {
    double x;
    double y;
    double z;
} CoreViewport3DVec3d;

typedef struct CoreViewport3DOrientation {
    double azimuth_rad;
    double elevation_rad;
} CoreViewport3DOrientation;

typedef struct CoreViewport3DBasis {
    CoreViewport3DVec3d right;
    CoreViewport3DVec3d screen_down;
    CoreViewport3DVec3d forward;
} CoreViewport3DBasis;

typedef struct CoreViewport3DState {
    CoreViewport3DVec3d target;
    CoreViewport3DOrientation orientation;
    double scale_px_per_world_unit;
    double min_scale_px_per_world_unit;
    double max_scale_px_per_world_unit;
} CoreViewport3DState;

typedef enum CoreViewport3DCommandKind {
    CORE_VIEWPORT3D_COMMAND_NONE = 0,
    CORE_VIEWPORT3D_COMMAND_ORBIT,
    CORE_VIEWPORT3D_COMMAND_PAN,
    CORE_VIEWPORT3D_COMMAND_ZOOM,
    CORE_VIEWPORT3D_COMMAND_FRAME,
    CORE_VIEWPORT3D_COMMAND_RESET,
    CORE_VIEWPORT3D_COMMAND_RESIZE
} CoreViewport3DCommandKind;

typedef struct CoreViewport3DCommand {
    CoreViewport3DCommandKind kind;
    union {
        struct {
            double azimuth_delta_rad;
            double elevation_delta_rad;
        } orbit;
        struct {
            double screen_dx;
            double screen_dy;
        } pan;
        struct {
            double factor;
            double anchor_offset_x;
            double anchor_offset_y;
        } zoom;
        struct {
            CoreViewport3DVec3d target;
            double scale_px_per_world_unit;
        } frame;
        CoreViewport3DState reset_state;
    } value;
} CoreViewport3DCommand;

typedef struct CoreViewport3DFitRequest {
    double viewport_width_px;
    double viewport_height_px;
    double projected_span_right_world;
    double projected_span_down_world;
    double fill_fraction;
    double min_scale_px_per_world_unit;
    double max_scale_px_per_world_unit;
} CoreViewport3DFitRequest;

CoreResult core_viewport3d_state_init(CoreViewport3DState *out_state,
                                      CoreViewport3DVec3d target,
                                      CoreViewport3DOrientation orientation,
                                      double scale_px_per_world_unit,
                                      double min_scale_px_per_world_unit,
                                      double max_scale_px_per_world_unit);
CoreResult core_viewport3d_state_validate(const CoreViewport3DState *state);
CoreResult core_viewport3d_build_basis(const CoreViewport3DOrientation *orientation,
                                       CoreViewport3DBasis *out_basis);
CoreResult core_viewport3d_apply(const CoreViewport3DState *state,
                                 const CoreViewport3DCommand *command,
                                 CoreViewport3DState *out_state);
CoreResult core_viewport3d_resolve_fit_scale(const CoreViewport3DFitRequest *request,
                                             double *out_scale_px_per_world_unit);

#ifdef __cplusplus
}
#endif

#endif
