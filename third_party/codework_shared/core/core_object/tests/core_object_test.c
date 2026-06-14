#include "core_object.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int nearly_equal(double a, double b, double eps) {
    return fabs(a - b) <= eps;
}

static int expect_ok(CoreResult r) {
    return r.code == CORE_OK;
}

static int expect_invalid_arg(CoreResult r) {
    return r.code == CORE_ERR_INVALID_ARG;
}

int main(void) {
    CoreObject object;
    CoreResult r;
    char long_id[80];
    char long_type[80];

    memset(long_id, 'a', sizeof(long_id) - 1);
    long_id[sizeof(long_id) - 1] = '\0';
    memset(long_type, 'b', sizeof(long_type) - 1);
    long_type[sizeof(long_type) - 1] = '\0';

    core_object_init(NULL);

    core_object_init(&object);
    if (object.object_id[0] != '\0') return 1;
    if (object.object_type[0] != '\0') return 1;
    if (object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED) return 1;
    if (object.locked_plane != CORE_OBJECT_PLANE_XY) return 1;
    if (!nearly_equal(object.transform.scale.x, 1.0, 1e-9)) return 1;
    if (!nearly_equal(object.transform.scale.y, 1.0, 1e-9)) return 1;
    if (!nearly_equal(object.transform.scale.z, 1.0, 1e-9)) return 1;
    if (!object.flags.visible) return 1;
    if (object.flags.locked) return 1;
    if (!object.flags.selectable) return 1;

    if (!expect_invalid_arg(core_object_validate(NULL))) return 1;
    if (!expect_invalid_arg(core_object_set_identity(NULL, "obj_01", "curve_path"))) return 1;
    if (!expect_invalid_arg(core_object_set_identity(&object, NULL, "curve_path"))) return 1;
    if (!expect_invalid_arg(core_object_set_identity(&object, "obj_01", NULL))) return 1;
    if (!expect_invalid_arg(core_object_set_identity(&object, "", "curve_path"))) return 1;
    if (!expect_invalid_arg(core_object_set_identity(&object, "obj_01", ""))) return 1;
    if (!expect_invalid_arg(core_object_set_identity(&object, long_id, "curve_path"))) return 1;
    if (!expect_invalid_arg(core_object_set_identity(&object, "obj_01", long_type))) return 1;
    if (!expect_invalid_arg(core_object_set_plane_lock(NULL, CORE_OBJECT_PLANE_XY))) return 1;
    if (!expect_invalid_arg(core_object_set_plane_lock(&object, (CoreObjectPlane) 99))) return 1;
    if (!expect_invalid_arg(core_object_promote_to_full_3d(NULL))) return 1;
    if (!expect_invalid_arg(core_object_enforce_dimensional_rules(NULL))) return 1;
    if (!expect_invalid_arg(core_object_validate(&object))) return 1;

    r = core_object_set_identity(&object, "obj_01", "curve_path");
    if (!expect_ok(r)) return 1;

    object.transform.position.x = 2.0;
    object.transform.position.y = 3.0;
    object.transform.position.z = 10.0;
    r = core_object_set_plane_lock(&object, CORE_OBJECT_PLANE_XY);
    if (!expect_ok(r)) return 1;
    if (!nearly_equal(object.transform.position.x, 2.0, 1e-9)) return 1;
    if (!nearly_equal(object.transform.position.y, 3.0, 1e-9)) return 1;
    if (!nearly_equal(object.transform.position.z, 0.0, 1e-9)) return 1;

    object.transform.position.x = 2.0;
    object.transform.position.y = 3.0;
    object.transform.position.z = 4.0;
    r = core_object_set_plane_lock(&object, CORE_OBJECT_PLANE_YZ);
    if (!expect_ok(r)) return 1;
    if (!nearly_equal(object.transform.position.x, 0.0, 1e-9)) return 1;
    if (!nearly_equal(object.transform.position.y, 3.0, 1e-9)) return 1;
    if (!nearly_equal(object.transform.position.z, 4.0, 1e-9)) return 1;

    object.transform.position.x = 5.0;
    object.transform.position.y = 6.0;
    object.transform.position.z = 7.0;
    r = core_object_set_plane_lock(&object, CORE_OBJECT_PLANE_XZ);
    if (!expect_ok(r)) return 1;
    if (!nearly_equal(object.transform.position.x, 5.0, 1e-9)) return 1;
    if (!nearly_equal(object.transform.position.y, 0.0, 1e-9)) return 1;
    if (!nearly_equal(object.transform.position.z, 7.0, 1e-9)) return 1;

    r = core_object_promote_to_full_3d(&object);
    if (!expect_ok(r)) return 1;
    if (object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D) return 1;

    object.locked_plane = CORE_OBJECT_PLANE_YZ;
    object.transform.position.x = 11.0;
    object.transform.position.y = 12.0;
    object.transform.position.z = 13.0;
    r = core_object_enforce_dimensional_rules(&object);
    if (!expect_ok(r)) return 1;
    if (!nearly_equal(object.transform.position.x, 11.0, 1e-9)) return 1;
    if (!nearly_equal(object.transform.position.y, 12.0, 1e-9)) return 1;
    if (!nearly_equal(object.transform.position.z, 13.0, 1e-9)) return 1;

    object.transform.scale.x = 1.0;
    object.transform.scale.y = 2.0;
    object.transform.scale.z = 3.0;
    r = core_object_validate(&object);
    if (!expect_ok(r)) return 1;

    object.transform.scale.z = 0.0;
    if (!expect_invalid_arg(core_object_validate(&object))) return 1;

    object.transform.scale.z = -1.0;
    if (!expect_invalid_arg(core_object_validate(&object))) return 1;

    object.transform.scale.z = 1.0;
    object.transform.position.x = NAN;
    if (!expect_invalid_arg(core_object_validate(&object))) return 1;
    object.transform.position.x = 0.0;

    object.transform.rotation_deg.y = INFINITY;
    if (!expect_invalid_arg(core_object_validate(&object))) return 1;
    object.transform.rotation_deg.y = 0.0;

    object.transform.scale.y = NAN;
    if (!expect_invalid_arg(core_object_validate(&object))) return 1;
    object.transform.scale.y = 1.0;

    object.dimensional_mode = (CoreObjectDimensionalMode) 99;
    if (!expect_invalid_arg(core_object_validate(&object))) return 1;

    object.dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    object.locked_plane = (CoreObjectPlane) 99;
    if (!expect_invalid_arg(core_object_validate(&object))) return 1;

    printf("core_object tests passed\n");
    return 0;
}
