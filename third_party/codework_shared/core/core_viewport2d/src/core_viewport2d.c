#include "core_viewport2d.h"

#include <math.h>

static int core_viewport2d_isfinite2(float a, float b) {
    return isfinite(a) && isfinite(b);
}

static float core_viewport2d_normalize_rotation(float rotation_rad) {
    const float tau = 6.28318530717958647693f;
    if (!isfinite(rotation_rad)) {
        return 0.0f;
    }

    rotation_rad = fmodf(rotation_rad, tau);
    if (rotation_rad <= (float)-M_PI) {
        rotation_rad += tau;
    } else if (rotation_rad > (float)M_PI) {
        rotation_rad -= tau;
    }
    return rotation_rad;
}

static void core_viewport2d_rotate_scaled_content(float content_x,
                                                  float content_y,
                                                  float zoom,
                                                  float rotation_rad,
                                                  float *out_screen_dx,
                                                  float *out_screen_dy) {
    float scaled_x = content_x * zoom;
    float scaled_y = content_y * zoom;
    float c = cosf(rotation_rad);
    float s = sinf(rotation_rad);

    if (out_screen_dx) {
        *out_screen_dx = (scaled_x * c) - (scaled_y * s);
    }
    if (out_screen_dy) {
        *out_screen_dy = (scaled_x * s) + (scaled_y * c);
    }
}

CoreResult core_viewport2d_init(CoreViewport2D *viewport) {
    if (!viewport) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "viewport is required" };
    }
    viewport->pan_x = 0.0f;
    viewport->pan_y = 0.0f;
    viewport->zoom = 1.0f;
    viewport->min_zoom = 0.0001f;
    viewport->max_zoom = 1024.0f;
    viewport->rotation_rad = 0.0f;
    return core_result_ok();
}

CoreResult core_viewport2d_validate(const CoreViewport2D *viewport) {
    if (!viewport) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "viewport is required" };
    }
    if (!core_viewport2d_isfinite2(viewport->pan_x, viewport->pan_y) ||
        !isfinite(viewport->zoom) ||
        !isfinite(viewport->min_zoom) ||
        !isfinite(viewport->max_zoom) ||
        !isfinite(viewport->rotation_rad)) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "viewport values must be finite" };
    }
    if (viewport->min_zoom <= 0.0f || viewport->max_zoom <= 0.0f) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "zoom bounds must be positive" };
    }
    if (viewport->min_zoom > viewport->max_zoom) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "min_zoom must be <= max_zoom" };
    }
    if (viewport->zoom < viewport->min_zoom || viewport->zoom > viewport->max_zoom) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "zoom must stay within bounds" };
    }
    return core_result_ok();
}

float core_viewport2d_clamp_zoom(const CoreViewport2D *viewport, float zoom) {
    float min_zoom = 0.0001f;
    float max_zoom = 1024.0f;
    if (!isfinite(zoom)) {
        return 1.0f;
    }
    if (viewport) {
        if (isfinite(viewport->min_zoom) && viewport->min_zoom > 0.0f) {
            min_zoom = viewport->min_zoom;
        }
        if (isfinite(viewport->max_zoom) && viewport->max_zoom >= min_zoom) {
            max_zoom = viewport->max_zoom;
        }
    }
    if (zoom < min_zoom) {
        return min_zoom;
    }
    if (zoom > max_zoom) {
        return max_zoom;
    }
    return zoom;
}

CoreResult core_viewport2d_pan_by(CoreViewport2D *viewport, float delta_x, float delta_y) {
    CoreResult valid;
    if (!viewport) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "viewport is required" };
    }
    if (!core_viewport2d_isfinite2(delta_x, delta_y)) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "pan delta must be finite" };
    }
    valid = core_viewport2d_validate(viewport);
    if (valid.code != CORE_OK) {
        return valid;
    }
    viewport->pan_x += delta_x;
    viewport->pan_y += delta_y;
    return core_result_ok();
}

CoreResult core_viewport2d_screen_to_content(const CoreViewport2D *viewport,
                                             float screen_x,
                                             float screen_y,
                                             float *out_content_x,
                                             float *out_content_y) {
    CoreResult valid;
    if (!viewport || !out_content_x || !out_content_y) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "viewport and outputs are required" };
    }
    if (!core_viewport2d_isfinite2(screen_x, screen_y)) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "screen point must be finite" };
    }
    valid = core_viewport2d_validate(viewport);
    if (valid.code != CORE_OK) {
        return valid;
    }
    {
        float dx = screen_x - viewport->pan_x;
        float dy = screen_y - viewport->pan_y;
        float rotation_rad = core_viewport2d_normalize_rotation(viewport->rotation_rad);
        float c = cosf(rotation_rad);
        float s = sinf(rotation_rad);
        *out_content_x = ((dx * c) + (dy * s)) / viewport->zoom;
        *out_content_y = ((dy * c) - (dx * s)) / viewport->zoom;
    }
    return core_result_ok();
}

CoreResult core_viewport2d_content_to_screen(const CoreViewport2D *viewport,
                                             float content_x,
                                             float content_y,
                                             float *out_screen_x,
                                             float *out_screen_y) {
    CoreResult valid;
    if (!viewport || !out_screen_x || !out_screen_y) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "viewport and outputs are required" };
    }
    if (!core_viewport2d_isfinite2(content_x, content_y)) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "content point must be finite" };
    }
    valid = core_viewport2d_validate(viewport);
    if (valid.code != CORE_OK) {
        return valid;
    }
    core_viewport2d_rotate_scaled_content(content_x,
                                          content_y,
                                          viewport->zoom,
                                          core_viewport2d_normalize_rotation(viewport->rotation_rad),
                                          out_screen_x,
                                          out_screen_y);
    *out_screen_x += viewport->pan_x;
    *out_screen_y += viewport->pan_y;
    return core_result_ok();
}

CoreResult core_viewport2d_zoom_at_screen_anchor(CoreViewport2D *viewport,
                                                 float screen_x,
                                                 float screen_y,
                                                 float zoom_factor) {
    CoreResult valid;
    float content_x = 0.0f;
    float content_y = 0.0f;
    float next_zoom;
    if (!viewport) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "viewport is required" };
    }
    if (!core_viewport2d_isfinite2(screen_x, screen_y) || !isfinite(zoom_factor) || zoom_factor <= 0.0f) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "zoom anchor request must be finite and positive" };
    }
    valid = core_viewport2d_validate(viewport);
    if (valid.code != CORE_OK) {
        return valid;
    }
    valid = core_viewport2d_screen_to_content(viewport, screen_x, screen_y, &content_x, &content_y);
    if (valid.code != CORE_OK) {
        return valid;
    }
    next_zoom = core_viewport2d_clamp_zoom(viewport, viewport->zoom * zoom_factor);
    viewport->zoom = next_zoom;
    {
        float rotated_x = 0.0f;
        float rotated_y = 0.0f;
        core_viewport2d_rotate_scaled_content(content_x,
                                              content_y,
                                              viewport->zoom,
                                              core_viewport2d_normalize_rotation(viewport->rotation_rad),
                                              &rotated_x,
                                              &rotated_y);
        viewport->pan_x = screen_x - rotated_x;
        viewport->pan_y = screen_y - rotated_y;
    }
    return core_result_ok();
}

CoreResult core_viewport2d_set_rotation_at_screen_anchor(CoreViewport2D *viewport,
                                                         float screen_x,
                                                         float screen_y,
                                                         float rotation_rad) {
    CoreResult valid;
    float content_x = 0.0f;
    float content_y = 0.0f;
    float rotated_x = 0.0f;
    float rotated_y = 0.0f;

    if (!viewport) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "viewport is required" };
    }
    if (!core_viewport2d_isfinite2(screen_x, screen_y) || !isfinite(rotation_rad)) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "rotation anchor request must be finite" };
    }

    valid = core_viewport2d_validate(viewport);
    if (valid.code != CORE_OK) {
        return valid;
    }
    valid = core_viewport2d_screen_to_content(viewport, screen_x, screen_y, &content_x, &content_y);
    if (valid.code != CORE_OK) {
        return valid;
    }

    viewport->rotation_rad = core_viewport2d_normalize_rotation(rotation_rad);
    core_viewport2d_rotate_scaled_content(content_x,
                                          content_y,
                                          viewport->zoom,
                                          viewport->rotation_rad,
                                          &rotated_x,
                                          &rotated_y);
    viewport->pan_x = screen_x - rotated_x;
    viewport->pan_y = screen_y - rotated_y;
    return core_result_ok();
}

CoreResult core_viewport2d_reset_to_fit(CoreViewport2D *viewport,
                                        float view_width,
                                        float view_height,
                                        float content_width,
                                        float content_height) {
    CoreResult valid;
    float scale_x;
    float scale_y;
    float fit_zoom;
    if (!viewport) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "viewport is required" };
    }
    if (!isfinite(view_width) || !isfinite(view_height) || !isfinite(content_width) || !isfinite(content_height) ||
        view_width <= 0.0f || view_height <= 0.0f || content_width <= 0.0f || content_height <= 0.0f) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "fit dimensions must be finite and positive" };
    }
    valid = core_viewport2d_validate(viewport);
    if (valid.code != CORE_OK) {
        return valid;
    }
    scale_x = view_width / content_width;
    scale_y = view_height / content_height;
    fit_zoom = scale_x < scale_y ? scale_x : scale_y;
    viewport->zoom = core_viewport2d_clamp_zoom(viewport, fit_zoom);
    viewport->rotation_rad = 0.0f;
    viewport->pan_x = (view_width - (content_width * viewport->zoom)) * 0.5f;
    viewport->pan_y = (view_height - (content_height * viewport->zoom)) * 0.5f;
    return core_result_ok();
}
