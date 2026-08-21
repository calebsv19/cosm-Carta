#include "core_space_frame_3d.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "core_space_frame_3d_test failed at line %d: %s\n", \
            __LINE__, #condition); return 1; } } while (0)

static int near(double a, double b) { return fabs(a - b) <= 1e-12; }
static int vec_near(CoreSpaceVec3d a, CoreSpaceVec3d b) {
    return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z);
}

int main(void) {
    CoreSpaceFrame3D basis = core_space_frame_3d_y_up_to_z_up();
    CoreSpaceFrame3D inverse;
    CoreSpaceVec3d mapped = {99, 99, 99};
    CHECK(core_space_frame_3d_validate(&basis).code == CORE_OK);
    CHECK(!strcmp(core_space_coordinate_system_3d_name(basis.source),
                  CORE_SPACE_COORDINATE_SYSTEM_RIGHT_HANDED_Y_UP_METERS));
    CHECK(!strcmp(core_space_coordinate_system_3d_name(basis.target),
                  CORE_SPACE_COORDINATE_SYSTEM_RIGHT_HANDED_Z_UP_METERS));
    CHECK(core_space_frame_3d_map_vec(
              &basis, (CoreSpaceVec3d){2, 3, 5}, &mapped).code == CORE_OK);
    CHECK(vec_near(mapped, (CoreSpaceVec3d){2, -5, 3}));
    CHECK(core_space_frame_3d_inverse(&basis, &inverse).code == CORE_OK);
    CHECK(core_space_frame_3d_map_vec(&inverse, mapped, &mapped).code == CORE_OK);
    CHECK(vec_near(mapped, (CoreSpaceVec3d){2, 3, 5}));

    CoreSpaceVec3d half;
    CHECK(core_space_frame_3d_map_half_extent(
              &basis, (CoreSpaceVec3d){8, .5, 6}, &half).code == CORE_OK);
    CHECK(vec_near(half, (CoreSpaceVec3d){8, 6, .5}));

    CoreSpaceAabb3d box;
    CHECK(core_space_frame_3d_map_aabb(
              &basis, (CoreSpaceAabb3d){{-2, 1, -4}, {3, 5, 7}},
              &box).code == CORE_OK);
    CHECK(vec_near(box.min, (CoreSpaceVec3d){-2, -7, 1}));
    CHECK(vec_near(box.max, (CoreSpaceVec3d){3, 4, 5}));

    CoreSpacePlane3d plane;
    CHECK(core_space_frame_3d_map_plane(
              &basis, (CoreSpacePlane3d){{0, 1, 0}, 2.5},
              &plane).code == CORE_OK);
    CHECK(vec_near(plane.normal, (CoreSpaceVec3d){0, 0, 1}));
    CHECK(near(plane.offset, 2.5));

    const double angle = .47;
    CoreSpaceQuatd quaternion = {cos(angle / 2), 0, sin(angle / 2), 0};
    CoreSpaceQuatd mapped_quaternion;
    CHECK(core_space_frame_3d_map_quat(
              &basis, quaternion, &mapped_quaternion).code == CORE_OK);
    CHECK(near(mapped_quaternion.w, quaternion.w));
    CHECK(near(mapped_quaternion.x, 0));
    CHECK(near(mapped_quaternion.y, 0));
    CHECK(near(mapped_quaternion.z, quaternion.y));

    CoreSpaceMat3d inertia = {{{2, .1, .2}, {.1, 3, .3}, {.2, .3, 5}}};
    CoreSpaceMat3d mapped_inertia;
    CHECK(core_space_frame_3d_map_mat3(
              &basis, inertia, &mapped_inertia).code == CORE_OK);
    CHECK(near(mapped_inertia.m[0][0], 2));
    CHECK(near(mapped_inertia.m[1][1], 5));
    CHECK(near(mapped_inertia.m[2][2], 3));
    CHECK(near(mapped_inertia.m[0][1], -.2));
    CHECK(near(mapped_inertia.m[0][2], .1));
    CHECK(near(mapped_inertia.m[1][2], -.3));

    CoreSpaceFrame3D reflected = basis;
    reflected.target_from_source.m[0][0] = -1;
    CHECK(core_space_frame_3d_validate(&reflected).code == CORE_ERR_INVALID_ARG);
    mapped = (CoreSpaceVec3d){11, 12, 13};
    CHECK(core_space_frame_3d_map_vec(
              &basis, (CoreSpaceVec3d){NAN, 0, 0}, &mapped).code ==
          CORE_ERR_INVALID_ARG);
    CHECK(vec_near(mapped, (CoreSpaceVec3d){11, 12, 13}));

    printf("core_space_frame_3d tests passed\n");
    return 0;
}
