#ifndef CORE_VIEWPORT2D_H
#define CORE_VIEWPORT2D_H

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CoreViewport2D {
    float pan_x;
    float pan_y;
    float zoom;
    float min_zoom;
    float max_zoom;
    float rotation_rad;
} CoreViewport2D;

CoreResult core_viewport2d_init(CoreViewport2D *viewport);
CoreResult core_viewport2d_validate(const CoreViewport2D *viewport);
float core_viewport2d_clamp_zoom(const CoreViewport2D *viewport, float zoom);
CoreResult core_viewport2d_pan_by(CoreViewport2D *viewport, float delta_x, float delta_y);
CoreResult core_viewport2d_screen_to_content(const CoreViewport2D *viewport,
                                             float screen_x,
                                             float screen_y,
                                             float *out_content_x,
                                             float *out_content_y);
CoreResult core_viewport2d_content_to_screen(const CoreViewport2D *viewport,
                                             float content_x,
                                             float content_y,
                                             float *out_screen_x,
                                             float *out_screen_y);
CoreResult core_viewport2d_zoom_at_screen_anchor(CoreViewport2D *viewport,
                                                 float screen_x,
                                                 float screen_y,
                                                 float zoom_factor);
CoreResult core_viewport2d_set_rotation_at_screen_anchor(CoreViewport2D *viewport,
                                                         float screen_x,
                                                         float screen_y,
                                                         float rotation_rad);
CoreResult core_viewport2d_reset_to_fit(CoreViewport2D *viewport,
                                        float view_width,
                                        float view_height,
                                        float content_width,
                                        float content_height);

#ifdef __cplusplus
}
#endif

#endif
