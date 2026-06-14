#include "core_space.h"

#include <math.h>
#include <stdio.h>

static int nearly_equal(float a, float b, float eps) {
    return fabsf(a - b) <= eps;
}

static int expect_ok(CoreResult r) {
    return r.code == CORE_OK;
}

static int expect_invalid_arg(CoreResult r) {
    return r.code == CORE_ERR_INVALID_ARG;
}

int main(void) {
    CoreSpaceDesc d;
    CoreResult r = core_space_desc_default_from_grid(96, 96, 0.0f, 0.0f, 1.0f, &d);
    if (!expect_ok(r)) return 1;
    d.author_window_w = 1200;
    d.author_window_h = 800;
    d.desired_fit = 0.25f;

    if (!expect_invalid_arg(core_space_desc_validate(NULL))) return 1;
    if (!expect_invalid_arg(core_space_desc_default_from_grid(96, 96, 0.0f, 0.0f, 1.0f, NULL))) return 1;

    {
        CoreSpaceDesc invalid = d;
        invalid.grid_w = 0;
        if (!expect_invalid_arg(core_space_desc_validate(&invalid))) return 1;
        invalid = d;
        invalid.grid_h = 0;
        if (!expect_invalid_arg(core_space_desc_validate(&invalid))) return 1;
        invalid = d;
        invalid.origin_x = NAN;
        if (!expect_invalid_arg(core_space_desc_validate(&invalid))) return 1;
        invalid = d;
        invalid.origin_y = INFINITY;
        if (!expect_invalid_arg(core_space_desc_validate(&invalid))) return 1;
        invalid = d;
        invalid.cell_size = 0.0f;
        if (!expect_invalid_arg(core_space_desc_validate(&invalid))) return 1;
        invalid = d;
        invalid.cell_size = NAN;
        if (!expect_invalid_arg(core_space_desc_validate(&invalid))) return 1;
        invalid = d;
        invalid.author_window_w = 0;
        if (!expect_invalid_arg(core_space_desc_validate(&invalid))) return 1;
        invalid = d;
        invalid.author_window_h = 0;
        if (!expect_invalid_arg(core_space_desc_validate(&invalid))) return 1;
        invalid = d;
        invalid.desired_fit = 0.0f;
        if (!expect_invalid_arg(core_space_desc_validate(&invalid))) return 1;
        invalid = d;
        invalid.desired_fit = NAN;
        if (!expect_invalid_arg(core_space_desc_validate(&invalid))) return 1;
    }

    {
        CoreSpaceDesc fallback_desc;
        r = core_space_desc_default_from_grid(3, 4, 10.0f, 20.0f, 0.0f, &fallback_desc);
        if (!expect_ok(r)) return 1;
        if (!nearly_equal(fallback_desc.cell_size, 1.0f, 1e-6f)) return 1;
        if (fallback_desc.author_window_w != 1200) return 1;
        if (fallback_desc.author_window_h != 800) return 1;
        if (!nearly_equal(fallback_desc.desired_fit, 0.25f, 1e-6f)) return 1;
    }

    {
        float sx = 0.0f, sy = 0.0f;
        r = core_space_compute_span_from_window(1200, 800, &sx, &sy);
        if (!expect_ok(r)) return 1;
        if (!nearly_equal(sx, 0.75f, 1e-6f)) return 1;
        if (!nearly_equal(sy, 0.5f, 1e-6f)) return 1;
    }

    {
        float sx = 11.0f, sy = 22.0f;
        if (!expect_invalid_arg(core_space_compute_span_from_window(1200, 800, NULL, &sy))) return 1;
        if (!expect_invalid_arg(core_space_compute_span_from_window(1200, 800, &sx, NULL))) return 1;
        if (!expect_invalid_arg(core_space_compute_span_from_window(0, 800, &sx, &sy))) return 1;
        if (!expect_invalid_arg(core_space_compute_span_from_window(1200, 0, &sx, &sy))) return 1;
        if (!nearly_equal(sx, 11.0f, 1e-6f)) return 1;
        if (!nearly_equal(sy, 22.0f, 1e-6f)) return 1;
    }

    {
        float ux = core_space_import_pos_to_unit(0.5f, 0.75f);
        float uy = core_space_import_pos_to_unit(0.5f, 0.5f);
        if (!nearly_equal(ux, 0.5f, 1e-6f)) return 1;
        if (!nearly_equal(uy, 0.5f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_import_pos_to_unit(-10.0f, 0.75f), 0.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_import_pos_to_unit(10.0f, 0.75f), 1.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_import_pos_to_unit(0.5f, 0.0f), 0.5f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_import_pos_to_unit(NAN, 0.75f), 0.5f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_import_pos_to_unit(0.5f, NAN), 0.5f, 1e-6f)) return 1;
    }

    {
        float wx = core_space_unit_to_world_x(&d, 0.5f);
        float wy = core_space_unit_to_world_y(&d, 0.5f);
        if (!nearly_equal(wx, 47.5f, 1e-4f)) return 1;
        if (!nearly_equal(wy, 47.5f, 1e-4f)) return 1;
        if (!nearly_equal(core_space_world_to_unit_x(&d, wx), 0.5f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_world_to_unit_y(&d, wy), 0.5f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_unit_to_world_x(&d, -5.0f), 0.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_unit_to_world_x(&d, 5.0f), 95.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_world_to_unit_x(&d, -100.0f), 0.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_world_to_unit_x(&d, 1000.0f), 1.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_unit_to_world_x(NULL, 0.5f), 0.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_world_to_unit_y(NULL, 0.5f), 0.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_unit_to_world_y(&d, NAN), 0.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_world_to_unit_y(&d, NAN), 0.0f, 1e-6f)) return 1;
    }

    {
        CoreSpaceDesc one_cell;
        r = core_space_desc_default_from_grid(1, 1, 10.0f, 20.0f, 2.0f, &one_cell);
        if (!expect_ok(r)) return 1;
        if (!nearly_equal(core_space_unit_to_world_x(&one_cell, 0.0f), 10.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_unit_to_world_x(&one_cell, 1.0f), 12.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_unit_to_world_y(&one_cell, 0.0f), 20.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_unit_to_world_y(&one_cell, 1.0f), 22.0f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_world_to_unit_x(&one_cell, 11.0f), 0.5f, 1e-6f)) return 1;
        if (!nearly_equal(core_space_world_to_unit_y(&one_cell, 21.0f), 0.5f, 1e-6f)) return 1;
    }

    {
        float s = core_space_fit_scale(1.0f, 0.25f, 2.0f, 96, 96, 1.0f);
        if (!nearly_equal(s, 12.0f, 1e-4f)) return 1;
        if (!nearly_equal(core_space_fit_scale(0.0f, 0.25f, 2.0f, 96, 96, 1.0f), 12.0f, 1e-4f)) return 1;
        if (!nearly_equal(core_space_fit_scale(1.0f, 0.0f, 2.0f, 96, 96, 1.0f), 12.0f, 1e-4f)) return 1;
        if (!nearly_equal(core_space_fit_scale(1.0f, 0.25f, 0.0f, 96, 96, 1.0f), 24.0f, 1e-4f)) return 1;
        if (!nearly_equal(core_space_fit_scale(1.0f, 0.25f, 2.0f, 0, 96, 1.0f), 0.125f, 1e-4f)) return 1;
        if (!nearly_equal(core_space_fit_scale(1.0f, 0.25f, 2.0f, 96, 96, 0.0f), 12.0f, 1e-4f)) return 1;
        if (!nearly_equal(core_space_fit_scale(NAN, 0.25f, 2.0f, 96, 96, 1.0f), 12.0f, 1e-4f)) return 1;
        if (!nearly_equal(core_space_fit_scale(1.0f, NAN, 2.0f, 96, 96, 1.0f), 12.0f, 1e-4f)) return 1;
        if (!nearly_equal(core_space_fit_scale(1.0f, 0.25f, NAN, 96, 96, 1.0f), 24.0f, 1e-4f)) return 1;
        if (!nearly_equal(core_space_fit_scale(1.0f, 0.25f, 2.0f, 96, 96, NAN), 12.0f, 1e-4f)) return 1;
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
        if (!expect_ok(r)) return 1;
        if (!nearly_equal(out.x, 47.5f, 1e-4f)) return 1;
        if (!nearly_equal(out.y, 47.5f, 1e-4f)) return 1;
        if (!nearly_equal(out.rotation_deg, 30.0f, 1e-6f)) return 1;
        if (!nearly_equal(out.scale, 12.0f, 1e-4f)) return 1;
    }

    {
        CoreSpaceImport in = { 0.5f, 0.5f, 30.0f, 1.0f, 2.0f };
        CoreSpaceWorldTransform out = { 101.0f, 202.0f, 303.0f, 404.0f };
        CoreSpaceDesc invalid = d;
        invalid.author_window_w = 0;

        if (!expect_invalid_arg(core_space_import_to_world(NULL, &in, &out))) return 1;
        if (!expect_invalid_arg(core_space_import_to_world(&d, NULL, &out))) return 1;
        if (!expect_invalid_arg(core_space_import_to_world(&d, &in, NULL))) return 1;
        if (!expect_invalid_arg(core_space_import_to_world(&invalid, &in, &out))) return 1;
        if (!nearly_equal(out.x, 101.0f, 1e-6f)) return 1;
        if (!nearly_equal(out.y, 202.0f, 1e-6f)) return 1;
        if (!nearly_equal(out.rotation_deg, 303.0f, 1e-6f)) return 1;
        if (!nearly_equal(out.scale, 404.0f, 1e-6f)) return 1;

        in.rotation_deg = NAN;
        if (!expect_invalid_arg(core_space_import_to_world(&d, &in, &out))) return 1;
        if (!nearly_equal(out.x, 101.0f, 1e-6f)) return 1;
        if (!nearly_equal(out.y, 202.0f, 1e-6f)) return 1;
        if (!nearly_equal(out.rotation_deg, 303.0f, 1e-6f)) return 1;
        if (!nearly_equal(out.scale, 404.0f, 1e-6f)) return 1;
    }

    printf("core_space tests passed\n");
    return 0;
}
