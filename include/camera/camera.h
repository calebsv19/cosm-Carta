#ifndef MAPFORGE_CAMERA_CAMERA_H
#define MAPFORGE_CAMERA_CAMERA_H

#include "core/input.h"

// Stores camera position and zoom for the map view.
typedef struct Camera {
    float x;
    float y;
    float zoom;
} Camera;

// Initializes the camera to a default position.
void camera_init(Camera *camera);

// Updates the camera state based on input and elapsed time.
void camera_handle_input(Camera *camera, const InputState *input, int screen_w, int screen_h, float dt);

// Advances camera simulation logic.
void camera_update(Camera *camera, float dt);

// Returns pixels per meter for the current camera zoom.
float camera_pixels_per_meter(const Camera *camera);

// Converts a world-space point to screen-space pixels.
void camera_world_to_screen(const Camera *camera, float world_x, float world_y, int screen_w, int screen_h, float *out_x, float *out_y);

// Converts a screen-space point to world-space meters.
void camera_screen_to_world(const Camera *camera, float screen_x, float screen_y, int screen_w, int screen_h, float *out_x, float *out_y);

#endif
