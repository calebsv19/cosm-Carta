#include "core_space_frame_3d.h"

#include <float.h>
#include <math.h>

static CoreResult invalid(const char *message) {
    CoreResult result = {CORE_ERR_INVALID_ARG, message};
    return result;
}

static int finite_vec(CoreSpaceVec3d value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static int finite_mat(CoreSpaceMat3d value) {
    for (size_t row = 0; row < 3; ++row)
        for (size_t column = 0; column < 3; ++column)
            if (!isfinite(value.m[row][column]))
                return 0;
    return 1;
}

static CoreSpaceVec3d map_unchecked(CoreSpaceMat3d matrix,
                                    CoreSpaceVec3d value) {
    CoreSpaceVec3d result = {0.0, 0.0, 0.0};
    result.x = matrix.m[0][0] * value.x + matrix.m[0][1] * value.y +
        matrix.m[0][2] * value.z;
    result.y = matrix.m[1][0] * value.x + matrix.m[1][1] * value.y +
        matrix.m[1][2] * value.z;
    result.z = matrix.m[2][0] * value.x + matrix.m[2][1] * value.y +
        matrix.m[2][2] * value.z;
    return result;
}

static CoreSpaceMat3d transpose(CoreSpaceMat3d value) {
    CoreSpaceMat3d result = {{{0}}};
    for (size_t row = 0; row < 3; ++row)
        for (size_t column = 0; column < 3; ++column)
            result.m[row][column] = value.m[column][row];
    return result;
}

static CoreSpaceMat3d multiply(CoreSpaceMat3d a, CoreSpaceMat3d b) {
    CoreSpaceMat3d result = {{{0}}};
    for (size_t row = 0; row < 3; ++row)
        for (size_t column = 0; column < 3; ++column)
            for (size_t inner = 0; inner < 3; ++inner)
                result.m[row][column] +=
                    a.m[row][inner] * b.m[inner][column];
    return result;
}

static double determinant(CoreSpaceMat3d value) {
    return value.m[0][0] *
            (value.m[1][1] * value.m[2][2] -
             value.m[1][2] * value.m[2][1]) -
        value.m[0][1] *
            (value.m[1][0] * value.m[2][2] -
             value.m[1][2] * value.m[2][0]) +
        value.m[0][2] *
            (value.m[1][0] * value.m[2][1] -
             value.m[1][1] * value.m[2][0]);
}

const char *core_space_coordinate_system_3d_name(
    CoreSpaceCoordinateSystem3D coordinate_system) {
    if (coordinate_system ==
        CORE_SPACE_COORDINATE_SYSTEM_3D_RIGHT_HANDED_Y_UP_METERS)
        return CORE_SPACE_COORDINATE_SYSTEM_RIGHT_HANDED_Y_UP_METERS;
    if (coordinate_system ==
        CORE_SPACE_COORDINATE_SYSTEM_3D_RIGHT_HANDED_Z_UP_METERS)
        return CORE_SPACE_COORDINATE_SYSTEM_RIGHT_HANDED_Z_UP_METERS;
    return NULL;
}

CoreSpaceFrame3D core_space_frame_3d_identity_z_up(void) {
    CoreSpaceFrame3D frame = {
        CORE_SPACE_COORDINATE_SYSTEM_3D_RIGHT_HANDED_Z_UP_METERS,
        CORE_SPACE_COORDINATE_SYSTEM_3D_RIGHT_HANDED_Z_UP_METERS,
        {{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}}};
    return frame;
}

CoreSpaceFrame3D core_space_frame_3d_y_up_to_z_up(void) {
    CoreSpaceFrame3D frame = {
        CORE_SPACE_COORDINATE_SYSTEM_3D_RIGHT_HANDED_Y_UP_METERS,
        CORE_SPACE_COORDINATE_SYSTEM_3D_RIGHT_HANDED_Z_UP_METERS,
        {{{1.0, 0.0, 0.0}, {0.0, 0.0, -1.0}, {0.0, 1.0, 0.0}}}};
    return frame;
}

CoreResult core_space_frame_3d_validate(const CoreSpaceFrame3D *frame) {
    const double tolerance = 1e-12;
    if (!frame || !core_space_coordinate_system_3d_name(frame->source) ||
        !core_space_coordinate_system_3d_name(frame->target) ||
        !finite_mat(frame->target_from_source))
        return invalid("invalid 3D frame");
    CoreSpaceMat3d product = multiply(
        frame->target_from_source, transpose(frame->target_from_source));
    for (size_t row = 0; row < 3; ++row)
        for (size_t column = 0; column < 3; ++column) {
            const double expected = row == column ? 1.0 : 0.0;
            if (fabs(product.m[row][column] - expected) > tolerance)
                return invalid("3D frame basis must be orthonormal");
        }
    if (fabs(determinant(frame->target_from_source) - 1.0) > tolerance)
        return invalid("3D frame basis must be right handed");
    return core_result_ok();
}

CoreResult core_space_frame_3d_inverse(
    const CoreSpaceFrame3D *frame, CoreSpaceFrame3D *out_inverse) {
    CoreResult validation = core_space_frame_3d_validate(frame);
    if (validation.code != CORE_OK)
        return validation;
    if (!out_inverse)
        return invalid("inverse output is null");
    CoreSpaceFrame3D inverse = {
        frame->target, frame->source, transpose(frame->target_from_source)};
    *out_inverse = inverse;
    return core_result_ok();
}

CoreResult core_space_frame_3d_map_vec(
    const CoreSpaceFrame3D *frame, CoreSpaceVec3d value,
    CoreSpaceVec3d *out_value) {
    CoreResult validation = core_space_frame_3d_validate(frame);
    if (validation.code != CORE_OK)
        return validation;
    if (!finite_vec(value) || !out_value)
        return invalid("invalid 3D vector mapping argument");
    *out_value = map_unchecked(frame->target_from_source, value);
    return core_result_ok();
}

CoreResult core_space_frame_3d_map_mat3(
    const CoreSpaceFrame3D *frame, CoreSpaceMat3d value,
    CoreSpaceMat3d *out_value) {
    CoreResult validation = core_space_frame_3d_validate(frame);
    if (validation.code != CORE_OK)
        return validation;
    if (!finite_mat(value) || !out_value)
        return invalid("invalid 3D matrix mapping argument");
    *out_value = multiply(multiply(frame->target_from_source, value),
                          transpose(frame->target_from_source));
    return core_result_ok();
}

static CoreSpaceMat3d quat_matrix(CoreSpaceQuatd value) {
    const double xx = value.x * value.x;
    const double yy = value.y * value.y;
    const double zz = value.z * value.z;
    const double xy = value.x * value.y;
    const double xz = value.x * value.z;
    const double yz = value.y * value.z;
    const double wx = value.w * value.x;
    const double wy = value.w * value.y;
    const double wz = value.w * value.z;
    CoreSpaceMat3d result = {{{
        1.0 - 2.0 * (yy + zz), 2.0 * (xy - wz), 2.0 * (xz + wy)}, {
        2.0 * (xy + wz), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - wx)}, {
        2.0 * (xz - wy), 2.0 * (yz + wx), 1.0 - 2.0 * (xx + yy)}}};
    return result;
}

static CoreSpaceQuatd matrix_quat(CoreSpaceMat3d value) {
    CoreSpaceQuatd result = {1.0, 0.0, 0.0, 0.0};
    const double trace = value.m[0][0] + value.m[1][1] + value.m[2][2];
    if (trace > 0.0) {
        const double scale = sqrt(trace + 1.0) * 2.0;
        result.w = 0.25 * scale;
        result.x = (value.m[2][1] - value.m[1][2]) / scale;
        result.y = (value.m[0][2] - value.m[2][0]) / scale;
        result.z = (value.m[1][0] - value.m[0][1]) / scale;
    } else if (value.m[0][0] > value.m[1][1] &&
               value.m[0][0] > value.m[2][2]) {
        const double scale = sqrt(1.0 + value.m[0][0] - value.m[1][1] -
                                  value.m[2][2]) * 2.0;
        result.w = (value.m[2][1] - value.m[1][2]) / scale;
        result.x = 0.25 * scale;
        result.y = (value.m[0][1] + value.m[1][0]) / scale;
        result.z = (value.m[0][2] + value.m[2][0]) / scale;
    } else if (value.m[1][1] > value.m[2][2]) {
        const double scale = sqrt(1.0 + value.m[1][1] - value.m[0][0] -
                                  value.m[2][2]) * 2.0;
        result.w = (value.m[0][2] - value.m[2][0]) / scale;
        result.x = (value.m[0][1] + value.m[1][0]) / scale;
        result.y = 0.25 * scale;
        result.z = (value.m[1][2] + value.m[2][1]) / scale;
    } else {
        const double scale = sqrt(1.0 + value.m[2][2] - value.m[0][0] -
                                  value.m[1][1]) * 2.0;
        result.w = (value.m[1][0] - value.m[0][1]) / scale;
        result.x = (value.m[0][2] + value.m[2][0]) / scale;
        result.y = (value.m[1][2] + value.m[2][1]) / scale;
        result.z = 0.25 * scale;
    }
    return result;
}

CoreResult core_space_frame_3d_map_quat(
    const CoreSpaceFrame3D *frame, CoreSpaceQuatd value,
    CoreSpaceQuatd *out_value) {
    CoreResult validation = core_space_frame_3d_validate(frame);
    const double norm2 = value.w * value.w + value.x * value.x +
        value.y * value.y + value.z * value.z;
    if (validation.code != CORE_OK)
        return validation;
    if (!isfinite(norm2) || norm2 <= DBL_MIN || !out_value)
        return invalid("invalid quaternion mapping argument");
    const double inverse_norm = 1.0 / sqrt(norm2);
    value.w *= inverse_norm;
    value.x *= inverse_norm;
    value.y *= inverse_norm;
    value.z *= inverse_norm;
    CoreSpaceMat3d mapped;
    CoreResult mapped_result = core_space_frame_3d_map_mat3(
        frame, quat_matrix(value), &mapped);
    if (mapped_result.code != CORE_OK)
        return mapped_result;
    CoreSpaceQuatd result = matrix_quat(mapped);
    const double result_norm = sqrt(result.w * result.w + result.x * result.x +
        result.y * result.y + result.z * result.z);
    if (!isfinite(result_norm) || result_norm <= DBL_MIN)
        return invalid("mapped quaternion is invalid");
    result.w /= result_norm;
    result.x /= result_norm;
    result.y /= result_norm;
    result.z /= result_norm;
    if (result.w < 0.0) {
        result.w = -result.w;
        result.x = -result.x;
        result.y = -result.y;
        result.z = -result.z;
    }
    *out_value = result;
    return core_result_ok();
}

CoreResult core_space_frame_3d_map_plane(
    const CoreSpaceFrame3D *frame, CoreSpacePlane3d value,
    CoreSpacePlane3d *out_value) {
    if (!out_value || !isfinite(value.offset) || !finite_vec(value.normal))
        return invalid("invalid plane mapping argument");
    CoreSpacePlane3d result = value;
    CoreResult mapped = core_space_frame_3d_map_vec(
        frame, value.normal, &result.normal);
    if (mapped.code != CORE_OK)
        return mapped;
    *out_value = result;
    return core_result_ok();
}

CoreResult core_space_frame_3d_map_aabb(
    const CoreSpaceFrame3D *frame, CoreSpaceAabb3d value,
    CoreSpaceAabb3d *out_value) {
    CoreResult validation = core_space_frame_3d_validate(frame);
    if (validation.code != CORE_OK)
        return validation;
    if (!out_value || !finite_vec(value.min) || !finite_vec(value.max) ||
        value.min.x > value.max.x || value.min.y > value.max.y ||
        value.min.z > value.max.z)
        return invalid("invalid AABB mapping argument");
    CoreSpaceAabb3d result = {
        {INFINITY, INFINITY, INFINITY},
        {-INFINITY, -INFINITY, -INFINITY}};
    for (size_t index = 0; index < 8; ++index) {
        CoreSpaceVec3d corner = {
            index & 1 ? value.max.x : value.min.x,
            index & 2 ? value.max.y : value.min.y,
            index & 4 ? value.max.z : value.min.z};
        corner = map_unchecked(frame->target_from_source, corner);
        result.min.x = fmin(result.min.x, corner.x);
        result.min.y = fmin(result.min.y, corner.y);
        result.min.z = fmin(result.min.z, corner.z);
        result.max.x = fmax(result.max.x, corner.x);
        result.max.y = fmax(result.max.y, corner.y);
        result.max.z = fmax(result.max.z, corner.z);
    }
    *out_value = result;
    return core_result_ok();
}

CoreResult core_space_frame_3d_map_half_extent(
    const CoreSpaceFrame3D *frame, CoreSpaceVec3d value,
    CoreSpaceVec3d *out_value) {
    CoreResult validation = core_space_frame_3d_validate(frame);
    if (validation.code != CORE_OK)
        return validation;
    if (!out_value || !finite_vec(value) || value.x < 0.0 || value.y < 0.0 ||
        value.z < 0.0)
        return invalid("invalid half extent mapping argument");
    CoreSpaceVec3d result = {0.0, 0.0, 0.0};
    result.x = fabs(frame->target_from_source.m[0][0]) * value.x +
        fabs(frame->target_from_source.m[0][1]) * value.y +
        fabs(frame->target_from_source.m[0][2]) * value.z;
    result.y = fabs(frame->target_from_source.m[1][0]) * value.x +
        fabs(frame->target_from_source.m[1][1]) * value.y +
        fabs(frame->target_from_source.m[1][2]) * value.z;
    result.z = fabs(frame->target_from_source.m[2][0]) * value.x +
        fabs(frame->target_from_source.m[2][1]) * value.y +
        fabs(frame->target_from_source.m[2][2]) * value.z;
    *out_value = result;
    return core_result_ok();
}
