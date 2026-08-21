#ifndef CORE_SPACE_FRAME_3D_H
#define CORE_SPACE_FRAME_3D_H

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_SPACE_COORDINATE_SYSTEM_RIGHT_HANDED_Y_UP_METERS \
    "right_handed_y_up_meters"
#define CORE_SPACE_COORDINATE_SYSTEM_RIGHT_HANDED_Z_UP_METERS \
    "right_handed_z_up_meters"

typedef enum CoreSpaceCoordinateSystem3D {
    CORE_SPACE_COORDINATE_SYSTEM_3D_INVALID = 0,
    CORE_SPACE_COORDINATE_SYSTEM_3D_RIGHT_HANDED_Y_UP_METERS,
    CORE_SPACE_COORDINATE_SYSTEM_3D_RIGHT_HANDED_Z_UP_METERS
} CoreSpaceCoordinateSystem3D;

typedef struct CoreSpaceVec3d {
    double x;
    double y;
    double z;
} CoreSpaceVec3d;

typedef struct CoreSpaceMat3d {
    double m[3][3];
} CoreSpaceMat3d;

typedef struct CoreSpaceQuatd {
    double w;
    double x;
    double y;
    double z;
} CoreSpaceQuatd;

typedef struct CoreSpaceAabb3d {
    CoreSpaceVec3d min;
    CoreSpaceVec3d max;
} CoreSpaceAabb3d;

/* Plane equation: dot(normal, point) = offset. */
typedef struct CoreSpacePlane3d {
    CoreSpaceVec3d normal;
    double offset;
} CoreSpacePlane3d;

/* An origin-preserving proper rigid basis. target_from_source maps points,
 * free vectors, and pseudovectors from source coordinates into target
 * coordinates. */
typedef struct CoreSpaceFrame3D {
    CoreSpaceCoordinateSystem3D source;
    CoreSpaceCoordinateSystem3D target;
    CoreSpaceMat3d target_from_source;
} CoreSpaceFrame3D;

const char *core_space_coordinate_system_3d_name(
    CoreSpaceCoordinateSystem3D coordinate_system);
CoreSpaceFrame3D core_space_frame_3d_identity_z_up(void);
CoreSpaceFrame3D core_space_frame_3d_y_up_to_z_up(void);
CoreResult core_space_frame_3d_validate(const CoreSpaceFrame3D *frame);
CoreResult core_space_frame_3d_inverse(
    const CoreSpaceFrame3D *frame, CoreSpaceFrame3D *out_inverse);
CoreResult core_space_frame_3d_map_vec(
    const CoreSpaceFrame3D *frame, CoreSpaceVec3d value,
    CoreSpaceVec3d *out_value);
CoreResult core_space_frame_3d_map_mat3(
    const CoreSpaceFrame3D *frame, CoreSpaceMat3d value,
    CoreSpaceMat3d *out_value);
CoreResult core_space_frame_3d_map_quat(
    const CoreSpaceFrame3D *frame, CoreSpaceQuatd value,
    CoreSpaceQuatd *out_value);
CoreResult core_space_frame_3d_map_plane(
    const CoreSpaceFrame3D *frame, CoreSpacePlane3d value,
    CoreSpacePlane3d *out_value);
CoreResult core_space_frame_3d_map_aabb(
    const CoreSpaceFrame3D *frame, CoreSpaceAabb3d value,
    CoreSpaceAabb3d *out_value);
CoreResult core_space_frame_3d_map_half_extent(
    const CoreSpaceFrame3D *frame, CoreSpaceVec3d value,
    CoreSpaceVec3d *out_value);

#ifdef __cplusplus
}
#endif

#endif
