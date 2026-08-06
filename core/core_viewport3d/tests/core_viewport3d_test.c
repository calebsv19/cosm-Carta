#include "core_viewport3d.h"

#include <math.h>
#include <stdio.h>

static const double PI = 3.14159265358979323846264338327950288;

static int close_enough(double actual, double expected) {
    return isfinite(actual) && fabs(actual - expected) <= 1e-9;
}

static double dot(CoreViewport3DVec3d a, CoreViewport3DVec3d b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static CoreViewport3DState base_state(void) {
    CoreViewport3DState state = {0};
    CoreResult result = core_viewport3d_state_init(
        &state,
        (CoreViewport3DVec3d){4.0, -3.0, 2.0},
        (CoreViewport3DOrientation){35.0 * PI / 180.0, -20.0 * PI / 180.0},
        16.0,
        1.0,
        100.0);
    if (result.code != CORE_OK) {
        state.scale_px_per_world_unit = 0.0;
    }
    return state;
}

static int test_basis(void) {
    CoreViewport3DBasis basis = {0};
    CoreViewport3DBasis sentinel = {{91.0, 0.0, 0.0}, {0}, {0}};
    CoreViewport3DOrientation orientation = {0.0, 0.0};
    CoreResult result = core_viewport3d_build_basis(&orientation, &basis);
    if (result.code != CORE_OK) return 0;
    if (!close_enough(basis.forward.x, 1.0) ||
        !close_enough(basis.right.y, -1.0) ||
        !close_enough(basis.screen_down.z, 1.0)) return 0;
    orientation = (CoreViewport3DOrientation){0.37, -0.48};
    result = core_viewport3d_build_basis(&orientation, &basis);
    if (result.code != CORE_OK) return 0;
    if (!close_enough(dot(basis.right, basis.right), 1.0) ||
        !close_enough(dot(basis.screen_down, basis.screen_down), 1.0) ||
        !close_enough(dot(basis.forward, basis.forward), 1.0) ||
        !close_enough(dot(basis.right, basis.screen_down), 0.0) ||
        !close_enough(dot(basis.right, basis.forward), 0.0) ||
        !close_enough(dot(basis.screen_down, basis.forward), 0.0)) return 0;
    result = core_viewport3d_build_basis(
        &(CoreViewport3DOrientation){NAN, 0.0}, &sentinel);
    return result.code == CORE_ERR_INVALID_ARG && close_enough(sentinel.right.x, 91.0);
}

static int test_pan_and_zoom(void) {
    CoreViewport3DState before = base_state();
    CoreViewport3DState after = {0};
    CoreViewport3DState sentinel = base_state();
    CoreViewport3DBasis basis = {0};
    CoreViewport3DVec3d delta;
    CoreViewport3DCommand command = {0};
    CoreResult result = core_viewport3d_build_basis(&before.orientation, &basis);
    if (result.code != CORE_OK) return 0;
    command.kind = CORE_VIEWPORT3D_COMMAND_PAN;
    command.value.pan.screen_dx = 32.0;
    command.value.pan.screen_dy = -16.0;
    result = core_viewport3d_apply(&before, &command, &after);
    if (result.code != CORE_OK) return 0;
    delta = (CoreViewport3DVec3d){after.target.x - before.target.x,
                                  after.target.y - before.target.y,
                                  after.target.z - before.target.z};
    if (!close_enough(dot(delta, basis.right), -2.0) ||
        !close_enough(dot(delta, basis.screen_down), 1.0) ||
        !close_enough(dot(delta, basis.forward), 0.0)) return 0;

    command.kind = CORE_VIEWPORT3D_COMMAND_ZOOM;
    command.value.zoom.factor = 2.0;
    command.value.zoom.anchor_offset_x = 48.0;
    command.value.zoom.anchor_offset_y = -24.0;
    result = core_viewport3d_apply(&before, &command, &after);
    if (result.code != CORE_OK || !close_enough(after.scale_px_per_world_unit, 32.0)) return 0;
    delta = (CoreViewport3DVec3d){after.target.x - before.target.x,
                                  after.target.y - before.target.y,
                                  after.target.z - before.target.z};
    if (!close_enough(dot(delta, basis.right), 1.5) ||
        !close_enough(dot(delta, basis.screen_down), -0.75) ||
        !close_enough(dot(delta, basis.forward), 0.0)) return 0;

    command.kind = CORE_VIEWPORT3D_COMMAND_PAN;
    command.value.pan.screen_dx = NAN;
    sentinel.target.x = 91.0;
    result = core_viewport3d_apply(&before, &command, &sentinel);
    return result.code == CORE_ERR_INVALID_ARG && close_enough(sentinel.target.x, 91.0);
}

static int test_orbit_frame_reset_resize(void) {
    CoreViewport3DState before = base_state();
    CoreViewport3DState after = {0};
    CoreViewport3DCommand command = {0};
    CoreResult result;
    command.kind = CORE_VIEWPORT3D_COMMAND_ORBIT;
    command.value.orbit.azimuth_delta_rad = 2.0 * PI;
    command.value.orbit.elevation_delta_rad = 0.25;
    result = core_viewport3d_apply(&before, &command, &after);
    if (result.code != CORE_OK ||
        !close_enough(after.target.x, before.target.x) ||
        !close_enough(after.orientation.azimuth_rad, before.orientation.azimuth_rad) ||
        !close_enough(after.orientation.elevation_rad,
                      before.orientation.elevation_rad + 0.25)) return 0;

    command.kind = CORE_VIEWPORT3D_COMMAND_FRAME;
    command.value.frame.target = (CoreViewport3DVec3d){8.0, 9.0, 10.0};
    command.value.frame.scale_px_per_world_unit = 1000.0;
    result = core_viewport3d_apply(&before, &command, &after);
    if (result.code != CORE_OK || !close_enough(after.target.z, 10.0) ||
        !close_enough(after.scale_px_per_world_unit, 100.0)) return 0;

    command.kind = CORE_VIEWPORT3D_COMMAND_RESIZE;
    result = core_viewport3d_apply(&before, &command, &after);
    if (result.code != CORE_OK || !close_enough(after.target.x, before.target.x) ||
        !close_enough(after.scale_px_per_world_unit, before.scale_px_per_world_unit)) return 0;

    command.kind = CORE_VIEWPORT3D_COMMAND_RESET;
    command.value.reset_state = base_state();
    command.value.reset_state.target.x = -4.0;
    result = core_viewport3d_apply(&before, &command, &after);
    return result.code == CORE_OK && close_enough(after.target.x, -4.0);
}

static int test_fit_and_validation(void) {
    CoreViewport3DFitRequest request = {
        1000.0, 800.0, 50.0, 20.0, 0.84, 1.0, 100.0
    };
    double scale = 91.0;
    CoreResult result = core_viewport3d_resolve_fit_scale(&request, &scale);
    if (result.code != CORE_OK || !close_enough(scale, 16.8)) return 0;
    request.fill_fraction = 0.72;
    result = core_viewport3d_resolve_fit_scale(&request, &scale);
    if (result.code != CORE_OK || !close_enough(scale, 14.4)) return 0;
    request.projected_span_right_world = 0.0;
    request.projected_span_down_world = 0.0;
    scale = 91.0;
    result = core_viewport3d_resolve_fit_scale(&request, &scale);
    if (result.code != CORE_ERR_INVALID_ARG || !close_enough(scale, 91.0)) return 0;
    {
        CoreViewport3DState invalid = base_state();
        invalid.min_scale_px_per_world_unit = 20.0;
        invalid.max_scale_px_per_world_unit = 10.0;
        result = core_viewport3d_state_validate(&invalid);
        if (result.code != CORE_ERR_INVALID_ARG) return 0;
    }
    return 1;
}

int main(void) {
    if (!test_basis()) return 1;
    if (!test_pan_and_zoom()) return 1;
    if (!test_orbit_frame_reset_resize()) return 1;
    if (!test_fit_and_validation()) return 1;
    puts("core_viewport3d tests passed");
    return 0;
}
