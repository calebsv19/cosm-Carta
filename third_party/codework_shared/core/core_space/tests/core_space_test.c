#include "core_space.h"

#include <math.h>
#include <stdio.h>

static int nearly_equal(float a, float b, float eps) {
    return fabsf(a - b) <= eps;
}

int main(void) {
    CoreSpaceDesc d;
    CoreResult r = core_space_desc_default_from_grid(96, 96, 0.0f, 0.0f, 1.0f, &d);
    if (r.code != CORE_OK) return 1;
    d.author_window_w = 1200;
    d.author_window_h = 800;
    d.desired_fit = 0.25f;

    {
        float sx = 0.0f, sy = 0.0f;
        r = core_space_compute_span_from_window(1200, 800, &sx, &sy);
        if (r.code != CORE_OK) return 1;
        if (!nearly_equal(sx, 0.75f, 1e-6f)) return 1;
        if (!nearly_equal(sy, 0.5f, 1e-6f)) return 1;
    }

    {
        float ux = core_space_import_pos_to_unit(0.5f, 0.75f);
        float uy = core_space_import_pos_to_unit(0.5f, 0.5f);
        if (!nearly_equal(ux, 0.5f, 1e-6f)) return 1;
        if (!nearly_equal(uy, 0.5f, 1e-6f)) return 1;
    }

    {
        float wx = core_space_unit_to_world_x(&d, 0.5f);
        float wy = core_space_unit_to_world_y(&d, 0.5f);
        if (!nearly_equal(wx, 47.5f, 1e-4f)) return 1;
        if (!nearly_equal(wy, 47.5f, 1e-4f)) return 1;
        if (!nearly_equal(core_space_world_to_unit_x(&d, wx), 0.5f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_world_to_unit_y(&d, wy), 0.5f, 1e-6f)) return 1;
    }

    {
        float s = core_space_fit_scale(1.0f, 0.25f, 2.0f, 96, 96, 1.0f);
        if (!nearly_equal(s, 12.0f, 1e-4f)) return 1;
    }

    {
        CoreSpaceImport in;
        CoreSpaceWorldTransform out;
        in.pos_x_raw = 0.5f;
        in.pos_y_raw = 0.5f;
        in.rotation_deg = 30.0f;
        in.scale = 1.0f;
        in.asset_max_dim = 2.0f;

        r = core_space_import_to_world(&d, &in, &out);
        if (r.code != CORE_OK) return 1;
        if (!nearly_equal(out.x, 47.5f, 1e-4f)) return 1;
        if (!nearly_equal(out.y, 47.5f, 1e-4f)) return 1;
        if (!nearly_equal(out.rotation_deg, 30.0f, 1e-6f)) return 1;
        if (!nearly_equal(out.scale, 12.0f, 1e-4f)) return 1;
    }

    printf("core_space tests passed\n");
    return 0;
}
