#include "core_object.h"

#include <math.h>
#include <stdio.h>

static int nearly_equal(double a, double b, double eps) {
    return fabs(a - b) <= eps;
}

int main(void) {
    CoreObject object;
    CoreResult r;

    core_object_init(&object);
    r = core_object_set_identity(&object, "obj_01", "curve_path");
    if (r.code != CORE_OK) return 1;

    object.transform.position.z = 10.0;
    r = core_object_set_plane_lock(&object, CORE_OBJECT_PLANE_XY);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(object.transform.position.z, 0.0, 1e-9)) return 1;

    object.transform.position.x = 2.0;
    r = core_object_set_plane_lock(&object, CORE_OBJECT_PLANE_YZ);
    if (r.code != CORE_OK) return 1;
    if (!nearly_equal(object.transform.position.x, 0.0, 1e-9)) return 1;

    r = core_object_promote_to_full_3d(&object);
    if (r.code != CORE_OK) return 1;
    if (object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D) return 1;

    object.transform.scale.x = 1.0;
    object.transform.scale.y = 2.0;
    object.transform.scale.z = 3.0;
    r = core_object_validate(&object);
    if (r.code != CORE_OK) return 1;

    object.transform.scale.z = 0.0;
    r = core_object_validate(&object);
    if (r.code == CORE_OK) return 1;

    printf("core_object tests passed\n");
    return 0;
}
