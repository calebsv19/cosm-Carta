/*
 * core_scene.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_scene.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int core_scene_is_finite(double v) {
    return isfinite(v) ? 1 : 0;
}

static bool path_has_extension(const char *path, const char *ext) {
    if (!path || !ext) return false;
    size_t path_len = strlen(path);
    size_t ext_len = strlen(ext);
    if (path_len < ext_len) return false;
    return strcmp(path + path_len - ext_len, ext) == 0;
}

static CoreResult core_scene_validate_vec3(const CoreObjectVec3 *vec, const char *label) {
    if (!vec) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "vec3 is null" };
        return r;
    }
    if (!core_scene_is_finite(vec->x) ||
        !core_scene_is_finite(vec->y) ||
        !core_scene_is_finite(vec->z)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, label ? label : "vec3 contains non-finite values" };
        return r;
    }
    return core_result_ok();
}

static double core_scene_vec3_length(const CoreObjectVec3 *vec) {
    return sqrt((vec->x * vec->x) + (vec->y * vec->y) + (vec->z * vec->z));
}

static double core_scene_vec3_dot(const CoreObjectVec3 *a, const CoreObjectVec3 *b) {
    return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

static CoreObjectVec3 core_scene_vec3_cross(const CoreObjectVec3 *a, const CoreObjectVec3 *b) {
    CoreObjectVec3 out;
    out.x = (a->y * b->z) - (a->z * b->y);
    out.y = (a->z * b->x) - (a->x * b->z);
    out.z = (a->x * b->y) - (a->y * b->x);
    return out;
}

static CoreResult core_scene_frame_validate(const CoreSceneFrame3 *frame) {
    CoreObjectVec3 cross;
    double axis_u_len;
    double axis_v_len;
    double normal_len;
    double cross_len;
    if (!frame) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "frame is null" };
        return r;
    }
    {
        CoreResult r = core_scene_validate_vec3(&frame->origin, "frame origin contains non-finite values");
        if (r.code != CORE_OK) return r;
        r = core_scene_validate_vec3(&frame->axis_u, "frame axis_u contains non-finite values");
        if (r.code != CORE_OK) return r;
        r = core_scene_validate_vec3(&frame->axis_v, "frame axis_v contains non-finite values");
        if (r.code != CORE_OK) return r;
        r = core_scene_validate_vec3(&frame->normal, "frame normal contains non-finite values");
        if (r.code != CORE_OK) return r;
    }

    axis_u_len = core_scene_vec3_length(&frame->axis_u);
    axis_v_len = core_scene_vec3_length(&frame->axis_v);
    normal_len = core_scene_vec3_length(&frame->normal);
    if (axis_u_len <= 0.0 || axis_v_len <= 0.0 || normal_len <= 0.0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "frame axes must be non-zero" };
        return r;
    }

    if (fabs(core_scene_vec3_dot(&frame->axis_u, &frame->axis_v)) > 1e-4 ||
        fabs(core_scene_vec3_dot(&frame->axis_u, &frame->normal)) > 1e-4 ||
        fabs(core_scene_vec3_dot(&frame->axis_v, &frame->normal)) > 1e-4) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "frame axes must be orthogonal" };
        return r;
    }

    cross = core_scene_vec3_cross(&frame->axis_u, &frame->axis_v);
    cross_len = core_scene_vec3_length(&cross);
    if (cross_len <= 0.0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "frame axis_u and axis_v cannot be collinear" };
        return r;
    }
    if (fabs(cross_len - normal_len) > 1e-4) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "frame normal magnitude must match axis_u x axis_v" };
        return r;
    }
    if (fabs(cross.x - frame->normal.x) > 1e-4 ||
        fabs(cross.y - frame->normal.y) > 1e-4 ||
        fabs(cross.z - frame->normal.z) > 1e-4) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "frame normal must match axis_u x axis_v" };
        return r;
    }

    return core_result_ok();
}

const char *core_scene_space_mode_name(CoreSceneSpaceMode mode) {
    switch (mode) {
        case CORE_SCENE_SPACE_MODE_2D: return "2d";
        case CORE_SCENE_SPACE_MODE_3D: return "3d";
        case CORE_SCENE_SPACE_MODE_UNKNOWN:
        default: return "unknown";
    }
}

CoreResult core_scene_space_mode_parse(const char *text, CoreSceneSpaceMode *out_mode) {
    if (!text || !out_mode) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if (strcmp(text, "2d") == 0) {
        *out_mode = CORE_SCENE_SPACE_MODE_2D;
        return core_result_ok();
    }
    if (strcmp(text, "3d") == 0) {
        *out_mode = CORE_SCENE_SPACE_MODE_3D;
        return core_result_ok();
    }

    {
        CoreResult r = { CORE_ERR_NOT_FOUND, "unknown scene space mode" };
        return r;
    }
}

const char *core_scene_object_kind_name(CoreSceneObjectKind kind) {
    switch (kind) {
        case CORE_SCENE_OBJECT_KIND_CURVE_PATH: return "curve_path";
        case CORE_SCENE_OBJECT_KIND_POINT_SET: return "point_set";
        case CORE_SCENE_OBJECT_KIND_EDGE_SET: return "edge_set";
        case CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE: return "plane_primitive";
        case CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE: return "rect_prism_primitive";
        case CORE_SCENE_OBJECT_KIND_UNKNOWN:
        default: return "unknown";
    }
}

CoreResult core_scene_object_kind_parse(const char *text, CoreSceneObjectKind *out_kind) {
    if (!text || !out_kind) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (strcmp(text, "curve_path") == 0) {
        *out_kind = CORE_SCENE_OBJECT_KIND_CURVE_PATH;
        return core_result_ok();
    }
    if (strcmp(text, "point_set") == 0) {
        *out_kind = CORE_SCENE_OBJECT_KIND_POINT_SET;
        return core_result_ok();
    }
    if (strcmp(text, "edge_set") == 0) {
        *out_kind = CORE_SCENE_OBJECT_KIND_EDGE_SET;
        return core_result_ok();
    }
    if (strcmp(text, "plane_primitive") == 0) {
        *out_kind = CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE;
        return core_result_ok();
    }
    if (strcmp(text, "rect_prism_primitive") == 0) {
        *out_kind = CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE;
        return core_result_ok();
    }

    {
        CoreResult r = { CORE_ERR_NOT_FOUND, "unknown scene object kind" };
        return r;
    }
}

void core_scene_root_contract_init(CoreSceneRootContract *contract) {
    if (!contract) return;
    memset(contract, 0, sizeof(*contract));
    contract->space_mode_intent = CORE_SCENE_SPACE_MODE_2D;
    contract->space_mode_default = CORE_SCENE_SPACE_MODE_2D;
    contract->unit_kind = CORE_UNIT_METER;
    contract->world_scale = 1.0;
}

CoreResult core_scene_root_contract_set_scene_id(CoreSceneRootContract *contract, const char *scene_id) {
    if (!contract || !scene_id) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if (!scene_id[0]) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "scene_id must be non-empty" };
        return r;
    }
    if (strlen(scene_id) >= sizeof(contract->scene_id)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "scene_id too long" };
        return r;
    }
    strncpy(contract->scene_id, scene_id, sizeof(contract->scene_id) - 1);
    contract->scene_id[sizeof(contract->scene_id) - 1] = '\0';
    return core_result_ok();
}

CoreResult core_scene_root_contract_validate(const CoreSceneRootContract *contract) {
    if (!contract) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "contract is null" };
        return r;
    }
    if (!contract->scene_id[0]) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "scene_id must be set" };
        return r;
    }
    if ((contract->space_mode_intent != CORE_SCENE_SPACE_MODE_2D &&
         contract->space_mode_intent != CORE_SCENE_SPACE_MODE_3D) ||
        (contract->space_mode_default != CORE_SCENE_SPACE_MODE_2D &&
         contract->space_mode_default != CORE_SCENE_SPACE_MODE_3D)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid scene space mode" };
        return r;
    }
    if (contract->unit_kind == CORE_UNIT_UNKNOWN) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "unit_kind must be set" };
        return r;
    }
    return core_units_validate_world_scale(contract->world_scale);
}

void core_scene_plane_primitive_init(CoreScenePlanePrimitive *primitive) {
    if (!primitive) return;
    memset(primitive, 0, sizeof(*primitive));
    primitive->width = 1.0;
    primitive->height = 1.0;
    primitive->frame.axis_u.x = 1.0;
    primitive->frame.axis_v.y = 1.0;
    primitive->frame.normal.z = 1.0;
}

CoreResult core_scene_plane_primitive_validate(const CoreScenePlanePrimitive *primitive) {
    if (!primitive) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "primitive is null" };
        return r;
    }
    if (!core_scene_is_finite(primitive->width) ||
        !core_scene_is_finite(primitive->height) ||
        primitive->width <= 0.0 ||
        primitive->height <= 0.0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "plane primitive dimensions must be > 0" };
        return r;
    }
    return core_scene_frame_validate(&primitive->frame);
}

void core_scene_rect_prism_primitive_init(CoreSceneRectPrismPrimitive *primitive) {
    if (!primitive) return;
    memset(primitive, 0, sizeof(*primitive));
    primitive->width = 1.0;
    primitive->height = 1.0;
    primitive->depth = 1.0;
    primitive->frame.axis_u.x = 1.0;
    primitive->frame.axis_v.y = 1.0;
    primitive->frame.normal.z = 1.0;
}

CoreResult core_scene_rect_prism_primitive_validate(const CoreSceneRectPrismPrimitive *primitive) {
    if (!primitive) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "primitive is null" };
        return r;
    }
    if (!core_scene_is_finite(primitive->width) ||
        !core_scene_is_finite(primitive->height) ||
        !core_scene_is_finite(primitive->depth) ||
        primitive->width <= 0.0 ||
        primitive->height <= 0.0 ||
        primitive->depth <= 0.0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "rect prism dimensions must be > 0" };
        return r;
    }
    return core_scene_frame_validate(&primitive->frame);
}

void core_scene_object_contract_init(CoreSceneObjectContract *contract) {
    if (!contract) return;
    memset(contract, 0, sizeof(*contract));
    core_object_init(&contract->object);
    core_scene_plane_primitive_init(&contract->plane_primitive);
    core_scene_rect_prism_primitive_init(&contract->rect_prism_primitive);
}

CoreResult core_scene_object_contract_prepare(CoreSceneObjectContract *contract,
                                              const char *object_id,
                                              CoreSceneObjectKind kind) {
    const char *object_type;
    if (!contract || !object_id) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    object_type = core_scene_object_kind_name(kind);
    if (kind == CORE_SCENE_OBJECT_KIND_UNKNOWN || strcmp(object_type, "unknown") == 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "unknown scene object kind" };
        return r;
    }

    core_scene_object_contract_init(contract);
    contract->kind = kind;

    {
        CoreResult r = core_object_set_identity(&contract->object, object_id, object_type);
        if (r.code != CORE_OK) return r;
    }

    if (kind == CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE) {
        return core_object_promote_to_full_3d(&contract->object);
    }

    return core_object_set_plane_lock(&contract->object, CORE_OBJECT_PLANE_XY);
}

CoreResult core_scene_object_contract_validate(const CoreSceneObjectContract *contract) {
    const char *kind_name;
    if (!contract) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "contract is null" };
        return r;
    }
    if (contract->kind == CORE_SCENE_OBJECT_KIND_UNKNOWN) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "object kind must be set" };
        return r;
    }
    kind_name = core_scene_object_kind_name(contract->kind);
    if (strcmp(kind_name, contract->object.object_type) != 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "object_type must match scene object kind" };
        return r;
    }

    {
        CoreResult r = core_object_validate(&contract->object);
        if (r.code != CORE_OK) return r;
    }

    if (contract->kind == CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE) {
        if (!contract->has_plane_primitive || contract->has_rect_prism_primitive) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "plane primitive payload mismatch" };
            return r;
        }
        if (contract->object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_PLANE_LOCKED) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "plane primitive must be plane-locked" };
            return r;
        }
        return core_scene_plane_primitive_validate(&contract->plane_primitive);
    }

    if (contract->kind == CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE) {
        if (!contract->has_rect_prism_primitive || contract->has_plane_primitive) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "rect prism payload mismatch" };
            return r;
        }
        if (contract->object.dimensional_mode != CORE_OBJECT_DIMENSIONAL_MODE_FULL_3D) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "rect prism must be full_3d" };
            return r;
        }
        return core_scene_rect_prism_primitive_validate(&contract->rect_prism_primitive);
    }

    if (contract->has_plane_primitive || contract->has_rect_prism_primitive) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "non-primitive object cannot carry primitive payload" };
        return r;
    }

    return core_result_ok();
}

static bool path_is_absolute(const char *path) {
    if (!path || !path[0]) return false;
    return path[0] == '/';
}

CoreResult core_scene_dirname(const char *path, char *out_dir, size_t out_dir_size) {
    if (!path || !out_dir || out_dir_size == 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    const char *slash = strrchr(path, '/');
    if (!slash) {
        if (out_dir_size < 2) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "buffer too small" };
            return r;
        }
        out_dir[0] = '.';
        out_dir[1] = '\0';
        return core_result_ok();
    }

    size_t len = (size_t)(slash - path);
    if (len + 1 > out_dir_size) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "buffer too small" };
        return r;
    }
    memcpy(out_dir, path, len);
    out_dir[len] = '\0';
    return core_result_ok();
}

CoreResult core_scene_resolve_path(const char *base_dir, const char *input_path, char *out_path, size_t out_path_size) {
    if (!base_dir || !input_path || !out_path || out_path_size == 0) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    if (path_is_absolute(input_path)) {
        size_t len = strlen(input_path);
        if (len + 1 > out_path_size) {
            CoreResult r = { CORE_ERR_INVALID_ARG, "buffer too small" };
            return r;
        }
        memcpy(out_path, input_path, len + 1);
        return core_result_ok();
    }

    size_t base_len = strlen(base_dir);
    size_t input_len = strlen(input_path);
    bool needs_slash = base_len > 0 && base_dir[base_len - 1] != '/';
    size_t total = base_len + (needs_slash ? 1u : 0u) + input_len + 1u;
    if (total > out_path_size) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "buffer too small" };
        return r;
    }

    memcpy(out_path, base_dir, base_len);
    if (needs_slash) out_path[base_len] = '/';
    memcpy(out_path + base_len + (needs_slash ? 1u : 0u), input_path, input_len + 1u);
    return core_result_ok();
}

CoreSceneSourceType core_scene_detect_source_type(const char *path) {
    if (!path) return CORE_SCENE_SOURCE_UNKNOWN;
    if (path_has_extension(path, ".pack")) return CORE_SCENE_SOURCE_PACK;
    if (path_has_extension(path, ".vf2d")) return CORE_SCENE_SOURCE_VF2D;
    if (path_has_extension(path, "manifest.json")) return CORE_SCENE_SOURCE_MANIFEST;
    return CORE_SCENE_SOURCE_UNKNOWN;
}

bool core_scene_path_is_scene_bundle(const char *path) {
    if (!path) return false;
    return path_has_extension(path, "scene_bundle.json") || path_has_extension(path, ".scene.json");
}

static bool read_text_file(const char *path, char *out, size_t out_size) {
    if (!path || !out || out_size < 2) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(out, 1, out_size - 1, f);
    fclose(f);
    out[n] = '\0';
    return n > 0;
}

static const char *skip_ws(const char *s) {
    while (s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')) s++;
    return s;
}

static bool json_extract_string_limited(const char *start,
                                        const char *limit,
                                        const char *key,
                                        char *out,
                                        size_t out_size) {
    if (!start || !key || !out || out_size == 0) return false;
    out[0] = '\0';

    char token[128];
    int n = snprintf(token, sizeof(token), "\"%s\"", key);
    if (n <= 0 || (size_t)n >= sizeof(token)) return false;

    const char *p = start;
    while ((p = strstr(p, token)) != NULL) {
        if (limit && p >= limit) return false;
        p += (size_t)n;
        p = skip_ws(p);
        if (!p || *p != ':') continue;
        p++;
        p = skip_ws(p);
        if (!p || *p != '"') continue;
        p++;
        size_t oi = 0;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) {
                p++;
            }
            if (oi + 1 < out_size) out[oi++] = *p;
            p++;
        }
        out[oi] = '\0';
        return oi > 0;
    }
    return false;
}

static bool json_extract_int_limited(const char *start,
                                     const char *limit,
                                     const char *key,
                                     int *out_value) {
    if (!start || !key || !out_value) return false;
    char token[128];
    int n = snprintf(token, sizeof(token), "\"%s\"", key);
    if (n <= 0 || (size_t)n >= sizeof(token)) return false;
    const char *p = start;
    while ((p = strstr(p, token)) != NULL) {
        if (limit && p >= limit) return false;
        p += (size_t)n;
        p = skip_ws(p);
        if (!p || *p != ':') continue;
        p++;
        p = skip_ws(p);
        if (!p) return false;
        int sign = 1;
        if (*p == '-') {
            sign = -1;
            p++;
        }
        if (*p < '0' || *p > '9') continue;
        int v = 0;
        while (*p >= '0' && *p <= '9') {
            v = (v * 10) + (*p - '0');
            p++;
        }
        *out_value = sign * v;
        return true;
    }
    return false;
}

static bool json_find_object_bounds(const char *json,
                                    const char *key,
                                    const char **out_begin,
                                    const char **out_end) {
    if (!json || !key || !out_begin || !out_end) return false;
    char token[128];
    int n = snprintf(token, sizeof(token), "\"%s\"", key);
    if (n <= 0 || (size_t)n >= sizeof(token)) return false;

    const char *p = strstr(json, token);
    if (!p) return false;
    p += (size_t)n;
    p = skip_ws(p);
    if (*p != ':') return false;
    p++;
    p = skip_ws(p);
    if (*p != '{') return false;

    const char *begin = p;
    int depth = 0;
    while (*p) {
        if (*p == '{') depth++;
        else if (*p == '}') {
            depth--;
            if (depth == 0) {
                *out_begin = begin;
                *out_end = p + 1;
                return true;
            }
        }
        p++;
    }
    return false;
}

CoreResult core_scene_bundle_resolve(const char *bundle_path, CoreSceneBundleInfo *out_info) {
    if (!bundle_path || !out_info) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    if (!core_scene_path_is_scene_bundle(bundle_path)) {
        CoreResult r = { CORE_ERR_FORMAT, "not a scene bundle path" };
        return r;
    }

    memset(out_info, 0, sizeof(*out_info));
    out_info->fluid_source_type = CORE_SCENE_SOURCE_UNKNOWN;

    char text[65536];
    if (!read_text_file(bundle_path, text, sizeof(text))) {
        CoreResult r = { CORE_ERR_IO, "failed to read bundle" };
        return r;
    }

    json_extract_string_limited(text, NULL, "bundle_type", out_info->bundle_type, sizeof(out_info->bundle_type));
    if (strstr(out_info->bundle_type, "scene_bundle") == NULL) {
        CoreResult r = { CORE_ERR_FORMAT, "invalid bundle_type" };
        return r;
    }
    json_extract_int_limited(text, NULL, "bundle_version", &out_info->bundle_version);
    json_extract_string_limited(text, NULL, "profile", out_info->profile, sizeof(out_info->profile));

    const char *fluid_begin = NULL;
    const char *fluid_end = NULL;
    bool has_fluid_obj = json_find_object_bounds(text, "fluid_source", &fluid_begin, &fluid_end);
    char raw_source_path[4096] = {0};

    if (has_fluid_obj) {
        json_extract_string_limited(fluid_begin, fluid_end, "kind",
                                    out_info->fluid_source_kind, sizeof(out_info->fluid_source_kind));
        json_extract_string_limited(fluid_begin, fluid_end, "path",
                                    raw_source_path, sizeof(raw_source_path));
    }
    if (raw_source_path[0] == '\0') {
        json_extract_string_limited(text, NULL, "manifest_path", raw_source_path, sizeof(raw_source_path));
    }
    if (raw_source_path[0] == '\0') {
        json_extract_string_limited(text, NULL, "fluid_path", raw_source_path, sizeof(raw_source_path));
    }
    if (raw_source_path[0] == '\0') {
        CoreResult r = { CORE_ERR_FORMAT, "missing fluid_source.path" };
        return r;
    }

    char base_dir[4096];
    CoreResult dir_r = core_scene_dirname(bundle_path, base_dir, sizeof(base_dir));
    if (dir_r.code != CORE_OK) return dir_r;
    CoreResult resolve_r =
        core_scene_resolve_path(base_dir, raw_source_path, out_info->fluid_source_path, sizeof(out_info->fluid_source_path));
    if (resolve_r.code != CORE_OK) return resolve_r;
    out_info->fluid_source_type = core_scene_detect_source_type(out_info->fluid_source_path);
    if (out_info->fluid_source_type == CORE_SCENE_SOURCE_UNKNOWN) {
        CoreResult r = { CORE_ERR_FORMAT, "unsupported fluid source type" };
        return r;
    }

    const char *meta_begin = NULL;
    const char *meta_end = NULL;
    if (json_find_object_bounds(text, "scene_metadata", &meta_begin, &meta_end)) {
        char raw_camera_path[4096] = {0};
        char raw_light_path[4096] = {0};
        if (json_extract_string_limited(meta_begin, meta_end, "camera_path",
                                        raw_camera_path, sizeof(raw_camera_path))) {
            if (core_scene_resolve_path(base_dir, raw_camera_path, out_info->camera_path, sizeof(out_info->camera_path)).code == CORE_OK) {
                out_info->has_camera_path = true;
            }
        }
        if (json_extract_string_limited(meta_begin, meta_end, "light_path",
                                        raw_light_path, sizeof(raw_light_path))) {
            if (core_scene_resolve_path(base_dir, raw_light_path, out_info->light_path, sizeof(out_info->light_path)).code == CORE_OK) {
                out_info->has_light_path = true;
            }
        }
        if (json_extract_string_limited(meta_begin, meta_end, "asset_mapping_profile",
                                        out_info->asset_mapping_profile, sizeof(out_info->asset_mapping_profile))) {
            out_info->has_asset_mapping_profile = true;
        }
    }

    // Backward-compatible top-level metadata keys.
    if (!out_info->has_camera_path) {
        char raw_camera_path[4096] = {0};
        if (json_extract_string_limited(text, NULL, "camera_path", raw_camera_path, sizeof(raw_camera_path))) {
            if (core_scene_resolve_path(base_dir, raw_camera_path, out_info->camera_path, sizeof(out_info->camera_path)).code == CORE_OK) {
                out_info->has_camera_path = true;
            }
        }
    }
    if (!out_info->has_light_path) {
        char raw_light_path[4096] = {0};
        if (json_extract_string_limited(text, NULL, "light_path", raw_light_path, sizeof(raw_light_path))) {
            if (core_scene_resolve_path(base_dir, raw_light_path, out_info->light_path, sizeof(out_info->light_path)).code == CORE_OK) {
                out_info->has_light_path = true;
            }
        }
    }
    if (!out_info->has_asset_mapping_profile) {
        if (json_extract_string_limited(text, NULL, "asset_mapping_profile",
                                        out_info->asset_mapping_profile, sizeof(out_info->asset_mapping_profile))) {
            out_info->has_asset_mapping_profile = true;
        }
    }

    return core_result_ok();
}
