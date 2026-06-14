#include "core_viewport2d.h"

#include <math.h>
#include <stdio.h>

static const float CORE_VIEWPORT2D_PI = 3.14159265358979323846f;

static int nearly_equal(float a, float b, float eps) {
    return fabsf(a - b) <= eps;
}

static int viewport_matches(const CoreViewport2D *viewport,
                            float pan_x,
                            float pan_y,
                            float zoom,
                            float min_zoom,
                            float max_zoom,
                            float rotation_rad,
                            float eps) {
    return nearly_equal(viewport->pan_x, pan_x, eps) &&
           nearly_equal(viewport->pan_y, pan_y, eps) &&
           nearly_equal(viewport->zoom, zoom, eps) &&
           nearly_equal(viewport->min_zoom, min_zoom, eps) &&
           nearly_equal(viewport->max_zoom, max_zoom, eps) &&
           nearly_equal(viewport->rotation_rad, rotation_rad, eps);
}

int main(void) {
    CoreViewport2D viewport;
    CoreViewport2D invalid_bounds;
    CoreViewport2D invalid_zoom;
    CoreViewport2D invalid_rotation;
    CoreViewport2D before;
    CoreResult r;
    float x = 0.0f;
    float y = 0.0f;
    float sentinel_x = 111.0f;
    float sentinel_y = -222.0f;

    r = core_viewport2d_init(NULL);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;

    r = core_viewport2d_init(&viewport);
    if (r.code != CORE_OK) return 1;
    if (!viewport_matches(&viewport, 0.0f, 0.0f, 1.0f, 0.0001f, 1024.0f, 0.0f, 1e-6f)) return 1;

    r = core_viewport2d_validate(NULL);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    r = core_viewport2d_validate(&viewport);
    if (r.code != CORE_OK) return 1;

    invalid_bounds = viewport;
    invalid_bounds.min_zoom = 4.0f;
    invalid_bounds.max_zoom = 2.0f;
    r = core_viewport2d_validate(&invalid_bounds);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;

    invalid_zoom = viewport;
    invalid_zoom.zoom = 0.0f;
    r = core_viewport2d_validate(&invalid_zoom);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;

    invalid_rotation = viewport;
    invalid_rotation.rotation_rad = NAN;
    r = core_viewport2d_validate(&invalid_rotation);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;

    if (!nearly_equal(core_viewport2d_clamp_zoom(NULL, 0.00001f), 0.0001f, 1e-8f)) return 1;
    if (!nearly_equal(core_viewport2d_clamp_zoom(NULL, 5000.0f), 1024.0f, 1e-6f)) return 1;
    if (!nearly_equal(core_viewport2d_clamp_zoom(NULL, NAN), 1.0f, 1e-6f)) return 1;
    if (!nearly_equal(core_viewport2d_clamp_zoom(&invalid_bounds, 0.00001f), 0.0001f, 1e-8f)) return 1;
    if (!nearly_equal(core_viewport2d_clamp_zoom(&invalid_bounds, 5000.0f), 1024.0f, 1e-6f)) return 1;

    r = core_viewport2d_pan_by(NULL, 1.0f, 2.0f);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    r = core_viewport2d_pan_by(&viewport, NAN, 2.0f);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    before = viewport;
    r = core_viewport2d_pan_by(&invalid_bounds, 1.0f, 2.0f);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    if (!viewport_matches(&invalid_bounds, before.pan_x, before.pan_y, 1.0f, 4.0f, 2.0f, 0.0f, 1e-6f)) {
        /* invalid_bounds should remain unchanged apart from its intentionally invalid fields */
        if (!nearly_equal(invalid_bounds.pan_x, 0.0f, 1e-6f) ||
            !nearly_equal(invalid_bounds.pan_y, 0.0f, 1e-6f) ||
            !nearly_equal(invalid_bounds.zoom, 1.0f, 1e-6f) ||
            !nearly_equal(invalid_bounds.min_zoom, 4.0f, 1e-6f) ||
            !nearly_equal(invalid_bounds.max_zoom, 2.0f, 1e-6f) ||
            !nearly_equal(invalid_bounds.rotation_rad, 0.0f, 1e-6f)) return 1;
    }

    r = core_viewport2d_pan_by(&viewport, 12.0f, -8.0f);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(viewport.pan_x, 12.0f, 1e-6f)) return 1;
    if (!nearly_equal(viewport.pan_y, -8.0f, 1e-6f)) return 1;

    x = sentinel_x;
    y = sentinel_y;
    r = core_viewport2d_screen_to_content(NULL, 52.0f, 32.0f, &x, &y);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    if (!nearly_equal(x, sentinel_x, 1e-6f) || !nearly_equal(y, sentinel_y, 1e-6f)) return 1;

    r = core_viewport2d_screen_to_content(&viewport, 52.0f, 32.0f, &x, &y);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(x, 40.0f, 1e-6f)) return 1;
    if (!nearly_equal(y, 40.0f, 1e-6f)) return 1;

    x = sentinel_x;
    y = sentinel_y;
    r = core_viewport2d_screen_to_content(&viewport, NAN, 32.0f, &x, &y);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    if (!nearly_equal(x, sentinel_x, 1e-6f) || !nearly_equal(y, sentinel_y, 1e-6f)) return 1;

    x = sentinel_x;
    y = sentinel_y;
    r = core_viewport2d_screen_to_content(&invalid_bounds, 52.0f, 32.0f, &x, &y);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    if (!nearly_equal(x, sentinel_x, 1e-6f) || !nearly_equal(y, sentinel_y, 1e-6f)) return 1;

    x = sentinel_x;
    y = sentinel_y;
    r = core_viewport2d_content_to_screen(NULL, 40.0f, 40.0f, &x, &y);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    if (!nearly_equal(x, sentinel_x, 1e-6f) || !nearly_equal(y, sentinel_y, 1e-6f)) return 1;

    r = core_viewport2d_content_to_screen(&viewport, 40.0f, 40.0f, &x, &y);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(x, 52.0f, 1e-6f)) return 1;
    if (!nearly_equal(y, 32.0f, 1e-6f)) return 1;

    x = sentinel_x;
    y = sentinel_y;
    r = core_viewport2d_content_to_screen(&viewport, 40.0f, INFINITY, &x, &y);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    if (!nearly_equal(x, sentinel_x, 1e-6f) || !nearly_equal(y, sentinel_y, 1e-6f)) return 1;

    viewport.pan_x = 100.0f;
    viewport.pan_y = 50.0f;
    viewport.zoom = 2.0f;
    viewport.rotation_rad = CORE_VIEWPORT2D_PI * 0.5f;
    r = core_viewport2d_content_to_screen(&viewport, 10.0f, 5.0f, &x, &y);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(x, 90.0f, 1e-5f)) return 1;
    if (!nearly_equal(y, 70.0f, 1e-5f)) return 1;
    r = core_viewport2d_screen_to_content(&viewport, x, y, &x, &y);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(x, 10.0f, 1e-5f)) return 1;
    if (!nearly_equal(y, 5.0f, 1e-5f)) return 1;

    viewport.pan_x = 0.0f;
    viewport.pan_y = 0.0f;
    viewport.zoom = 1.0f;
    viewport.min_zoom = 0.25f;
    viewport.max_zoom = 4.0f;
    viewport.rotation_rad = 5.5f * CORE_VIEWPORT2D_PI;
    r = core_viewport2d_content_to_screen(&viewport, 10.0f, 0.0f, &x, &y);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(x, 0.0f, 1e-4f)) return 1;
    if (!nearly_equal(y, -10.0f, 1e-4f)) return 1;

    viewport.pan_x = 25.0f;
    viewport.pan_y = 10.0f;
    viewport.zoom = 2.0f;
    viewport.rotation_rad = 0.0f;
    r = core_viewport2d_zoom_at_screen_anchor(&viewport, 125.0f, 90.0f, 1.5f);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(viewport.zoom, 3.0f, 1e-6f)) return 1;
    r = core_viewport2d_screen_to_content(&viewport, 125.0f, 90.0f, &x, &y);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(x, 50.0f, 1e-5f)) return 1;
    if (!nearly_equal(y, 40.0f, 1e-5f)) return 1;

    before = viewport;
    r = core_viewport2d_zoom_at_screen_anchor(&viewport, 125.0f, 90.0f, 0.0f);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    if (!viewport_matches(&viewport, before.pan_x, before.pan_y, before.zoom, before.min_zoom, before.max_zoom, before.rotation_rad, 1e-6f)) return 1;

    before = viewport;
    r = core_viewport2d_zoom_at_screen_anchor(&viewport, 125.0f, 90.0f, NAN);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    if (!viewport_matches(&viewport, before.pan_x, before.pan_y, before.zoom, before.min_zoom, before.max_zoom, before.rotation_rad, 1e-6f)) return 1;

    viewport.min_zoom = 0.5f;
    viewport.max_zoom = 2.0f;
    viewport.zoom = 1.5f;
    viewport.pan_x = 10.0f;
    viewport.pan_y = 20.0f;
    viewport.rotation_rad = 0.3f;
    r = core_viewport2d_zoom_at_screen_anchor(&viewport, 80.0f, 90.0f, 100.0f);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(viewport.zoom, 2.0f, 1e-6f)) return 1;

    viewport.pan_x = 12.0f;
    viewport.pan_y = -6.0f;
    viewport.zoom = 3.0f;
    viewport.min_zoom = 0.0001f;
    viewport.max_zoom = 1024.0f;
    viewport.rotation_rad = -0.4f;
    r = core_viewport2d_screen_to_content(&viewport, 144.0f, 96.0f, &x, &y);
    if (r.code != CORE_OK) return 1;
    r = core_viewport2d_set_rotation_at_screen_anchor(&viewport, 144.0f, 96.0f, 5.5f * CORE_VIEWPORT2D_PI);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(viewport.rotation_rad, -0.5f * CORE_VIEWPORT2D_PI, 1e-5f)) return 1;
    {
        float ax = 0.0f;
        float ay = 0.0f;
        r = core_viewport2d_screen_to_content(&viewport, 144.0f, 96.0f, &ax, &ay);
        if (r.code != CORE_OK) return 1;
        if (!nearly_equal(ax, x, 1e-4f)) return 1;
        if (!nearly_equal(ay, y, 1e-4f)) return 1;
    }

    before = viewport;
    r = core_viewport2d_set_rotation_at_screen_anchor(&viewport, 144.0f, 96.0f, NAN);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    if (!viewport_matches(&viewport, before.pan_x, before.pan_y, before.zoom, before.min_zoom, before.max_zoom, before.rotation_rad, 1e-6f)) return 1;

    viewport.min_zoom = 0.0001f;
    viewport.max_zoom = 100.0f;
    viewport.rotation_rad = 0.7f;
    r = core_viewport2d_reset_to_fit(&viewport, 1000.0f, 800.0f, 4000.0f, 1000.0f);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(viewport.zoom, 0.25f, 1e-6f)) return 1;
    if (!nearly_equal(viewport.pan_x, 0.0f, 1e-6f)) return 1;
    if (!nearly_equal(viewport.pan_y, 275.0f, 1e-6f)) return 1;
    if (!nearly_equal(viewport.rotation_rad, 0.0f, 1e-6f)) return 1;

    viewport.min_zoom = 0.5f;
    viewport.max_zoom = 10.0f;
    viewport.zoom = 1.0f;
    viewport.rotation_rad = -1.0f;
    r = core_viewport2d_reset_to_fit(&viewport, 1000.0f, 800.0f, 4000.0f, 1000.0f);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(viewport.zoom, 0.5f, 1e-6f)) return 1;
    if (!nearly_equal(viewport.pan_x, -500.0f, 1e-6f)) return 1;
    if (!nearly_equal(viewport.pan_y, 150.0f, 1e-6f)) return 1;
    if (!nearly_equal(viewport.rotation_rad, 0.0f, 1e-6f)) return 1;

    before = viewport;
    r = core_viewport2d_reset_to_fit(&viewport, NAN, 800.0f, 4000.0f, 1000.0f);
    if (r.code != CORE_ERR_INVALID_ARG) return 1;
    if (!viewport_matches(&viewport, before.pan_x, before.pan_y, before.zoom, before.min_zoom, before.max_zoom, before.rotation_rad, 1e-6f)) return 1;

    printf("core_viewport2d tests passed\n");
    return 0;
}
