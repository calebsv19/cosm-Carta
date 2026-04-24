#ifndef CORE_OBJECT_H
#define CORE_OBJECT_H

#include <stdbool.h>

#include "core_base.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum CoreObjectDimensionalMode {
    CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED = 1,
    CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D = 2
} CoreObjectDimensionalMode;

typedef enum CoreObjectPlane {
    CORE_OBJECT_PLANE_XY = 1,
    CORE_OBJECT_PLANE_YZ = 2,
    CORE_OBJECT_PLANE_XZ = 3
} CoreObjectPlane;

typedef struct CoreObjectVec3 {
    double x;
    double y;
    double z;
} CoreObjectVec3;

typedef struct CoreObjectTransform {
    CoreObjectVec3 position;
    CoreObjectVec3 rotation_deg;
    CoreObjectVec3 scale;
} CoreObjectTransform;

typedef struct CoreObjectFlags {
    bool visible;
    bool locked;
    bool selectable;
} CoreObjectFlags;

typedef struct CoreObject {
    char object_id[64];
    char object_type[64];
    CoreObjectDimensionalMode dimensional_mode;
    CoreObjectPlane locked_plane;
    CoreObjectTransform transform;
    CoreObjectFlags flags;
} CoreObject;

void core_object_init(CoreObject *object);
CoreResult core_object_set_identity(CoreObject *object,
                                    const char *object_id,
                                    const char *object_type);
CoreResult core_object_set_plane_lock(CoreObject *object, CoreObjectPlane plane);
CoreResult core_object_promote_to_full_3d(CoreObject *object);
CoreResult core_object_enforce_dimensional_rules(CoreObject *object);
CoreResult core_object_validate(const CoreObject *object);

#ifdef __cplusplus
}
#endif

#endif
