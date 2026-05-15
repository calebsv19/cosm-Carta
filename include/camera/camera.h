#ifndef MAPFORGE_CAMERA_CAMERA_H
#define MAPFORGE_CAMERA_CAMERA_H

#include "core/input.h"

// Stores camera position and zoom for the map view.
typedef struct Camera {
    float x;
    float y;
    float x_target;
    float y_target;
    float zoom;
    float zoom_target;
    float heading_rad;
    float heading_target_rad;
} Camera;

// Initializes the camera to a default position.
void camera_init(Camera *camera);

// Updates the camera state based on input and elapsed time.
void camera_handle_input(Camera *camera, const InputState *input, int screen_w, int screen_h, float dt, bool allow_mouse_pan);

// Advances camera simulation logic.
void camera_update(Camera *camera, float dt);

// Updates the desired heading while keeping the stored angle normalized.
void camera_set_heading_target(Camera *camera, float heading_rad);

// Returns pixels per meter for the current camera zoom.
float camera_pixels_per_meter(const Camera *camera);

// Converts a world-space point to screen-space pixels.
void camera_world_to_screen(const Camera *camera, float world_x, float world_y, int screen_w, int screen_h, float *out_x, float *out_y);

// Converts a screen-space point to world-space meters.
void camera_screen_to_world(const Camera *camera, float screen_x, float screen_y, int screen_w, int screen_h, float *out_x, float *out_y);

// Computes the enclosing world-space AABB for the current screen corners.
bool camera_visible_world_aabb(const Camera *camera,
                               int screen_w,
                               int screen_h,
                               float *out_min_x,
                               float *out_min_y,
                               float *out_max_x,
                               float *out_max_y);

#endif
