#include "camera/camera.h"
#include "camera_viewport_bridge.h"

#include "map/mercator.h"

#include <math.h>

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
    camera->x_target = camera->x;
    camera->y_target = camera->y;
    camera->zoom = 14.0f;
    camera->zoom_target = camera->zoom;
}

void camera_handle_input(Camera *camera, const InputState *input, int screen_w, int screen_h, float dt, bool allow_mouse_pan) {
    (void)dt;
    if (!camera || !input) {
        return;
    }

    if (input->mouse_wheel_y != 0) {
        float zoom = camera->zoom_target + 0.25f * (float)input->mouse_wheel_y;
        float next_zoom = clampf(zoom, (float)CAMERA_ZOOM_MIN_LEVEL, (float)CAMERA_ZOOM_MAX_LEVEL);
        CoreResult zoom_result = camera_viewport_bridge_zoom_target_at_anchor(camera,
                                                                              (float)input->mouse_x,
                                                                              (float)input->mouse_y,
                                                                              screen_w,
                                                                              screen_h,
                                                                              next_zoom);
        if (zoom_result.code != CORE_OK) {
            camera->zoom_target = next_zoom;
        }
    }
    const float pan_speed_pixels = 500.0f;
    float pan_delta_x = 0.0f;
    float pan_delta_y = 0.0f;

    if (input->pan_left) {
        pan_delta_x += pan_speed_pixels * dt;
    }
    if (input->pan_right) {
        pan_delta_x -= pan_speed_pixels * dt;
    }
    if (input->pan_up) {
        pan_delta_y += pan_speed_pixels * dt;
    }
    if (input->pan_down) {
        pan_delta_y -= pan_speed_pixels * dt;
    }

    if (allow_mouse_pan && (input->mouse_buttons & SDL_BUTTON_LMASK)) {
        pan_delta_x += (float)input->mouse_dx;
        pan_delta_y += (float)input->mouse_dy;
    }

    if (pan_delta_x != 0.0f || pan_delta_y != 0.0f) {
        (void)camera_viewport_bridge_pan_target_by_screen_delta(camera,
                                                                pan_delta_x,
                                                                pan_delta_y,
                                                                screen_w,
                                                                screen_h);
    }
}

void camera_update(Camera *camera, float dt) {
    if (!camera) {
        return;
    }

    const float response = 16.0f;
    float alpha = 1.0f - expf(-response * dt);
    camera->zoom += (camera->zoom_target - camera->zoom) * alpha;
    camera->x += (camera->x_target - camera->x) * alpha;
    camera->y += (camera->y_target - camera->y) * alpha;
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
