#ifndef MAPFORGE_CAMERA_VIEWPORT_BRIDGE_H
#define MAPFORGE_CAMERA_VIEWPORT_BRIDGE_H

#include "camera/camera.h"
#include "core_viewport2d.h"

enum {
    CAMERA_ZOOM_MIN_LEVEL = 10,
    CAMERA_ZOOM_MAX_LEVEL = 18
};

CoreResult camera_viewport_bridge_world_to_screen(const Camera *camera,
                                                  float world_x,
                                                  float world_y,
                                                  int screen_w,
                                                  int screen_h,
                                                  float *out_screen_x,
                                                  float *out_screen_y);
CoreResult camera_viewport_bridge_screen_to_world(const Camera *camera,
                                                  float screen_x,
                                                  float screen_y,
                                                  int screen_w,
                                                  int screen_h,
                                                  float *out_world_x,
                                                  float *out_world_y);
CoreResult camera_viewport_bridge_zoom_target_at_anchor(Camera *camera,
                                                        float screen_x,
                                                        float screen_y,
                                                        int screen_w,
                                                        int screen_h,
                                                        float next_zoom_level);
CoreResult camera_viewport_bridge_pan_target_by_screen_delta(Camera *camera,
                                                             float delta_x,
                                                             float delta_y,
                                                             int screen_w,
                                                             int screen_h);
CoreResult camera_viewport_bridge_rotate_target_at_anchor(Camera *camera,
                                                          float screen_x,
                                                          float screen_y,
                                                          int screen_w,
                                                          int screen_h,
                                                          float next_heading_rad);

#endif
