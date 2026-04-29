#include "camera_viewport_bridge.h"

#include "map/mercator.h"

#include <math.h>

static float camera_viewport_bridge_pixels_per_meter_for_zoom(float zoom_level) {
    double world_size = mercator_world_size_meters();
    double pixels = 256.0 * pow(2.0, (double)zoom_level);
    return (float)(pixels / world_size);
}

static CoreResult camera_viewport_bridge_viewport_from_camera(const Camera *camera,
                                                              int screen_w,
                                                              int screen_h,
                                                              float zoom_level,
                                                              float center_x,
                                                              float center_y,
                                                              CoreViewport2D *out_viewport) {
    CoreResult result;
    float ppm = 0.0f;
    if (!camera || !out_viewport || screen_w <= 0 || screen_h <= 0) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "camera, viewport, and screen bounds are required" };
    }

    result = core_viewport2d_init(out_viewport);
    if (result.code != CORE_OK) {
        return result;
    }

    ppm = camera_viewport_bridge_pixels_per_meter_for_zoom(zoom_level);
    out_viewport->zoom = ppm;
    out_viewport->min_zoom = camera_viewport_bridge_pixels_per_meter_for_zoom((float)CAMERA_ZOOM_MIN_LEVEL);
    out_viewport->max_zoom = camera_viewport_bridge_pixels_per_meter_for_zoom((float)CAMERA_ZOOM_MAX_LEVEL);
    out_viewport->pan_x = ((float)screen_w * 0.5f) - (center_x * out_viewport->zoom);
    out_viewport->pan_y = ((float)screen_h * 0.5f) + (center_y * out_viewport->zoom);
    return core_viewport2d_validate(out_viewport);
}

static CoreResult camera_viewport_bridge_target_center_from_viewport(const CoreViewport2D *viewport,
                                                                     int screen_w,
                                                                     int screen_h,
                                                                     float *out_center_x,
                                                                     float *out_center_y) {
    CoreResult result;
    float content_x = 0.0f;
    float content_y = 0.0f;
    if (!viewport || !out_center_x || !out_center_y || screen_w <= 0 || screen_h <= 0) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "viewport, outputs, and screen bounds are required" };
    }

    result = core_viewport2d_screen_to_content(viewport,
                                               (float)screen_w * 0.5f,
                                               (float)screen_h * 0.5f,
                                               &content_x,
                                               &content_y);
    if (result.code != CORE_OK) {
        return result;
    }

    *out_center_x = content_x;
    *out_center_y = -content_y;
    return core_result_ok();
}

CoreResult camera_viewport_bridge_world_to_screen(const Camera *camera,
                                                  float world_x,
                                                  float world_y,
                                                  int screen_w,
                                                  int screen_h,
                                                  float *out_screen_x,
                                                  float *out_screen_y) {
    CoreViewport2D viewport;
    CoreResult result = camera_viewport_bridge_viewport_from_camera(camera,
                                                                    screen_w,
                                                                    screen_h,
                                                                    camera ? camera->zoom : 0.0f,
                                                                    camera ? camera->x : 0.0f,
                                                                    camera ? camera->y : 0.0f,
                                                                    &viewport);
    if (result.code != CORE_OK) {
        return result;
    }

    return core_viewport2d_content_to_screen(&viewport,
                                             world_x,
                                             -world_y,
                                             out_screen_x,
                                             out_screen_y);
}

CoreResult camera_viewport_bridge_screen_to_world(const Camera *camera,
                                                  float screen_x,
                                                  float screen_y,
                                                  int screen_w,
                                                  int screen_h,
                                                  float *out_world_x,
                                                  float *out_world_y) {
    CoreViewport2D viewport;
    CoreResult result = camera_viewport_bridge_viewport_from_camera(camera,
                                                                    screen_w,
                                                                    screen_h,
                                                                    camera ? camera->zoom : 0.0f,
                                                                    camera ? camera->x : 0.0f,
                                                                    camera ? camera->y : 0.0f,
                                                                    &viewport);
    float content_x = 0.0f;
    float content_y = 0.0f;
    if (result.code != CORE_OK) {
        return result;
    }

    result = core_viewport2d_screen_to_content(&viewport, screen_x, screen_y, &content_x, &content_y);
    if (result.code != CORE_OK) {
        return result;
    }

    if (out_world_x) {
        *out_world_x = content_x;
    }
    if (out_world_y) {
        *out_world_y = -content_y;
    }
    return core_result_ok();
}

CoreResult camera_viewport_bridge_zoom_target_at_anchor(Camera *camera,
                                                        float screen_x,
                                                        float screen_y,
                                                        int screen_w,
                                                        int screen_h,
                                                        float next_zoom_level) {
    CoreViewport2D viewport;
    CoreResult result;
    float current_ppm = 0.0f;
    float next_ppm = 0.0f;
    float zoom_factor = 1.0f;
    if (!camera) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "camera is required" };
    }

    result = camera_viewport_bridge_viewport_from_camera(camera,
                                                         screen_w,
                                                         screen_h,
                                                         camera->zoom,
                                                         camera->x,
                                                         camera->y,
                                                         &viewport);
    if (result.code != CORE_OK) {
        return result;
    }

    current_ppm = viewport.zoom;
    next_ppm = camera_viewport_bridge_pixels_per_meter_for_zoom(next_zoom_level);
    if (!isfinite(current_ppm) || !isfinite(next_ppm) || current_ppm <= 0.0f || next_ppm <= 0.0f) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "camera zoom conversion failed" };
    }

    zoom_factor = next_ppm / current_ppm;
    result = core_viewport2d_zoom_at_screen_anchor(&viewport, screen_x, screen_y, zoom_factor);
    if (result.code != CORE_OK) {
        return result;
    }

    result = camera_viewport_bridge_target_center_from_viewport(&viewport,
                                                                screen_w,
                                                                screen_h,
                                                                &camera->x_target,
                                                                &camera->y_target);
    if (result.code != CORE_OK) {
        return result;
    }

    camera->zoom_target = next_zoom_level;
    return core_result_ok();
}

CoreResult camera_viewport_bridge_pan_target_by_screen_delta(Camera *camera,
                                                             float delta_x,
                                                             float delta_y,
                                                             int screen_w,
                                                             int screen_h) {
    CoreViewport2D viewport;
    CoreResult result;
    if (!camera) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "camera is required" };
    }

    result = camera_viewport_bridge_viewport_from_camera(camera,
                                                         screen_w,
                                                         screen_h,
                                                         camera->zoom,
                                                         camera->x_target,
                                                         camera->y_target,
                                                         &viewport);
    if (result.code != CORE_OK) {
        return result;
    }

    result = core_viewport2d_pan_by(&viewport, delta_x, delta_y);
    if (result.code != CORE_OK) {
        return result;
    }

    return camera_viewport_bridge_target_center_from_viewport(&viewport,
                                                              screen_w,
                                                              screen_h,
                                                              &camera->x_target,
                                                              &camera->y_target);
}
