#include "core_mesh_asset.h"

#include "core_io.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../shape/external/cjson/cJSON.h"

static CoreResult core_mesh_asset_doc_invalid_arg(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static CoreResult core_mesh_asset_doc_io_error(const char *message) {
    CoreResult r = { CORE_ERR_IO, message };
    return r;
}

static bool core_mesh_asset_doc_text_equals(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0;
}

static char *core_mesh_asset_doc_print_json(const cJSON *root) {
    size_t capacity = 65536u;
    const size_t max_capacity = 4u * 1024u * 1024u;
    if (!root) {
        return NULL;
    }
    while (capacity <= max_capacity) {
        char *text = (char *)malloc(capacity);
        if (!text) {
            return NULL;
        }
        if (cJSON_PrintPreallocated((cJSON *)root, text, (int)capacity, cJSON_True)) {
            return text;
        }
        free(text);
        capacity *= 2u;
    }
    return NULL;
}

static bool core_mesh_asset_doc_vec3_equalish(CoreObjectVec3 a, CoreObjectVec3 b) {
    const double eps = 1e-6;
    return fabs(a.x - b.x) <= eps && fabs(a.y - b.y) <= eps && fabs(a.z - b.z) <= eps;
}

static CoreResult core_mesh_asset_doc_validate_frame(const CoreMeshAssetFrame3 *frame) {
    CoreMeshAssetAuthoringContract contract;
    CoreResult r;
    core_mesh_asset_authoring_contract_init(&contract);
    r = core_mesh_asset_authoring_contract_set_asset_id(&contract, "frame_probe");
    if (r.code != CORE_OK) {
        return r;
    }
    contract.pivot = *frame;
    contract.source_mode = CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED;
    return core_mesh_asset_authoring_contract_validate(&contract);
}

static const char *core_mesh_asset_doc_dimensional_mode_name(CoreObjectDimensionalMode mode) {
    switch (mode) {
        case CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED:
            return "plane_locked";
        case CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D:
            return "full_3d";
        default:
            return "unknown";
    }
}

static CoreResult core_mesh_asset_doc_dimensional_mode_parse(const char *text,
                                                             CoreObjectDimensionalMode *out_mode) {
    if (out_mode) {
        *out_mode = 0;
    }
    if (!text || !out_mode) {
        return core_mesh_asset_doc_invalid_arg("invalid argument");
    }
    if (core_mesh_asset_doc_text_equals(text, "plane_locked")) {
        *out_mode = CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED;
        return core_result_ok();
    }
    if (core_mesh_asset_doc_text_equals(text, "full_3d")) {
        *out_mode = CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D;
        return core_result_ok();
    }
    return core_mesh_asset_doc_invalid_arg("unknown dimensional_mode");
}

static const char *core_mesh_asset_doc_plane_name(CoreObjectPlane plane) {
    switch (plane) {
        case CORE_OBJECT_PLANE_XY:
            return "xy";
        case CORE_OBJECT_PLANE_YZ:
            return "yz";
        case CORE_OBJECT_PLANE_XZ:
            return "xz";
        default:
            return "unknown";
    }
}

static CoreResult core_mesh_asset_doc_plane_parse(const char *text, CoreObjectPlane *out_plane) {
    if (out_plane) {
        *out_plane = 0;
    }
    if (!text || !out_plane) {
        return core_mesh_asset_doc_invalid_arg("invalid argument");
    }
    if (core_mesh_asset_doc_text_equals(text, "xy")) {
        *out_plane = CORE_OBJECT_PLANE_XY;
        return core_result_ok();
    }
    if (core_mesh_asset_doc_text_equals(text, "yz")) {
        *out_plane = CORE_OBJECT_PLANE_YZ;
        return core_result_ok();
    }
    if (core_mesh_asset_doc_text_equals(text, "xz")) {
        *out_plane = CORE_OBJECT_PLANE_XZ;
        return core_result_ok();
    }
    return core_mesh_asset_doc_invalid_arg("unknown locked_plane");
}

static CoreResult core_mesh_asset_doc_copy_text(char *dst, size_t dst_size, const char *src) {
    size_t len;
    if (!dst || dst_size == 0u || !src || src[0] == '\0') {
        return core_mesh_asset_doc_invalid_arg("text field must be non-empty");
    }
    len = strlen(src);
    if (len >= dst_size) {
        return core_mesh_asset_doc_invalid_arg("text field too long");
    }
    memcpy(dst, src, len + 1u);
    return core_result_ok();
}

static cJSON *core_mesh_asset_doc_vec3_to_json(CoreObjectVec3 v) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
        return NULL;
    }
    cJSON_AddNumberToObject(obj, "x", v.x);
    cJSON_AddNumberToObject(obj, "y", v.y);
    cJSON_AddNumberToObject(obj, "z", v.z);
    return obj;
}

static bool core_mesh_asset_doc_vec3_from_json(const cJSON *node, CoreObjectVec3 *out_vec) {
    const cJSON *x = NULL;
    const cJSON *y = NULL;
    const cJSON *z = NULL;
    if (!cJSON_IsObject(node) || !out_vec) {
        return false;
    }
    x = cJSON_GetObjectItemCaseSensitive(node, "x");
    y = cJSON_GetObjectItemCaseSensitive(node, "y");
    z = cJSON_GetObjectItemCaseSensitive(node, "z");
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y) || !cJSON_IsNumber(z)) {
        return false;
    }
    out_vec->x = x->valuedouble;
    out_vec->y = y->valuedouble;
    out_vec->z = z->valuedouble;
    return true;
}

static cJSON *core_mesh_asset_doc_frame_to_json(const CoreMeshAssetFrame3 *frame) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj || !frame) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddItemToObject(obj, "origin", core_mesh_asset_doc_vec3_to_json(frame->origin));
    cJSON_AddItemToObject(obj, "axis_u", core_mesh_asset_doc_vec3_to_json(frame->axis_u));
    cJSON_AddItemToObject(obj, "axis_v", core_mesh_asset_doc_vec3_to_json(frame->axis_v));
    cJSON_AddItemToObject(obj, "normal", core_mesh_asset_doc_vec3_to_json(frame->normal));
    return obj;
}

static bool core_mesh_asset_doc_frame_from_json(const cJSON *node, CoreMeshAssetFrame3 *out_frame) {
    if (!cJSON_IsObject(node) || !out_frame) {
        return false;
    }
    return core_mesh_asset_doc_vec3_from_json(cJSON_GetObjectItemCaseSensitive(node, "origin"),
                                              &out_frame->origin) &&
           core_mesh_asset_doc_vec3_from_json(cJSON_GetObjectItemCaseSensitive(node, "axis_u"),
                                              &out_frame->axis_u) &&
           core_mesh_asset_doc_vec3_from_json(cJSON_GetObjectItemCaseSensitive(node, "axis_v"),
                                              &out_frame->axis_v) &&
           core_mesh_asset_doc_vec3_from_json(cJSON_GetObjectItemCaseSensitive(node, "normal"),
                                              &out_frame->normal);
}

static cJSON *core_mesh_asset_doc_transform_to_json(const CoreObjectTransform *transform) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj || !transform) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddItemToObject(obj, "position", core_mesh_asset_doc_vec3_to_json(transform->position));
    cJSON_AddItemToObject(obj, "rotation_deg",
                          core_mesh_asset_doc_vec3_to_json(transform->rotation_deg));
    cJSON_AddItemToObject(obj, "scale", core_mesh_asset_doc_vec3_to_json(transform->scale));
    return obj;
}

static bool core_mesh_asset_doc_transform_from_json(const cJSON *node,
                                                    CoreObjectTransform *out_transform) {
    if (!cJSON_IsObject(node) || !out_transform) {
        return false;
    }
    return core_mesh_asset_doc_vec3_from_json(cJSON_GetObjectItemCaseSensitive(node, "position"),
                                              &out_transform->position) &&
           core_mesh_asset_doc_vec3_from_json(cJSON_GetObjectItemCaseSensitive(node, "rotation_deg"),
                                              &out_transform->rotation_deg) &&
           core_mesh_asset_doc_vec3_from_json(cJSON_GetObjectItemCaseSensitive(node, "scale"),
                                              &out_transform->scale);
}

static cJSON *core_mesh_asset_doc_object_to_json(const CoreObject *object) {
    cJSON *obj = cJSON_CreateObject();
    cJSON *flags = NULL;
    if (!obj || !object) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddStringToObject(obj, "object_id", object->object_id);
    cJSON_AddStringToObject(obj, "object_type", object->object_type);
    cJSON_AddStringToObject(obj, "dimensional_mode",
                            core_mesh_asset_doc_dimensional_mode_name(object->dimensional_mode));
    cJSON_AddStringToObject(obj, "locked_plane",
                            core_mesh_asset_doc_plane_name(object->locked_plane));
    cJSON_AddItemToObject(obj, "transform", core_mesh_asset_doc_transform_to_json(&object->transform));
    flags = cJSON_CreateObject();
    if (!flags) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddItemToObject(obj, "flags", flags);
    cJSON_AddBoolToObject(flags, "visible", object->flags.visible);
    cJSON_AddBoolToObject(flags, "locked", object->flags.locked);
    cJSON_AddBoolToObject(flags, "selectable", object->flags.selectable);
    return obj;
}

static CoreResult core_mesh_asset_doc_object_from_json(const cJSON *node, CoreObject *out_object) {
    const cJSON *object_id = NULL;
    const cJSON *object_type = NULL;
    const cJSON *dimensional_mode = NULL;
    const cJSON *locked_plane = NULL;
    const cJSON *flags = NULL;
    const cJSON *visible = NULL;
    const cJSON *locked = NULL;
    const cJSON *selectable = NULL;
    CoreResult r;
    if (!cJSON_IsObject(node) || !out_object) {
        return core_mesh_asset_doc_invalid_arg("object node is invalid");
    }

    core_object_init(out_object);
    object_id = cJSON_GetObjectItemCaseSensitive(node, "object_id");
    object_type = cJSON_GetObjectItemCaseSensitive(node, "object_type");
    dimensional_mode = cJSON_GetObjectItemCaseSensitive(node, "dimensional_mode");
    locked_plane = cJSON_GetObjectItemCaseSensitive(node, "locked_plane");
    flags = cJSON_GetObjectItemCaseSensitive(node, "flags");
    if (!cJSON_IsString(object_id) || !object_id->valuestring || !cJSON_IsString(object_type) ||
        !object_type->valuestring || !cJSON_IsString(dimensional_mode) ||
        !dimensional_mode->valuestring || !cJSON_IsString(locked_plane) ||
        !locked_plane->valuestring || !cJSON_IsObject(flags)) {
        return core_mesh_asset_doc_invalid_arg("object fields are missing");
    }

    r = core_object_set_identity(out_object, object_id->valuestring, object_type->valuestring);
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_mesh_asset_doc_dimensional_mode_parse(dimensional_mode->valuestring,
                                                   &out_object->dimensional_mode);
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_mesh_asset_doc_plane_parse(locked_plane->valuestring, &out_object->locked_plane);
    if (r.code != CORE_OK) {
        return r;
    }
    if (!core_mesh_asset_doc_transform_from_json(cJSON_GetObjectItemCaseSensitive(node, "transform"),
                                                 &out_object->transform)) {
        return core_mesh_asset_doc_invalid_arg("object transform is invalid");
    }
    visible = cJSON_GetObjectItemCaseSensitive(flags, "visible");
    locked = cJSON_GetObjectItemCaseSensitive(flags, "locked");
    selectable = cJSON_GetObjectItemCaseSensitive(flags, "selectable");
    if (!cJSON_IsBool(visible) || !cJSON_IsBool(locked) || !cJSON_IsBool(selectable)) {
        return core_mesh_asset_doc_invalid_arg("object flags are invalid");
    }
    out_object->flags.visible = cJSON_IsTrue(visible);
    out_object->flags.locked = cJSON_IsTrue(locked);
    out_object->flags.selectable = cJSON_IsTrue(selectable);
    return core_object_validate(out_object);
}

static cJSON *core_mesh_asset_doc_plane_seed_to_json(const CoreMeshAssetPlanePrimitiveSeed *plane) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj || !plane) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddNumberToObject(obj, "width", plane->width);
    cJSON_AddNumberToObject(obj, "height", plane->height);
    cJSON_AddItemToObject(obj, "frame", core_mesh_asset_doc_frame_to_json(&plane->frame));
    cJSON_AddBoolToObject(obj, "lock_to_construction_plane", plane->lock_to_construction_plane);
    cJSON_AddBoolToObject(obj, "lock_to_bounds", plane->lock_to_bounds);
    return obj;
}

static bool core_mesh_asset_doc_plane_seed_from_json(const cJSON *node,
                                                     CoreMeshAssetPlanePrimitiveSeed *out_plane) {
    const cJSON *width = NULL;
    const cJSON *height = NULL;
    const cJSON *lock_to_construction_plane = NULL;
    const cJSON *lock_to_bounds = NULL;
    if (!cJSON_IsObject(node) || !out_plane) {
        return false;
    }
    width = cJSON_GetObjectItemCaseSensitive(node, "width");
    height = cJSON_GetObjectItemCaseSensitive(node, "height");
    lock_to_construction_plane =
        cJSON_GetObjectItemCaseSensitive(node, "lock_to_construction_plane");
    lock_to_bounds = cJSON_GetObjectItemCaseSensitive(node, "lock_to_bounds");
    if (!cJSON_IsNumber(width) || !cJSON_IsNumber(height) ||
        !cJSON_IsBool(lock_to_construction_plane) || !cJSON_IsBool(lock_to_bounds)) {
        return false;
    }
    out_plane->width = width->valuedouble;
    out_plane->height = height->valuedouble;
    out_plane->lock_to_construction_plane = cJSON_IsTrue(lock_to_construction_plane);
    out_plane->lock_to_bounds = cJSON_IsTrue(lock_to_bounds);
    return core_mesh_asset_doc_frame_from_json(cJSON_GetObjectItemCaseSensitive(node, "frame"),
                                               &out_plane->frame);
}

static cJSON *core_mesh_asset_doc_rect_prism_seed_to_json(
    const CoreMeshAssetRectPrismPrimitiveSeed *rect_prism) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj || !rect_prism) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddNumberToObject(obj, "width", rect_prism->width);
    cJSON_AddNumberToObject(obj, "height", rect_prism->height);
    cJSON_AddNumberToObject(obj, "depth", rect_prism->depth);
    cJSON_AddItemToObject(obj, "frame", core_mesh_asset_doc_frame_to_json(&rect_prism->frame));
    cJSON_AddBoolToObject(obj,
                          "lock_to_construction_plane",
                          rect_prism->lock_to_construction_plane);
    cJSON_AddBoolToObject(obj, "lock_to_bounds", rect_prism->lock_to_bounds);
    return obj;
}

static bool core_mesh_asset_doc_rect_prism_seed_from_json(
    const cJSON *node,
    CoreMeshAssetRectPrismPrimitiveSeed *out_rect_prism) {
    const cJSON *width = NULL;
    const cJSON *height = NULL;
    const cJSON *depth = NULL;
    const cJSON *lock_to_construction_plane = NULL;
    const cJSON *lock_to_bounds = NULL;
    if (!cJSON_IsObject(node) || !out_rect_prism) {
        return false;
    }
    width = cJSON_GetObjectItemCaseSensitive(node, "width");
    height = cJSON_GetObjectItemCaseSensitive(node, "height");
    depth = cJSON_GetObjectItemCaseSensitive(node, "depth");
    lock_to_construction_plane =
        cJSON_GetObjectItemCaseSensitive(node, "lock_to_construction_plane");
    lock_to_bounds = cJSON_GetObjectItemCaseSensitive(node, "lock_to_bounds");
    if (!cJSON_IsNumber(width) || !cJSON_IsNumber(height) || !cJSON_IsNumber(depth) ||
        !cJSON_IsBool(lock_to_construction_plane) || !cJSON_IsBool(lock_to_bounds)) {
        return false;
    }
    out_rect_prism->width = width->valuedouble;
    out_rect_prism->height = height->valuedouble;
    out_rect_prism->depth = depth->valuedouble;
    out_rect_prism->lock_to_construction_plane = cJSON_IsTrue(lock_to_construction_plane);
    out_rect_prism->lock_to_bounds = cJSON_IsTrue(lock_to_bounds);
    return core_mesh_asset_doc_frame_from_json(cJSON_GetObjectItemCaseSensitive(node, "frame"),
                                               &out_rect_prism->frame);
}

static CoreResult core_mesh_asset_doc_validate_primitive_seed(
    const CoreMeshAssetPrimitiveSeed *seed) {
    CoreResult r;
    if (!seed) {
        return core_mesh_asset_doc_invalid_arg("primitive seed is null");
    }
    if (seed->primitive_id[0] == '\0') {
        return core_mesh_asset_doc_invalid_arg("primitive_id is required");
    }
    if (seed->kind != CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE &&
        seed->kind != CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_RECT_PRISM) {
        return core_mesh_asset_doc_invalid_arg("known primitive seed kind is required");
    }
    r = core_object_validate(&seed->object);
    if (r.code != CORE_OK) {
        return r;
    }
    if (!core_mesh_asset_doc_vec3_equalish(seed->object.transform.position,
                                           (seed->kind == CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE)
                                               ? seed->plane.frame.origin
                                               : seed->rect_prism.frame.origin)) {
        return core_mesh_asset_doc_invalid_arg("primitive frame origin must match object position");
    }
    if (seed->kind == CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE) {
        if (seed->plane.width <= 0.0 || seed->plane.height <= 0.0) {
            return core_mesh_asset_doc_invalid_arg("plane primitive dimensions must be positive");
        }
        r = core_mesh_asset_doc_validate_frame(&seed->plane.frame);
        if (r.code != CORE_OK) {
            return r;
        }
        if (seed->object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED &&
            seed->object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D) {
            return core_mesh_asset_doc_invalid_arg("plane primitive dimensional mode is invalid");
        }
    } else {
        if (seed->rect_prism.width <= 0.0 || seed->rect_prism.height <= 0.0 ||
            seed->rect_prism.depth < 0.0) {
            return core_mesh_asset_doc_invalid_arg("rect prism primitive dimensions are invalid");
        }
        r = core_mesh_asset_doc_validate_frame(&seed->rect_prism.frame);
        if (r.code != CORE_OK) {
            return r;
        }
    }
    return core_result_ok();
}

static cJSON *core_mesh_asset_doc_imported_mesh_source_to_json(
    const CoreMeshAssetImportedMeshSource *source) {
    cJSON *obj = cJSON_CreateObject();
    if (!obj || !source) {
        cJSON_Delete(obj);
        return NULL;
    }
    cJSON_AddStringToObject(obj, "import_id", source->import_id);
    cJSON_AddStringToObject(
        obj,
        "source_format",
        core_mesh_asset_imported_mesh_source_format_name(source->source_format));
    cJSON_AddStringToObject(obj, "source_uri", source->source_uri);
    cJSON_AddStringToObject(obj, "source_unit_system",
                            core_units_kind_name(source->source_unit_kind));
    cJSON_AddNumberToObject(obj, "source_to_asset_scale", source->source_to_asset_scale);
    cJSON_AddStringToObject(obj, "orientation_policy", source->orientation_policy);
    cJSON_AddStringToObject(obj,
                            "default_surface_group_id",
                            source->default_surface_group_id);
    cJSON_AddBoolToObject(obj, "weld_vertices", source->weld_vertices);
    cJSON_AddNumberToObject(obj, "weld_tolerance", source->weld_tolerance);
    cJSON_AddBoolToObject(obj, "preserve_source_normals", source->preserve_source_normals);
    cJSON_AddBoolToObject(obj,
                          "topology_closed_volume_observed",
                          source->topology_closed_volume_observed);
    cJSON_AddBoolToObject(obj,
                          "topology_manifold_observed",
                          source->topology_manifold_observed);
    return obj;
}

static CoreResult core_mesh_asset_doc_imported_mesh_source_from_json(
    const cJSON *node,
    CoreMeshAssetImportedMeshSource *out_source) {
    const cJSON *import_id = NULL;
    const cJSON *source_format = NULL;
    const cJSON *source_uri = NULL;
    const cJSON *source_unit_system = NULL;
    const cJSON *source_to_asset_scale = NULL;
    const cJSON *orientation_policy = NULL;
    const cJSON *default_surface_group_id = NULL;
    const cJSON *weld_vertices = NULL;
    const cJSON *weld_tolerance = NULL;
    const cJSON *preserve_source_normals = NULL;
    const cJSON *topology_closed_volume_observed = NULL;
    const cJSON *topology_manifold_observed = NULL;
    CoreResult r;
    if (!cJSON_IsObject(node) || !out_source) {
        return core_mesh_asset_doc_invalid_arg("imported_mesh source is invalid");
    }
    core_mesh_asset_imported_mesh_source_init(out_source);
    import_id = cJSON_GetObjectItemCaseSensitive(node, "import_id");
    source_format = cJSON_GetObjectItemCaseSensitive(node, "source_format");
    source_uri = cJSON_GetObjectItemCaseSensitive(node, "source_uri");
    source_unit_system = cJSON_GetObjectItemCaseSensitive(node, "source_unit_system");
    source_to_asset_scale = cJSON_GetObjectItemCaseSensitive(node, "source_to_asset_scale");
    orientation_policy = cJSON_GetObjectItemCaseSensitive(node, "orientation_policy");
    default_surface_group_id = cJSON_GetObjectItemCaseSensitive(node, "default_surface_group_id");
    weld_vertices = cJSON_GetObjectItemCaseSensitive(node, "weld_vertices");
    weld_tolerance = cJSON_GetObjectItemCaseSensitive(node, "weld_tolerance");
    preserve_source_normals = cJSON_GetObjectItemCaseSensitive(node, "preserve_source_normals");
    topology_closed_volume_observed =
        cJSON_GetObjectItemCaseSensitive(node, "topology_closed_volume_observed");
    topology_manifold_observed =
        cJSON_GetObjectItemCaseSensitive(node, "topology_manifold_observed");
    if (!cJSON_IsString(import_id) || !import_id->valuestring ||
        !cJSON_IsString(source_format) || !source_format->valuestring ||
        !cJSON_IsString(source_uri) || !source_uri->valuestring ||
        !cJSON_IsString(source_unit_system) || !source_unit_system->valuestring ||
        !cJSON_IsNumber(source_to_asset_scale) || !cJSON_IsString(orientation_policy) ||
        !orientation_policy->valuestring || !cJSON_IsString(default_surface_group_id) ||
        !default_surface_group_id->valuestring || !cJSON_IsBool(weld_vertices) ||
        !cJSON_IsNumber(weld_tolerance) || !cJSON_IsBool(preserve_source_normals) ||
        !cJSON_IsBool(topology_closed_volume_observed) ||
        !cJSON_IsBool(topology_manifold_observed)) {
        return core_mesh_asset_doc_invalid_arg("imported_mesh source fields are missing");
    }
    r = core_mesh_asset_doc_copy_text(out_source->import_id,
                                      sizeof(out_source->import_id),
                                      import_id->valuestring);
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_mesh_asset_imported_mesh_source_format_parse(source_format->valuestring,
                                                          &out_source->source_format);
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_mesh_asset_doc_copy_text(out_source->source_uri,
                                      sizeof(out_source->source_uri),
                                      source_uri->valuestring);
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_units_parse_kind(source_unit_system->valuestring, &out_source->source_unit_kind);
    if (r.code != CORE_OK) {
        return r;
    }
    out_source->source_to_asset_scale = source_to_asset_scale->valuedouble;
    r = core_mesh_asset_doc_copy_text(out_source->orientation_policy,
                                      sizeof(out_source->orientation_policy),
                                      orientation_policy->valuestring);
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_mesh_asset_doc_copy_text(out_source->default_surface_group_id,
                                      sizeof(out_source->default_surface_group_id),
                                      default_surface_group_id->valuestring);
    if (r.code != CORE_OK) {
        return r;
    }
    out_source->weld_vertices = cJSON_IsTrue(weld_vertices);
    out_source->weld_tolerance = weld_tolerance->valuedouble;
    out_source->preserve_source_normals = cJSON_IsTrue(preserve_source_normals);
    out_source->topology_closed_volume_observed = cJSON_IsTrue(topology_closed_volume_observed);
    out_source->topology_manifold_observed = cJSON_IsTrue(topology_manifold_observed);
    return core_mesh_asset_imported_mesh_source_validate(out_source);
}

static CoreResult core_mesh_asset_doc_parse_root_contract(const cJSON *root,
                                                          CoreMeshAssetAuthoringDocument *document) {
    const cJSON *schema_family = NULL;
    const cJSON *schema_variant = NULL;
    const cJSON *schema_version = NULL;
    const cJSON *asset_id = NULL;
    const cJSON *unit_system = NULL;
    const cJSON *world_scale = NULL;
    const cJSON *asset_type = NULL;
    const cJSON *compile_hints = NULL;
    CoreResult r;
    if (!cJSON_IsObject(root) || !document) {
        return core_mesh_asset_doc_invalid_arg("root document is invalid");
    }

    schema_family = cJSON_GetObjectItemCaseSensitive(root, "schema_family");
    schema_variant = cJSON_GetObjectItemCaseSensitive(root, "schema_variant");
    schema_version = cJSON_GetObjectItemCaseSensitive(root, "schema_version");
    asset_id = cJSON_GetObjectItemCaseSensitive(root, "asset_id");
    unit_system = cJSON_GetObjectItemCaseSensitive(root, "unit_system");
    world_scale = cJSON_GetObjectItemCaseSensitive(root, "world_scale");
    asset_type = cJSON_GetObjectItemCaseSensitive(root, "asset_type");
    compile_hints = cJSON_GetObjectItemCaseSensitive(root, "compile_hints");

    if (!cJSON_IsString(schema_family) || !schema_family->valuestring ||
        !core_mesh_asset_doc_text_equals(schema_family->valuestring, "codework_geometry")) {
        return core_mesh_asset_doc_invalid_arg("schema_family must be codework_geometry");
    }
    if (!cJSON_IsString(schema_variant) || !schema_variant->valuestring ||
        !core_mesh_asset_doc_text_equals(schema_variant->valuestring, "mesh_asset_authoring_v1")) {
        return core_mesh_asset_doc_invalid_arg("schema_variant must be mesh_asset_authoring_v1");
    }
    if (!cJSON_IsNumber(schema_version) ||
        schema_version->valueint != CORE_MESH_ASSET_SCHEMA_VERSION_1) {
        return core_mesh_asset_doc_invalid_arg("unsupported schema_version");
    }
    if (!cJSON_IsString(asset_id) || !asset_id->valuestring || !cJSON_IsString(unit_system) ||
        !unit_system->valuestring || !cJSON_IsNumber(world_scale) || !cJSON_IsString(asset_type) ||
        !asset_type->valuestring) {
        return core_mesh_asset_doc_invalid_arg("authoring root fields are missing");
    }
    r = core_mesh_asset_authoring_contract_set_asset_id(&document->contract, asset_id->valuestring);
    if (r.code != CORE_OK) {
        return r;
    }
    r = core_units_parse_kind(unit_system->valuestring, &document->contract.unit_kind);
    if (r.code != CORE_OK) {
        return r;
    }
    document->contract.world_scale = world_scale->valuedouble;
    r = core_mesh_asset_type_parse(asset_type->valuestring, &document->contract.asset_type);
    if (r.code != CORE_OK) {
        return r;
    }
    if (!core_mesh_asset_doc_frame_from_json(cJSON_GetObjectItemCaseSensitive(root, "pivot"),
                                             &document->contract.pivot)) {
        return core_mesh_asset_doc_invalid_arg("pivot is invalid");
    }
    if (cJSON_IsObject(compile_hints)) {
        const cJSON *expect_closed_volume =
            cJSON_GetObjectItemCaseSensitive(compile_hints, "expect_closed_volume");
        const cJSON *expect_manifold =
            cJSON_GetObjectItemCaseSensitive(compile_hints, "expect_manifold");
        if (cJSON_IsBool(expect_closed_volume)) {
            document->contract.topology_closed_volume_expected = cJSON_IsTrue(expect_closed_volume);
        }
        if (cJSON_IsBool(expect_manifold)) {
            document->contract.topology_manifold_expected = cJSON_IsTrue(expect_manifold);
        }
    }
    return core_result_ok();
}

void core_mesh_asset_authoring_document_init(CoreMeshAssetAuthoringDocument *document) {
    if (!document) {
        return;
    }
    memset(document, 0, sizeof(*document));
    core_mesh_asset_authoring_contract_init(&document->contract);
    core_mesh_asset_imported_mesh_source_init(&document->imported_mesh_source);
}

void core_mesh_asset_authoring_document_free(CoreMeshAssetAuthoringDocument *document) {
    if (!document) {
        return;
    }
    core_free(document->primitive_seeds);
    document->primitive_seeds = NULL;
    document->primitive_seed_count = 0u;
    document->has_imported_mesh_source = false;
    core_mesh_asset_imported_mesh_source_init(&document->imported_mesh_source);
    core_mesh_asset_authoring_contract_init(&document->contract);
}

CoreResult core_mesh_asset_authoring_document_set_primitive_seed_count(
    CoreMeshAssetAuthoringDocument *document,
    size_t primitive_seed_count) {
    void *buffer = NULL;
    if (!document) {
        return core_mesh_asset_doc_invalid_arg("document is null");
    }
    if (primitive_seed_count == 0u) {
        core_free(document->primitive_seeds);
        document->primitive_seeds = NULL;
        document->primitive_seed_count = 0u;
        return core_result_ok();
    }
    if (primitive_seed_count > (SIZE_MAX / sizeof(CoreMeshAssetPrimitiveSeed))) {
        return core_mesh_asset_doc_invalid_arg("primitive seed count is too large");
    }
    buffer = core_alloc(primitive_seed_count * sizeof(CoreMeshAssetPrimitiveSeed));
    if (!buffer) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }
    memset(buffer, 0, primitive_seed_count * sizeof(CoreMeshAssetPrimitiveSeed));
    core_free(document->primitive_seeds);
    document->primitive_seeds = (CoreMeshAssetPrimitiveSeed *)buffer;
    document->primitive_seed_count = primitive_seed_count;
    return core_result_ok();
}

CoreResult core_mesh_asset_authoring_document_validate(
    const CoreMeshAssetAuthoringDocument *document) {
    size_t i;
    CoreResult r;
    if (!document) {
        return core_mesh_asset_doc_invalid_arg("document is null");
    }
    r = core_mesh_asset_authoring_contract_validate(&document->contract);
    if (r.code != CORE_OK) {
        return r;
    }
    if (document->contract.source_mode == CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED) {
        if (document->primitive_seed_count == 0u || !document->primitive_seeds) {
            return core_mesh_asset_doc_invalid_arg(
                "primitive_seed source_mode requires primitive_seeds");
        }
        if (document->has_imported_mesh_source) {
            return core_mesh_asset_doc_invalid_arg(
                "imported_mesh source is only valid for imported_mesh source_mode");
        }
    } else if (document->contract.source_mode == CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH) {
        if (!document->has_imported_mesh_source) {
            return core_mesh_asset_doc_invalid_arg(
                "imported_mesh source_mode requires imported_mesh source");
        }
        r = core_mesh_asset_imported_mesh_source_validate(&document->imported_mesh_source);
        if (r.code != CORE_OK) {
            return r;
        }
        if (document->primitive_seed_count != 0u || document->primitive_seeds != NULL) {
            return core_mesh_asset_doc_invalid_arg(
                "primitive_seeds are only valid for primitive_seed source_mode");
        }
    } else if (document->primitive_seed_count != 0u || document->primitive_seeds != NULL) {
        return core_mesh_asset_doc_invalid_arg(
            "primitive_seeds are only valid for primitive_seed source_mode");
    } else if (document->has_imported_mesh_source) {
        return core_mesh_asset_doc_invalid_arg(
            "imported_mesh source is only valid for imported_mesh source_mode");
    }
    for (i = 0u; i < document->primitive_seed_count; ++i) {
        r = core_mesh_asset_doc_validate_primitive_seed(&document->primitive_seeds[i]);
        if (r.code != CORE_OK) {
            return r;
        }
    }
    return core_result_ok();
}

CoreResult core_mesh_asset_authoring_document_load_file(const char *path,
                                                        CoreMeshAssetAuthoringDocument *out_document) {
    CoreBuffer file_data = {0};
    char *json_text = NULL;
    cJSON *root = NULL;
    const cJSON *authoring = NULL;
    const cJSON *source_mode = NULL;
    const cJSON *primitive_seeds = NULL;
    const cJSON *imported_mesh = NULL;
    int primitive_seed_count = 0;
    int i;
    CoreResult r;
    CoreMeshAssetAuthoringDocument document;

    if (!path || !out_document) {
        return core_mesh_asset_doc_invalid_arg("invalid argument");
    }

    r = core_io_read_all(path, &file_data);
    if (r.code != CORE_OK) {
        return r;
    }
    json_text = (char *)core_alloc(file_data.size + 1u);
    if (!json_text) {
        core_io_buffer_free(&file_data);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    if (file_data.size > 0u && file_data.data) {
        memcpy(json_text, file_data.data, file_data.size);
    }
    json_text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    root = cJSON_Parse(json_text);
    core_free(json_text);
    if (!root) {
        return core_mesh_asset_doc_invalid_arg("mesh asset authoring JSON parse failed");
    }

    core_mesh_asset_authoring_document_init(&document);
    r = core_mesh_asset_doc_parse_root_contract(root, &document);
    if (r.code != CORE_OK) {
        cJSON_Delete(root);
        core_mesh_asset_authoring_document_free(&document);
        return r;
    }

    authoring = cJSON_GetObjectItemCaseSensitive(root, "authoring");
    source_mode = authoring ? cJSON_GetObjectItemCaseSensitive(authoring, "source_mode") : NULL;
    if (!cJSON_IsObject(authoring) || !cJSON_IsString(source_mode) || !source_mode->valuestring) {
        cJSON_Delete(root);
        core_mesh_asset_authoring_document_free(&document);
        return core_mesh_asset_doc_invalid_arg("authoring.source_mode is required");
    }
    r = core_mesh_asset_source_mode_parse(source_mode->valuestring, &document.contract.source_mode);
    if (r.code != CORE_OK) {
        cJSON_Delete(root);
        core_mesh_asset_authoring_document_free(&document);
        return r;
    }

    primitive_seeds = cJSON_GetObjectItemCaseSensitive(authoring, "primitive_seeds");
    imported_mesh = cJSON_GetObjectItemCaseSensitive(authoring, "imported_mesh");
    if (document.contract.source_mode == CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED) {
        if (!cJSON_IsArray(primitive_seeds)) {
            cJSON_Delete(root);
            core_mesh_asset_authoring_document_free(&document);
            return core_mesh_asset_doc_invalid_arg("primitive_seed authoring requires primitive_seeds");
        }
        primitive_seed_count = cJSON_GetArraySize(primitive_seeds);
        if (primitive_seed_count <= 0) {
            cJSON_Delete(root);
            core_mesh_asset_authoring_document_free(&document);
            return core_mesh_asset_doc_invalid_arg("primitive_seeds must be non-empty");
        }
        r = core_mesh_asset_authoring_document_set_primitive_seed_count(&document,
                                                                        (size_t)primitive_seed_count);
        if (r.code != CORE_OK) {
            cJSON_Delete(root);
            core_mesh_asset_authoring_document_free(&document);
            return r;
        }
        for (i = 0; i < primitive_seed_count; ++i) {
            CoreMeshAssetPrimitiveSeed *seed = &document.primitive_seeds[i];
            const cJSON *seed_node = cJSON_GetArrayItem(primitive_seeds, i);
            const cJSON *primitive_id =
                seed_node ? cJSON_GetObjectItemCaseSensitive(seed_node, "primitive_id") : NULL;
            const cJSON *kind = seed_node ? cJSON_GetObjectItemCaseSensitive(seed_node, "kind") : NULL;
            if (!cJSON_IsObject(seed_node) || !cJSON_IsString(primitive_id) ||
                !primitive_id->valuestring || !cJSON_IsString(kind) || !kind->valuestring) {
                cJSON_Delete(root);
                core_mesh_asset_authoring_document_free(&document);
                return core_mesh_asset_doc_invalid_arg("primitive seed fields are missing");
            }
            r = core_mesh_asset_doc_copy_text(seed->primitive_id,
                                              sizeof(seed->primitive_id),
                                              primitive_id->valuestring);
            if (r.code != CORE_OK) {
                cJSON_Delete(root);
                core_mesh_asset_authoring_document_free(&document);
                return r;
            }
            r = core_mesh_asset_primitive_seed_kind_parse(kind->valuestring, &seed->kind);
            if (r.code != CORE_OK) {
                cJSON_Delete(root);
                core_mesh_asset_authoring_document_free(&document);
                return r;
            }
            r = core_mesh_asset_doc_object_from_json(
                cJSON_GetObjectItemCaseSensitive(seed_node, "object"),
                &seed->object);
            if (r.code != CORE_OK) {
                cJSON_Delete(root);
                core_mesh_asset_authoring_document_free(&document);
                return r;
            }
            if (seed->kind == CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE) {
                if (!core_mesh_asset_doc_plane_seed_from_json(
                        cJSON_GetObjectItemCaseSensitive(seed_node, "plane"),
                        &seed->plane)) {
                    cJSON_Delete(root);
                    core_mesh_asset_authoring_document_free(&document);
                    return core_mesh_asset_doc_invalid_arg("plane primitive seed is invalid");
                }
            } else if (!core_mesh_asset_doc_rect_prism_seed_from_json(
                           cJSON_GetObjectItemCaseSensitive(seed_node, "rect_prism"),
                           &seed->rect_prism)) {
                cJSON_Delete(root);
                core_mesh_asset_authoring_document_free(&document);
                return core_mesh_asset_doc_invalid_arg("rect prism primitive seed is invalid");
            }
        }
    } else if (document.contract.source_mode == CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH) {
        r = core_mesh_asset_doc_imported_mesh_source_from_json(imported_mesh,
                                                               &document.imported_mesh_source);
        if (r.code != CORE_OK) {
            cJSON_Delete(root);
            core_mesh_asset_authoring_document_free(&document);
            return r;
        }
        document.has_imported_mesh_source = true;
    }

    cJSON_Delete(root);
    r = core_mesh_asset_authoring_document_validate(&document);
    if (r.code != CORE_OK) {
        core_mesh_asset_authoring_document_free(&document);
        return r;
    }
    *out_document = document;
    return core_result_ok();
}

CoreResult core_mesh_asset_authoring_document_save_file(
    const CoreMeshAssetAuthoringDocument *document,
    const char *path) {
    cJSON *root = NULL;
    cJSON *authoring = NULL;
    cJSON *compile_hints = NULL;
    cJSON *primitive_seeds = NULL;
    char *json_text = NULL;
    size_t i;
    CoreResult r;

    if (!document || !path) {
        return core_mesh_asset_doc_invalid_arg("invalid argument");
    }
    r = core_mesh_asset_authoring_document_validate(document);
    if (r.code != CORE_OK) {
        return r;
    }

    root = cJSON_CreateObject();
    if (!root) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    cJSON_AddStringToObject(root, "schema_family", "codework_geometry");
    cJSON_AddStringToObject(root, "schema_variant", "mesh_asset_authoring_v1");
    cJSON_AddNumberToObject(root, "schema_version", CORE_MESH_ASSET_SCHEMA_VERSION_1);
    cJSON_AddStringToObject(root, "asset_id", document->contract.asset_id);
    cJSON_AddStringToObject(root, "unit_system", core_units_kind_name(document->contract.unit_kind));
    cJSON_AddNumberToObject(root, "world_scale", document->contract.world_scale);
    cJSON_AddStringToObject(root, "asset_type", core_mesh_asset_type_name(document->contract.asset_type));
    cJSON_AddItemToObject(root, "pivot", core_mesh_asset_doc_frame_to_json(&document->contract.pivot));

    authoring = cJSON_CreateObject();
    if (!authoring) {
        cJSON_Delete(root);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    cJSON_AddItemToObject(root, "authoring", authoring);
    cJSON_AddStringToObject(authoring,
                            "source_mode",
                            core_mesh_asset_source_mode_name(document->contract.source_mode));
    if (document->contract.source_mode == CORE_MESH_ASSET_SOURCE_MODE_PRIMITIVE_SEED) {
        primitive_seeds = cJSON_CreateArray();
        if (!primitive_seeds) {
            cJSON_Delete(root);
            return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        }
        cJSON_AddItemToObject(authoring, "primitive_seeds", primitive_seeds);
        for (i = 0u; i < document->primitive_seed_count; ++i) {
            const CoreMeshAssetPrimitiveSeed *seed = &document->primitive_seeds[i];
            cJSON *seed_node = cJSON_CreateObject();
            if (!seed_node) {
                cJSON_Delete(root);
                return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            }
            cJSON_AddItemToArray(primitive_seeds, seed_node);
            cJSON_AddStringToObject(seed_node, "primitive_id", seed->primitive_id);
            cJSON_AddStringToObject(seed_node,
                                    "kind",
                                    core_mesh_asset_primitive_seed_kind_name(seed->kind));
            cJSON_AddItemToObject(seed_node, "object", core_mesh_asset_doc_object_to_json(&seed->object));
            if (seed->kind == CORE_MESH_ASSET_PRIMITIVE_SEED_KIND_PLANE) {
                cJSON_AddItemToObject(seed_node, "plane", core_mesh_asset_doc_plane_seed_to_json(&seed->plane));
            } else {
                cJSON_AddItemToObject(seed_node,
                                      "rect_prism",
                                      core_mesh_asset_doc_rect_prism_seed_to_json(&seed->rect_prism));
            }
        }
    } else if (document->contract.source_mode == CORE_MESH_ASSET_SOURCE_MODE_IMPORTED_MESH) {
        cJSON_AddItemToObject(
            authoring,
            "imported_mesh",
            core_mesh_asset_doc_imported_mesh_source_to_json(&document->imported_mesh_source));
    }
    cJSON_AddItemToObject(root, "surface_groups", cJSON_CreateArray());
    compile_hints = cJSON_CreateObject();
    if (!compile_hints) {
        cJSON_Delete(root);
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    cJSON_AddItemToObject(root, "compile_hints", compile_hints);
    cJSON_AddBoolToObject(compile_hints,
                          "expect_closed_volume",
                          document->contract.topology_closed_volume_expected);
    cJSON_AddBoolToObject(compile_hints,
                          "expect_manifold",
                          document->contract.topology_manifold_expected);
    cJSON_AddItemToObject(root, "extensions", cJSON_CreateObject());

    json_text = core_mesh_asset_doc_print_json(root);
    cJSON_Delete(root);
    if (!json_text) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    r = core_io_write_all_atomic(path, json_text, strlen(json_text));
    free(json_text);
    if (r.code != CORE_OK) {
        return core_mesh_asset_doc_io_error("failed to save mesh asset authoring file");
    }
    return core_result_ok();
}
