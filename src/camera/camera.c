#include "camera/camera.h"

#include "map/mercator.h"

#include <math.h>
#include <SDL.h>

static float clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

void camera_init(Camera *camera) {
    if (!camera) {
        return;
    }

    MercatorMeters center = mercator_from_latlon((LatLon){47.664005, -122.303765});
    camera->x = (float)center.x;
    camera->y = (float)center.y;
    camera->zoom = 14.0f;
}

void camera_handle_input(Camera *camera, const InputState *input, int screen_w, int screen_h, float dt) {
    (void)dt;
    if (!camera || !input) {
        return;
    }

    if (input->mouse_wheel_y != 0) {
        float world_x = 0.0f;
        float world_y = 0.0f;
        camera_screen_to_world(camera, (float)input->mouse_x, (float)input->mouse_y, screen_w, screen_h, &world_x, &world_y);

        float zoom = camera->zoom + 0.25f * (float)input->mouse_wheel_y;
        camera->zoom = clampf(zoom, 10.0f, 18.0f);

        float ppm = camera_pixels_per_meter(camera);
        if (ppm > 0.0f) {
            float dx = (float)input->mouse_x - (float)screen_w * 0.5f;
            float dy = (float)input->mouse_y - (float)screen_h * 0.5f;
            camera->x = world_x - dx / ppm;
            camera->y = world_y + dy / ppm;
        }
    }

    float ppm = camera_pixels_per_meter(camera);
    if (ppm <= 0.0f) {
        return;
    }

    const float pan_speed_pixels = 500.0f;
    float pan_step = (pan_speed_pixels / ppm) * dt;

    if (input->pan_left) {
        camera->x -= pan_step;
    }
    if (input->pan_right) {
        camera->x += pan_step;
    }
    if (input->pan_up) {
        camera->y += pan_step;
    }
    if (input->pan_down) {
        camera->y -= pan_step;
    }

    if (input->mouse_buttons & SDL_BUTTON_LMASK) {
        camera->x -= (float)input->mouse_dx / ppm;
        camera->y += (float)input->mouse_dy / ppm;
    }
}

void camera_update(Camera *camera, float dt) {
    (void)camera;
    (void)dt;
}

float camera_pixels_per_meter(const Camera *camera) {
    if (!camera) {
        return 1.0f;
    }

    double world_size = mercator_world_size_meters();
    double pixels = 256.0 * pow(2.0, camera->zoom);
    return (float)(pixels / world_size);
}

void camera_world_to_screen(const Camera *camera, float world_x, float world_y, int screen_w, int screen_h, float *out_x, float *out_y) {
    if (!camera || !out_x || !out_y) {
        return;
    }

    float ppm = camera_pixels_per_meter(camera);
    float dx = (world_x - camera->x) * ppm;
    float dy = (camera->y - world_y) * ppm;

    *out_x = (float)screen_w * 0.5f + dx;
    *out_y = (float)screen_h * 0.5f + dy;
}

void camera_screen_to_world(const Camera *camera, float screen_x, float screen_y, int screen_w, int screen_h, float *out_x, float *out_y) {
    if (!camera || !out_x || !out_y) {
        return;
    }

    float ppm = camera_pixels_per_meter(camera);
    if (ppm <= 0.0f) {
        return;
    }

    float dx = screen_x - (float)screen_w * 0.5f;
    float dy = screen_y - (float)screen_h * 0.5f;

    *out_x = camera->x + dx / ppm;
    *out_y = camera->y - dy / ppm;
}
