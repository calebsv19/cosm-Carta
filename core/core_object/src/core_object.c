/*
 * core_object.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_object.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int core_object_is_finite(double v) {
    return isfinite(v) ? 1 : 0;
}

static CoreResult core_object_validate_transform(const CoreObjectTransform *transform) {
    if (!transform) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "transform is null" };
        return r;
    }

    if (!core_object_is_finite(transform->position.x) ||
        !core_object_is_finite(transform->position.y) ||
        !core_object_is_finite(transform->position.z) ||
        !core_object_is_finite(transform->rotation_deg.x) ||
        !core_object_is_finite(transform->rotation_deg.y) ||
        !core_object_is_finite(transform->rotation_deg.z) ||
        !core_object_is_finite(transform->scale.x) ||
        !core_object_is_finite(transform->scale.y) ||
        !core_object_is_finite(transform->scale.z)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "transform contains non-finite values" };
        return r;
    }

    if (transform->scale.x <= 0.0 || transform->scale.y <= 0.0 || transform->scale.z <= 0.0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "scale values must be > 0" };
        return r;
    }

    return core_result_ok();
}

void core_object_init(CoreObject *object) {
    if (!object) {
        return;
    }

    memset(object, 0, sizeof(*object));
    object->dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    object->locked_plane = CORE_OBJECT_PLANE_XY;
    object->transform.scale.x = 1.0;
    object->transform.scale.y = 1.0;
    object->transform.scale.z = 1.0;
    object->flags.visible = true;
    object->flags.selectable = true;
}

CoreResult core_object_set_identity(CoreObject *object,
                                    const char *object_id,
                                    const char *object_type) {
    if (!object || !object_id || !object_type) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if (!object_id[0] || !object_type[0]) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "identity fields must be non-empty" };
        return r;
    }
    if (strlen(object_id) >= sizeof(object->object_id) ||
        strlen(object_type) >= sizeof(object->object_type)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "identity field too long" };
        return r;
    }

    strncpy(object->object_id, object_id, sizeof(object->object_id) - 1);
    object->object_id[sizeof(object->object_id) - 1] = '\0';
    strncpy(object->object_type, object_type, sizeof(object->object_type) - 1);
    object->object_type[sizeof(object->object_type) - 1] = '\0';
    return core_result_ok();
}

CoreResult core_object_set_plane_lock(CoreObject *object, CoreObjectPlane plane) {
    if (!object) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "object is null" };
        return r;
    }
    if (plane != CORE_OBJECT_PLANE_XY &&
        plane != CORE_OBJECT_PLANE_YZ &&
        plane != CORE_OBJECT_PLANE_XZ) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "unknown plane" };
        return r;
    }

    object->dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
    object->locked_plane = plane;
    return core_object_enforce_dimensional_rules(object);
}

CoreResult core_object_promote_to_full_3d(CoreObject *object) {
    if (!object) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "object is null" };
        return r;
    }
    object->dimensional_mode = CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
    return core_result_ok();
}

CoreResult core_object_enforce_dimensional_rules(CoreObject *object) {
    if (!object) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "object is null" };
        return r;
    }

    if (object->dimensional_mode == CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D) {
        return core_result_ok();
    }

    switch (object->locked_plane) {
        case CORE_OBJECT_PLANE_XY:
            object->transform.position.z = 0.0;
            break;
        case CORE_OBJECT_PLANE_YZ:
            object->transform.position.x = 0.0;
            break;
        case CORE_OBJECT_PLANE_XZ:
            object->transform.position.y = 0.0;
            break;
        default: {
            CoreResult r = { CORE_ERR_INVALID_ARG, "unknown plane lock" };
            return r;
        }
    }

    return core_result_ok();
}

CoreResult core_object_validate(const CoreObject *object) {
    if (!object) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "object is null" };
        return r;
    }

    if (!object->object_id[0] || !object->object_type[0]) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "identity fields must be set" };
        return r;
    }

    if (object->dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED &&
        object->dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid dimensional mode" };
        return r;
    }

    {
        CoreResult transform_r = core_object_validate_transform(&object->transform);
        if (transform_r.code != CORE_OK) {
            return transform_r;
        }
    }

    if (object->dimensional_mode == CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED) {
        switch (object->locked_plane) {
            case CORE_OBJECT_PLANE_XY:
            case CORE_OBJECT_PLANE_YZ:
            case CORE_OBJECT_PLANE_XZ:
                break;
            default: {
                CoreResult r = { CORE_ERR_INVALID_ARG, "invalid plane lock" };
                return r;
            }
        }
    }

    return core_result_ok();
}
