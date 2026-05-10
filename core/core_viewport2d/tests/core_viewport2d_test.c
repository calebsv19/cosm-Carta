#include "core_viewport2d.h"

#include <math.h>
#include <stdio.h>

static int nearly_equal(float a, float b, float eps) {
    return fabsf(a - b) <= eps;
}

int main(void) {
    CoreViewport2D viewport;
    CoreResult r;
    float x = 0.0f;
    float y = 0.0f;

    r = core_viewport2d_init(&viewport);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(viewport.zoom, 1.0f, 1e-6f)) return 1;
    if (!nearly_equal(viewport.pan_x, 0.0f, 1e-6f)) return 1;
    if (!nearly_equal(viewport.pan_y, 0.0f, 1e-6f)) return 1;
    if (!nearly_equal(viewport.rotation_rad, 0.0f, 1e-6f)) return 1;

    r = core_viewport2d_validate(&viewport);
    if (r.code != CORE_OK) return 1;

    r = core_viewport2d_pan_by(&viewport, 12.0f, -8.0f);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(viewport.pan_x, 12.0f, 1e-6f)) return 1;
    if (!nearly_equal(viewport.pan_y, -8.0f, 1e-6f)) return 1;

    r = core_viewport2d_screen_to_content(&viewport, 52.0f, 32.0f, &x, &y);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(x, 40.0f, 1e-6f)) return 1;
    if (!nearly_equal(y, 40.0f, 1e-6f)) return 1;

    r = core_viewport2d_content_to_screen(&viewport, 40.0f, 40.0f, &x, &y);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(x, 52.0f, 1e-6f)) return 1;
    if (!nearly_equal(y, 32.0f, 1e-6f)) return 1;

    viewport.pan_x = 100.0f;
    viewport.pan_y = 50.0f;
    viewport.zoom = 2.0f;
    viewport.rotation_rad = (float)(M_PI * 0.5);
    r = core_viewport2d_content_to_screen(&viewport, 10.0f, 5.0f, &x, &y);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(x, 90.0f, 1e-5f)) return 1;
    if (!nearly_equal(y, 70.0f, 1e-5f)) return 1;
    r = core_viewport2d_screen_to_content(&viewport, x, y, &x, &y);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(x, 10.0f, 1e-5f)) return 1;
    if (!nearly_equal(y, 5.0f, 1e-5f)) return 1;

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

    viewport.pan_x = 40.0f;
    viewport.pan_y = 30.0f;
    viewport.zoom = 4.0f;
    viewport.rotation_rad = 0.35f;
    r = core_viewport2d_screen_to_content(&viewport, 110.0f, 75.0f, &x, &y);
    if (r.code != CORE_OK) return 1;
    r = core_viewport2d_zoom_at_screen_anchor(&viewport, 110.0f, 75.0f, 1.25f);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(viewport.zoom, 5.0f, 1e-5f)) return 1;
    {
        float ax = 0.0f;
        float ay = 0.0f;
        r = core_viewport2d_screen_to_content(&viewport, 110.0f, 75.0f, &ax, &ay);
        if (r.code != CORE_OK) return 1;
        if (!nearly_equal(ax, x, 1e-4f)) return 1;
        if (!nearly_equal(ay, y, 1e-4f)) return 1;
    }

    viewport.pan_x = 12.0f;
    viewport.pan_y = -6.0f;
    viewport.zoom = 3.0f;
    viewport.rotation_rad = -0.4f;
    r = core_viewport2d_screen_to_content(&viewport, 144.0f, 96.0f, &x, &y);
    if (r.code != CORE_OK) return 1;
    r = core_viewport2d_set_rotation_at_screen_anchor(&viewport, 144.0f, 96.0f, 1.1f);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(viewport.rotation_rad, 1.1f, 1e-5f)) return 1;
    {
        float ax = 0.0f;
        float ay = 0.0f;
        r = core_viewport2d_screen_to_content(&viewport, 144.0f, 96.0f, &ax, &ay);
        if (r.code != CORE_OK) return 1;
        if (!nearly_equal(ax, x, 1e-4f)) return 1;
        if (!nearly_equal(ay, y, 1e-4f)) return 1;
    }

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

    printf("core_viewport2d tests passed\n");
    return 0;
}
