#include "core_viewport3d.h"

#include <float.h>
#include <math.h>

static const double CORE_VIEWPORT3D_PI = 3.14159265358979323846264338327950288;
static const double CORE_VIEWPORT3D_TAU = 6.28318530717958647692528676655900576;
static const double CORE_VIEWPORT3D_SPAN_EPSILON = 1e-12;

static CoreResult core_viewport3d_invalid(const char *message) {
    CoreResult result = {CORE_ERR_INVALID_ARG, message};
    return result;
}

static int core_viewport3d_vec3_finite(CoreViewport3DVec3d value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static double core_viewport3d_normalize_angle(double angle_rad) {
    double normalized = fmod(angle_rad, CORE_VIEWPORT3D_TAU);
    if (normalized <= -CORE_VIEWPORT3D_PI) {
        normalized += CORE_VIEWPORT3D_TAU;
    } else if (normalized > CORE_VIEWPORT3D_PI) {
        normalized -= CORE_VIEWPORT3D_TAU;
    }
    return normalized;
}

static double core_viewport3d_clamp_scale(const CoreViewport3DState *state,
                                          double scale) {
    if (scale < state->min_scale_px_per_world_unit) {
        return state->min_scale_px_per_world_unit;
    }
    if (scale > state->max_scale_px_per_world_unit) {
        return state->max_scale_px_per_world_unit;
    }
    return scale;
}

CoreResult core_viewport3d_state_validate(const CoreViewport3DState *state) {
    if (!state) {
        return core_viewport3d_invalid("state is required");
    }
    if (!core_viewport3d_vec3_finite(state->target) ||
        !isfinite(state->orientation.azimuth_rad) ||
        !isfinite(state->orientation.elevation_rad) ||
        !isfinite(state->scale_px_per_world_unit) ||
        !isfinite(state->min_scale_px_per_world_unit) ||
        !isfinite(state->max_scale_px_per_world_unit)) {
        return core_viewport3d_invalid("state values must be finite");
    }
    if (state->scale_px_per_world_unit <= 0.0 ||
        state->min_scale_px_per_world_unit <= 0.0 ||
        state->max_scale_px_per_world_unit <= 0.0) {
        return core_viewport3d_invalid("scale values must be positive");
    }
    if (state->min_scale_px_per_world_unit > state->max_scale_px_per_world_unit) {
        return core_viewport3d_invalid("minimum scale must not exceed maximum scale");
    }
    if (state->scale_px_per_world_unit < state->min_scale_px_per_world_unit ||
        state->scale_px_per_world_unit > state->max_scale_px_per_world_unit) {
        return core_viewport3d_invalid("scale must stay within bounds");
    }
    return core_result_ok();
}

CoreResult core_viewport3d_state_init(CoreViewport3DState *out_state,
                                      CoreViewport3DVec3d target,
                                      CoreViewport3DOrientation orientation,
                                      double scale_px_per_world_unit,
                                      double min_scale_px_per_world_unit,
                                      double max_scale_px_per_world_unit) {
    CoreViewport3DState candidate;
    CoreResult valid;
    if (!out_state) {
        return core_viewport3d_invalid("output state is required");
    }
    candidate.target = target;
    candidate.orientation.azimuth_rad = core_viewport3d_normalize_angle(orientation.azimuth_rad);
    candidate.orientation.elevation_rad = core_viewport3d_normalize_angle(orientation.elevation_rad);
    candidate.scale_px_per_world_unit = scale_px_per_world_unit;
    candidate.min_scale_px_per_world_unit = min_scale_px_per_world_unit;
    candidate.max_scale_px_per_world_unit = max_scale_px_per_world_unit;
    valid = core_viewport3d_state_validate(&candidate);
    if (valid.code != CORE_OK) {
        return valid;
    }
    *out_state = candidate;
    return core_result_ok();
}

CoreResult core_viewport3d_build_basis(const CoreViewport3DOrientation *orientation,
                                       CoreViewport3DBasis *out_basis) {
    CoreViewport3DBasis candidate;
    double azimuth;
    double elevation;
    double cos_azimuth;
    double sin_azimuth;
    double cos_elevation;
    double sin_elevation;
    if (!orientation || !out_basis ||
        !isfinite(orientation->azimuth_rad) ||
        !isfinite(orientation->elevation_rad)) {
        return core_viewport3d_invalid("finite orientation and output basis are required");
    }
    azimuth = core_viewport3d_normalize_angle(orientation->azimuth_rad);
    elevation = core_viewport3d_normalize_angle(orientation->elevation_rad);
    cos_azimuth = cos(azimuth);
    sin_azimuth = sin(azimuth);
    cos_elevation = cos(elevation);
    sin_elevation = sin(elevation);
    candidate.forward = (CoreViewport3DVec3d){
        cos_azimuth * cos_elevation,
        sin_azimuth * cos_elevation,
        sin_elevation
    };
    candidate.right = (CoreViewport3DVec3d){sin_azimuth, -cos_azimuth, 0.0};
    candidate.screen_down = (CoreViewport3DVec3d){
        -cos_azimuth * sin_elevation,
        -sin_azimuth * sin_elevation,
        cos_elevation
    };
    if (!core_viewport3d_vec3_finite(candidate.right) ||
        !core_viewport3d_vec3_finite(candidate.screen_down) ||
        !core_viewport3d_vec3_finite(candidate.forward)) {
        return core_viewport3d_invalid("orientation did not produce a finite basis");
    }
    *out_basis = candidate;
    return core_result_ok();
}

CoreResult core_viewport3d_apply(const CoreViewport3DState *state,
                                 const CoreViewport3DCommand *command,
                                 CoreViewport3DState *out_state) {
    CoreViewport3DState candidate;
    CoreViewport3DBasis basis;
    CoreResult valid;
    if (!command || !out_state) {
        return core_viewport3d_invalid("command and output state are required");
    }
    valid = core_viewport3d_state_validate(state);
    if (valid.code != CORE_OK) {
        return valid;
    }
    candidate = *state;

    switch (command->kind) {
        case CORE_VIEWPORT3D_COMMAND_ORBIT:
            if (!isfinite(command->value.orbit.azimuth_delta_rad) ||
                !isfinite(command->value.orbit.elevation_delta_rad)) {
                return core_viewport3d_invalid("orbit deltas must be finite");
            }
            candidate.orientation.azimuth_rad = core_viewport3d_normalize_angle(
                candidate.orientation.azimuth_rad + command->value.orbit.azimuth_delta_rad);
            candidate.orientation.elevation_rad = core_viewport3d_normalize_angle(
                candidate.orientation.elevation_rad + command->value.orbit.elevation_delta_rad);
            break;

        case CORE_VIEWPORT3D_COMMAND_PAN: {
            double world_dx;
            double world_dy;
            if (!isfinite(command->value.pan.screen_dx) ||
                !isfinite(command->value.pan.screen_dy)) {
                return core_viewport3d_invalid("pan deltas must be finite");
            }
            valid = core_viewport3d_build_basis(&candidate.orientation, &basis);
            if (valid.code != CORE_OK) return valid;
            world_dx = -command->value.pan.screen_dx / candidate.scale_px_per_world_unit;
            world_dy = -command->value.pan.screen_dy / candidate.scale_px_per_world_unit;
            candidate.target.x += basis.right.x * world_dx + basis.screen_down.x * world_dy;
            candidate.target.y += basis.right.y * world_dx + basis.screen_down.y * world_dy;
            candidate.target.z += basis.right.z * world_dx + basis.screen_down.z * world_dy;
            break;
        }

        case CORE_VIEWPORT3D_COMMAND_ZOOM: {
            double next_scale;
            double scale_delta;
            if (!isfinite(command->value.zoom.factor) || command->value.zoom.factor <= 0.0 ||
                !isfinite(command->value.zoom.anchor_offset_x) ||
                !isfinite(command->value.zoom.anchor_offset_y)) {
                return core_viewport3d_invalid("zoom factor and anchor must be finite and positive");
            }
            next_scale = candidate.scale_px_per_world_unit * command->value.zoom.factor;
            if (!isfinite(next_scale) && command->value.zoom.factor > 1.0) {
                next_scale = candidate.max_scale_px_per_world_unit;
            }
            next_scale = core_viewport3d_clamp_scale(&candidate, next_scale);
            valid = core_viewport3d_build_basis(&candidate.orientation, &basis);
            if (valid.code != CORE_OK) return valid;
            scale_delta = (1.0 / candidate.scale_px_per_world_unit) - (1.0 / next_scale);
            candidate.target.x += basis.right.x * command->value.zoom.anchor_offset_x * scale_delta +
                                  basis.screen_down.x * command->value.zoom.anchor_offset_y * scale_delta;
            candidate.target.y += basis.right.y * command->value.zoom.anchor_offset_x * scale_delta +
                                  basis.screen_down.y * command->value.zoom.anchor_offset_y * scale_delta;
            candidate.target.z += basis.right.z * command->value.zoom.anchor_offset_x * scale_delta +
                                  basis.screen_down.z * command->value.zoom.anchor_offset_y * scale_delta;
            candidate.scale_px_per_world_unit = next_scale;
            break;
        }

        case CORE_VIEWPORT3D_COMMAND_FRAME:
            if (!core_viewport3d_vec3_finite(command->value.frame.target) ||
                !isfinite(command->value.frame.scale_px_per_world_unit) ||
                command->value.frame.scale_px_per_world_unit <= 0.0) {
                return core_viewport3d_invalid("frame target and scale must be finite and positive");
            }
            candidate.target = command->value.frame.target;
            candidate.scale_px_per_world_unit = core_viewport3d_clamp_scale(
                &candidate, command->value.frame.scale_px_per_world_unit);
            break;

        case CORE_VIEWPORT3D_COMMAND_RESET:
            valid = core_viewport3d_state_validate(&command->value.reset_state);
            if (valid.code != CORE_OK) return valid;
            candidate = command->value.reset_state;
            candidate.orientation.azimuth_rad = core_viewport3d_normalize_angle(
                candidate.orientation.azimuth_rad);
            candidate.orientation.elevation_rad = core_viewport3d_normalize_angle(
                candidate.orientation.elevation_rad);
            break;

        case CORE_VIEWPORT3D_COMMAND_RESIZE:
            break;

        case CORE_VIEWPORT3D_COMMAND_NONE:
        default:
            return core_viewport3d_invalid("unsupported viewport command");
    }

    valid = core_viewport3d_state_validate(&candidate);
    if (valid.code != CORE_OK) {
        return valid;
    }
    *out_state = candidate;
    return core_result_ok();
}

CoreResult core_viewport3d_resolve_fit_scale(const CoreViewport3DFitRequest *request,
                                             double *out_scale_px_per_world_unit) {
    double scale = DBL_MAX;
    int has_span = 0;
    if (!request || !out_scale_px_per_world_unit) {
        return core_viewport3d_invalid("fit request and output scale are required");
    }
    if (!isfinite(request->viewport_width_px) || request->viewport_width_px <= 0.0 ||
        !isfinite(request->viewport_height_px) || request->viewport_height_px <= 0.0 ||
        !isfinite(request->projected_span_right_world) || request->projected_span_right_world < 0.0 ||
        !isfinite(request->projected_span_down_world) || request->projected_span_down_world < 0.0 ||
        !isfinite(request->fill_fraction) || request->fill_fraction <= 0.0 ||
        request->fill_fraction > 1.0 ||
        !isfinite(request->min_scale_px_per_world_unit) ||
        request->min_scale_px_per_world_unit <= 0.0 ||
        !isfinite(request->max_scale_px_per_world_unit) ||
        request->max_scale_px_per_world_unit < request->min_scale_px_per_world_unit) {
        return core_viewport3d_invalid("fit request values are invalid");
    }
    if (request->projected_span_right_world > CORE_VIEWPORT3D_SPAN_EPSILON) {
        scale = request->viewport_width_px * request->fill_fraction /
                request->projected_span_right_world;
        has_span = 1;
    }
    if (request->projected_span_down_world > CORE_VIEWPORT3D_SPAN_EPSILON) {
        double vertical_scale = request->viewport_height_px * request->fill_fraction /
                                request->projected_span_down_world;
        if (vertical_scale < scale) scale = vertical_scale;
        has_span = 1;
    }
    if (!has_span || !isfinite(scale)) {
        return core_viewport3d_invalid("fit request needs a non-zero projected span");
    }
    if (scale < request->min_scale_px_per_world_unit) {
        scale = request->min_scale_px_per_world_unit;
    }
    if (scale > request->max_scale_px_per_world_unit) {
        scale = request->max_scale_px_per_world_unit;
    }
    *out_scale_px_per_world_unit = scale;
    return core_result_ok();
}
