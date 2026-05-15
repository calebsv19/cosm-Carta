#include "camera/camera.h"
#include "camera_viewport_bridge.h"

#include "map/mercator.h"

#include <math.h>

static float camera_normalize_angle(float angle_rad) {
    const float tau = 6.28318530717958647693f;
    if (!isfinite(angle_rad)) {
        return 0.0f;
    }

    angle_rad = fmodf(angle_rad, tau);
    if (angle_rad <= -3.14159265358979323846f) {
        angle_rad += tau;
    } else if (angle_rad > 3.14159265358979323846f) {
        angle_rad -= tau;
    }
    return angle_rad;
}

static float camera_shortest_angle_delta(float current_angle_rad, float target_angle_rad) {
    return camera_normalize_angle(target_angle_rad - current_angle_rad);
}

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
    camera->heading_rad = 0.0f;
    camera->heading_target_rad = 0.0f;
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
    camera->heading_target_rad = camera_normalize_angle(camera->heading_target_rad);
    camera->heading_rad = camera_normalize_angle(camera->heading_rad +
                                                 camera_shortest_angle_delta(camera->heading_rad, camera->heading_target_rad) * alpha);
}

void camera_set_heading_target(Camera *camera, float heading_rad) {
    if (!camera) {
        return;
    }
    camera->heading_target_rad = camera_normalize_angle(heading_rad);
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
    if (camera_viewport_bridge_world_to_screen(camera, world_x, world_y, screen_w, screen_h, out_x, out_y).code != CORE_OK) {
        *out_x = 0.0f;
        *out_y = 0.0f;
    }
}

void camera_screen_to_world(const Camera *camera, float screen_x, float screen_y, int screen_w, int screen_h, float *out_x, float *out_y) {
    if (!camera || !out_x || !out_y) {
        return;
    }
    if (camera_viewport_bridge_screen_to_world(camera, screen_x, screen_y, screen_w, screen_h, out_x, out_y).code != CORE_OK) {
        *out_x = 0.0f;
        *out_y = 0.0f;
    }
}

bool camera_visible_world_aabb(const Camera *camera,
                               int screen_w,
                               int screen_h,
                               float *out_min_x,
                               float *out_min_y,
                               float *out_max_x,
                               float *out_max_y) {
    if (!camera || screen_w <= 0 || screen_h <= 0 ||
        !out_min_x || !out_min_y || !out_max_x || !out_max_y) {
        return false;
    }

    static const float corner_uvs[4][2] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 1.0f}
    };

    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    for (int i = 0; i < 4; ++i) {
        float world_x = 0.0f;
        float world_y = 0.0f;
        camera_screen_to_world(camera,
                               corner_uvs[i][0] * (float)screen_w,
                               corner_uvs[i][1] * (float)screen_h,
                               screen_w,
                               screen_h,
                               &world_x,
                               &world_y);
        if (i == 0) {
            min_x = max_x = world_x;
            min_y = max_y = world_y;
            continue;
        }
        if (world_x < min_x) {
            min_x = world_x;
        }
        if (world_x > max_x) {
            max_x = world_x;
        }
        if (world_y < min_y) {
            min_y = world_y;
        }
        if (world_y > max_y) {
            max_y = world_y;
        }
    }

    *out_min_x = min_x;
    *out_min_y = min_y;
    *out_max_x = max_x;
    *out_max_y = max_y;
    return true;
}
