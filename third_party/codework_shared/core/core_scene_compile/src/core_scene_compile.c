/*
 * core_scene_compile.c
 * Part of the CodeWork Shared Libraries
 */

#include "core_scene_compile.h"

#include "core_base.h"
#include "core_io.h"
#include "core_scene.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct JsonSlice {
    const char *begin;
    size_t len;
} JsonSlice;

typedef struct StrBuf {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

typedef struct StringList {
    char **items;
    size_t count;
    size_t cap;
} StringList;

typedef struct NormalizedEntry {
    char *id_a;
    char *id_b;
    char *json;
} NormalizedEntry;

typedef struct NormalizedArray {
    NormalizedEntry *items;
    size_t count;
    size_t cap;
} NormalizedArray;

static const char *k_schema_family = "codework_scene";
static const char *k_authoring_variant = "scene_authoring_v1";
static const char *k_compiler_version = "0.3.0";

static bool json_slice_is_string(const JsonSlice *slice);
static bool json_slice_eq_string(const JsonSlice *slice, const char *text);
static const char *skip_ws(const char *p);

static void diag_write(char *diag, size_t diag_size, const char *fmt, ...) {
    va_list args;
    if (!diag || diag_size == 0) return;
    va_start(args, fmt);
    vsnprintf(diag, diag_size, fmt, args);
    va_end(args);
}

static void sb_init(StrBuf *sb) {
    if (!sb) return;
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void sb_free(StrBuf *sb) {
    if (!sb) return;
    core_free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static bool sb_reserve(StrBuf *sb, size_t needed) {
    char *next;
    size_t cap;
    if (!sb) return false;
    if (needed <= sb->cap) return true;
    cap = sb->cap ? sb->cap : 256;
    while (cap < needed) {
        if (cap > ((size_t)-1) / 2u) return false;
        cap *= 2u;
    }
    next = (char *)core_realloc(sb->data, cap);
    if (!next) return false;
    sb->data = next;
    sb->cap = cap;
    return true;
}

static bool sb_appendn(StrBuf *sb, const char *text, size_t n) {
    if (!sb || !text) return false;
    if (!sb_reserve(sb, sb->len + n + 1u)) return false;
    memcpy(sb->data + sb->len, text, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
    return true;
}

static bool sb_append(StrBuf *sb, const char *text) {
    if (!text) return false;
    return sb_appendn(sb, text, strlen(text));
}

static void string_list_init(StringList *list) {
    if (!list) return;
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static void string_list_free(StringList *list) {
    size_t i;
    if (!list) return;
    for (i = 0; i < list->count; ++i) {
        core_free(list->items[i]);
    }
    core_free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static bool string_list_has(const StringList *list, const char *value) {
    size_t i;
    if (!list || !value) return false;
    for (i = 0; i < list->count; ++i) {
        if (strcmp(list->items[i], value) == 0) return true;
    }
    return false;
}

static bool string_list_push_owned(StringList *list, char *owned) {
    char **next;
    size_t cap;
    if (!list || !owned) return false;
    if (list->count == list->cap) {
        cap = list->cap ? list->cap * 2u : 8u;
        next = (char **)core_realloc(list->items, cap * sizeof(char *));
        if (!next) return false;
        list->items = next;
        list->cap = cap;
    }
    list->items[list->count++] = owned;
    return true;
}

static char *dup_cstr(const char *text) {
    size_t len;
    char *copy;
    if (!text) return NULL;
    len = strlen(text);
    copy = (char *)core_alloc(len + 1u);
    if (!copy) return NULL;
    memcpy(copy, text, len + 1u);
    return copy;
}

static void normalized_array_init(NormalizedArray *arr) {
    if (!arr) return;
    arr->items = NULL;
    arr->count = 0;
    arr->cap = 0;
}

static void normalized_array_free(NormalizedArray *arr) {
    size_t i;
    if (!arr) return;
    for (i = 0; i < arr->count; ++i) {
        core_free(arr->items[i].id_a);
        core_free(arr->items[i].id_b);
        core_free(arr->items[i].json);
    }
    core_free(arr->items);
    arr->items = NULL;
    arr->count = 0;
    arr->cap = 0;
}

static bool normalized_array_push_owned(NormalizedArray *arr, NormalizedEntry *entry) {
    NormalizedEntry *next;
    size_t cap;
    if (!arr || !entry || !entry->json) return false;
    if (arr->count == arr->cap) {
        cap = arr->cap ? arr->cap * 2u : 8u;
        next = (NormalizedEntry *)core_realloc(arr->items, cap * sizeof(NormalizedEntry));
        if (!next) return false;
        arr->items = next;
        arr->cap = cap;
    }
    arr->items[arr->count++] = *entry;
    entry->id_a = NULL;
    entry->id_b = NULL;
    entry->json = NULL;
    return true;
}

static int cmp_norm_entry_ida(const void *lhs, const void *rhs) {
    const NormalizedEntry *a = (const NormalizedEntry *)lhs;
    const NormalizedEntry *b = (const NormalizedEntry *)rhs;
    if (!a->id_a && !b->id_a) return 0;
    if (!a->id_a) return -1;
    if (!b->id_a) return 1;
    return strcmp(a->id_a, b->id_a);
}

static int cmp_norm_entry_pair(const void *lhs, const void *rhs) {
    const NormalizedEntry *a = (const NormalizedEntry *)lhs;
    const NormalizedEntry *b = (const NormalizedEntry *)rhs;
    int cmp = 0;
    if (!a->id_a && !b->id_a) cmp = 0;
    else if (!a->id_a) cmp = -1;
    else if (!b->id_a) cmp = 1;
    else cmp = strcmp(a->id_a, b->id_a);
    if (cmp != 0) return cmp;

    if (!a->id_b && !b->id_b) return 0;
    if (!a->id_b) return -1;
    if (!b->id_b) return 1;
    return strcmp(a->id_b, b->id_b);
}

static bool normalized_array_sort_by_id(NormalizedArray *arr) {
    if (!arr) return false;
    if (arr->count > 1u) {
        qsort(arr->items, arr->count, sizeof(NormalizedEntry), cmp_norm_entry_ida);
    }
    return true;
}

static bool normalized_array_sort_by_pair(NormalizedArray *arr) {
    if (!arr) return false;
    if (arr->count > 1u) {
        qsort(arr->items, arr->count, sizeof(NormalizedEntry), cmp_norm_entry_pair);
    }
    return true;
}

static bool json_slice_to_owned_text(const JsonSlice *slice, char **out_text) {
    char *text;
    if (!slice || !out_text) return false;
    *out_text = NULL;
    text = (char *)core_alloc(slice->len + 1u);
    if (!text) return false;
    memcpy(text, slice->begin, slice->len);
    text[slice->len] = '\0';
    *out_text = text;
    return true;
}

static bool json_slice_extract_string_copy(const JsonSlice *slice, char **out_text) {
    char *text;
    size_t text_len;
    if (!json_slice_is_string(slice) || !out_text) return false;
    *out_text = NULL;
    text_len = slice->len - 2u;
    text = (char *)core_alloc(text_len + 1u);
    if (!text) return false;
    memcpy(text, slice->begin + 1, text_len);
    text[text_len] = '\0';
    *out_text = text;
    return true;
}

static bool json_slice_extract_owned_copy(const JsonSlice *slice, char **out_text) {
    char *text;
    if (!slice || !slice->begin || !out_text) return false;
    *out_text = NULL;
    text = (char *)core_alloc(slice->len + 1u);
    if (!text) return false;
    memcpy(text, slice->begin, slice->len);
    text[slice->len] = '\0';
    *out_text = text;
    return true;
}

static bool json_slice_parse_number(const JsonSlice *slice, double *out_value) {
    char *text = NULL;
    char *end = NULL;
    double value;
    if (!slice || !out_value) return false;
    if (!json_slice_to_owned_text(slice, &text)) return false;
    value = strtod(text, &end);
    if (end == text) {
        core_free(text);
        return false;
    }
    end = (char *)skip_ws(end);
    if (*end != '\0') {
        core_free(text);
        return false;
    }
    *out_value = value;
    core_free(text);
    return true;
}

static bool json_slice_parse_bool(const JsonSlice *slice, bool *out_value) {
    if (!slice || !slice->begin || !out_value) return false;
    if (slice->len == 4u && strncmp(slice->begin, "true", 4u) == 0) {
        *out_value = true;
        return true;
    }
    if (slice->len == 5u && strncmp(slice->begin, "false", 5u) == 0) {
        *out_value = false;
        return true;
    }
    return false;
}

static const char *skip_ws(const char *p) {
    while (p && *p && isspace((unsigned char)*p)) ++p;
    return p;
}

static const char *parse_json_string_end(const char *p) {
    if (!p || *p != '"') return NULL;
    ++p;
    while (*p) {
        if (*p == '\\') {
            if (!p[1]) return NULL;
            p += 2;
            continue;
        }
        if (*p == '"') return p + 1;
        ++p;
    }
    return NULL;
}

static const char *parse_json_value_end(const char *p) {
    const char *s = skip_ws(p);
    int obj_depth = 0;
    int arr_depth = 0;
    if (!s || !*s) return NULL;

    if (*s == '"') return parse_json_string_end(s);

    if (*s == '{' || *s == '[') {
        const char *it = s;
        while (*it) {
            if (*it == '"') {
                it = parse_json_string_end(it);
                if (!it) return NULL;
                continue;
            }
            if (*it == '{') obj_depth++;
            else if (*it == '}') {
                obj_depth--;
                if (obj_depth < 0) return NULL;
            } else if (*it == '[') arr_depth++;
            else if (*it == ']') {
                arr_depth--;
                if (arr_depth < 0) return NULL;
            }
            ++it;
            if (obj_depth == 0 && arr_depth == 0) return it;
        }
        return NULL;
    }

    {
        const char *it = s;
        while (*it) {
            if (*it == ',' || *it == '}') return it;
            if (isspace((unsigned char)*it)) {
                const char *after = skip_ws(it);
                if (*after == ',' || *after == '}') return after;
            }
            ++it;
        }
        return it;
    }
}

static bool key_matches_unescaped(const char *key_start, const char *key_end, const char *key) {
    size_t key_len;
    if (!key_start || !key_end || !key || key_end < key_start) return false;
    if (memchr(key_start, '\\', (size_t)(key_end - key_start)) != NULL) return false;
    key_len = (size_t)(key_end - key_start);
    return strlen(key) == key_len && strncmp(key_start, key, key_len) == 0;
}

static bool json_find_top_level_value(const char *json, const char *key, JsonSlice *out_slice) {
    const char *p;
    if (!json || !key || !out_slice) return false;
    p = skip_ws(json);
    if (!p || *p != '{') return false;
    ++p;

    while (*p) {
        const char *key_start;
        const char *key_end;
        const char *value_start;
        const char *value_end;

        p = skip_ws(p);
        if (*p == '}') return false;
        if (*p == ',') {
            ++p;
            continue;
        }
        if (*p != '"') return false;

        key_start = p + 1;
        p = parse_json_string_end(p);
        if (!p) return false;
        key_end = p - 1;

        p = skip_ws(p);
        if (*p != ':') return false;
        ++p;

        value_start = skip_ws(p);
        value_end = parse_json_value_end(value_start);
        if (!value_start || !value_end) return false;

        if (key_matches_unescaped(key_start, key_end, key)) {
            out_slice->begin = value_start;
            out_slice->len = (size_t)(value_end - value_start);
            return true;
        }

        p = value_end;
    }
    return false;
}

static bool json_slice_is_string(const JsonSlice *slice) {
    if (!slice || !slice->begin || slice->len < 2) return false;
    return slice->begin[0] == '"' && slice->begin[slice->len - 1] == '"';
}

static bool json_slice_is_array(const JsonSlice *slice) {
    if (!slice || !slice->begin || slice->len < 2) return false;
    return slice->begin[0] == '[';
}

static bool json_slice_is_object(const JsonSlice *slice) {
    if (!slice || !slice->begin || slice->len < 2) return false;
    return slice->begin[0] == '{';
}

static CoreResult json_slice_parse_core_object_vec3(const JsonSlice *slice, CoreObjectVec3 *out_vec) {
    JsonSlice x = {0};
    JsonSlice y = {0};
    JsonSlice z = {0};
    if (!slice || !out_vec || !json_slice_is_object(slice)) {
        return (CoreResult){ CORE_ERR_FORMAT, "invalid vec3 object" };
    }
    if (!json_find_top_level_value(slice->begin, "x", &x) ||
        !json_find_top_level_value(slice->begin, "y", &y) ||
        !json_find_top_level_value(slice->begin, "z", &z) ||
        !json_slice_parse_number(&x, &out_vec->x) ||
        !json_slice_parse_number(&y, &out_vec->y) ||
        !json_slice_parse_number(&z, &out_vec->z)) {
        return (CoreResult){ CORE_ERR_FORMAT, "invalid vec3 fields" };
    }
    return core_result_ok();
}

static CoreResult json_slice_parse_core_scene_frame3(const JsonSlice *slice, CoreSceneFrame3 *out_frame) {
    JsonSlice origin = {0};
    JsonSlice axis_u = {0};
    JsonSlice axis_v = {0};
    JsonSlice normal = {0};
    if (!slice || !out_frame || !json_slice_is_object(slice)) {
        return (CoreResult){ CORE_ERR_FORMAT, "invalid frame object" };
    }
    if (!json_find_top_level_value(slice->begin, "origin", &origin) ||
        !json_find_top_level_value(slice->begin, "axis_u", &axis_u) ||
        !json_find_top_level_value(slice->begin, "axis_v", &axis_v) ||
        !json_find_top_level_value(slice->begin, "normal", &normal)) {
        return (CoreResult){ CORE_ERR_FORMAT, "frame missing required fields" };
    }
    {
        CoreResult result = json_slice_parse_core_object_vec3(&origin, &out_frame->origin);
        if (result.code != CORE_OK) return result;
        result = json_slice_parse_core_object_vec3(&axis_u, &out_frame->axis_u);
        if (result.code != CORE_OK) return result;
        result = json_slice_parse_core_object_vec3(&axis_v, &out_frame->axis_v);
        if (result.code != CORE_OK) return result;
        return json_slice_parse_core_object_vec3(&normal, &out_frame->normal);
    }
}

static CoreResult parse_primitive_locked_plane(const JsonSlice *locked_plane,
                                               CoreObjectPlane *out_plane) {
    if (!locked_plane || !out_plane || !json_slice_is_string(locked_plane)) {
        return (CoreResult){ CORE_ERR_FORMAT, "locked_plane must be string" };
    }
    if (json_slice_eq_string(locked_plane, "xy")) {
        *out_plane = CORE_OBJECT_PLANE_XY;
        return core_result_ok();
    }
    if (json_slice_eq_string(locked_plane, "yz")) {
        *out_plane = CORE_OBJECT_PLANE_YZ;
        return core_result_ok();
    }
    if (json_slice_eq_string(locked_plane, "xz")) {
        *out_plane = CORE_OBJECT_PLANE_XZ;
        return core_result_ok();
    }
    return (CoreResult){ CORE_ERR_FORMAT, "locked_plane must be xy/yz/xz" };
}

static CoreResult parse_plane_primitive_payload(const JsonSlice *primitive,
                                                const char *expected_kind,
                                                CoreSceneObjectContract *contract) {
    JsonSlice kind = {0};
    JsonSlice width = {0};
    JsonSlice height = {0};
    JsonSlice frame = {0};
    JsonSlice lock_to_construction_plane = {0};
    JsonSlice lock_to_bounds = {0};
    char *kind_text = NULL;
    CoreResult result = core_result_ok();

    if (!primitive || !contract || !json_slice_is_object(primitive)) {
        return (CoreResult){ CORE_ERR_FORMAT, "primitive must be object" };
    }
    if (!json_find_top_level_value(primitive->begin, "kind", &kind) ||
        !json_slice_extract_string_copy(&kind, &kind_text)) {
        return (CoreResult){ CORE_ERR_FORMAT, "primitive.kind missing" };
    }
    if (strcmp(kind_text, expected_kind) != 0) {
        core_free(kind_text);
        return (CoreResult){ CORE_ERR_FORMAT, "primitive.kind mismatch" };
    }
    core_free(kind_text);

    if (!json_find_top_level_value(primitive->begin, "width", &width) ||
        !json_find_top_level_value(primitive->begin, "height", &height) ||
        !json_find_top_level_value(primitive->begin, "frame", &frame) ||
        !json_slice_parse_number(&width, &contract->plane_primitive.width) ||
        !json_slice_parse_number(&height, &contract->plane_primitive.height)) {
        return (CoreResult){ CORE_ERR_FORMAT, "primitive width/height missing or invalid" };
    }
    result = json_slice_parse_core_scene_frame3(&frame, &contract->plane_primitive.frame);
    if (result.code != CORE_OK) return result;

    if (json_find_top_level_value(primitive->begin, "lock_to_construction_plane", &lock_to_construction_plane) &&
        !json_slice_parse_bool(&lock_to_construction_plane,
                               &contract->plane_primitive.lock_to_construction_plane)) {
        return (CoreResult){ CORE_ERR_FORMAT, "primitive lock_to_construction_plane invalid" };
    }
    if (json_find_top_level_value(primitive->begin, "lock_to_bounds", &lock_to_bounds) &&
        !json_slice_parse_bool(&lock_to_bounds, &contract->plane_primitive.lock_to_bounds)) {
        return (CoreResult){ CORE_ERR_FORMAT, "primitive lock_to_bounds invalid" };
    }

    contract->has_plane_primitive = true;
    return core_result_ok();
}

static CoreResult parse_rect_prism_primitive_payload(const JsonSlice *primitive,
                                                     const char *expected_kind,
                                                     CoreSceneObjectContract *contract) {
    JsonSlice kind = {0};
    JsonSlice width = {0};
    JsonSlice height = {0};
    JsonSlice depth = {0};
    JsonSlice frame = {0};
    JsonSlice lock_to_construction_plane = {0};
    JsonSlice lock_to_bounds = {0};
    char *kind_text = NULL;
    CoreResult result = core_result_ok();

    if (!primitive || !contract || !json_slice_is_object(primitive)) {
        return (CoreResult){ CORE_ERR_FORMAT, "primitive must be object" };
    }
    if (!json_find_top_level_value(primitive->begin, "kind", &kind) ||
        !json_slice_extract_string_copy(&kind, &kind_text)) {
        return (CoreResult){ CORE_ERR_FORMAT, "primitive.kind missing" };
    }
    if (strcmp(kind_text, expected_kind) != 0) {
        core_free(kind_text);
        return (CoreResult){ CORE_ERR_FORMAT, "primitive.kind mismatch" };
    }
    core_free(kind_text);

    if (!json_find_top_level_value(primitive->begin, "width", &width) ||
        !json_find_top_level_value(primitive->begin, "height", &height) ||
        !json_find_top_level_value(primitive->begin, "depth", &depth) ||
        !json_find_top_level_value(primitive->begin, "frame", &frame) ||
        !json_slice_parse_number(&width, &contract->rect_prism_primitive.width) ||
        !json_slice_parse_number(&height, &contract->rect_prism_primitive.height) ||
        !json_slice_parse_number(&depth, &contract->rect_prism_primitive.depth)) {
        return (CoreResult){ CORE_ERR_FORMAT, "primitive width/height/depth missing or invalid" };
    }
    result = json_slice_parse_core_scene_frame3(&frame, &contract->rect_prism_primitive.frame);
    if (result.code != CORE_OK) return result;

    if (json_find_top_level_value(primitive->begin, "lock_to_construction_plane", &lock_to_construction_plane) &&
        !json_slice_parse_bool(&lock_to_construction_plane,
                               &contract->rect_prism_primitive.lock_to_construction_plane)) {
        return (CoreResult){ CORE_ERR_FORMAT, "primitive lock_to_construction_plane invalid" };
    }
    if (json_find_top_level_value(primitive->begin, "lock_to_bounds", &lock_to_bounds) &&
        !json_slice_parse_bool(&lock_to_bounds, &contract->rect_prism_primitive.lock_to_bounds)) {
        return (CoreResult){ CORE_ERR_FORMAT, "primitive lock_to_bounds invalid" };
    }

    contract->has_rect_prism_primitive = true;
    return core_result_ok();
}

static CoreResult validate_object_primitive_contract(const JsonSlice *obj,
                                                     size_t index,
                                                     const char *object_id_text,
                                                     char *diagnostics,
                                                     size_t diagnostics_size) {
    JsonSlice object_type = {0};
    JsonSlice primitive = {0};
    JsonSlice dimensional_mode = {0};
    JsonSlice locked_plane = {0};
    char *object_type_text = NULL;
    CoreSceneObjectKind kind = CORE_SCENE_OBJECT_KIND_UNKNOWN;
    CoreSceneObjectContract contract;
    CoreResult result = core_result_ok();
    bool has_primitive = false;

    if (!obj || !object_id_text) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid primitive validation arguments" };
    }

    has_primitive = json_find_top_level_value(obj->begin, "primitive", &primitive);
    if (!json_find_top_level_value(obj->begin, "object_type", &object_type)) {
        if (has_primitive) {
            diag_write(diagnostics, diagnostics_size, "objects[%zu] primitive payload requires object_type", index);
            return (CoreResult){ CORE_ERR_FORMAT, "missing object_type" };
        }
        return core_result_ok();
    }
    if (!json_slice_extract_string_copy(&object_type, &object_type_text)) {
        diag_write(diagnostics, diagnostics_size, "objects[%zu] object_type must be string", index);
        return (CoreResult){ CORE_ERR_FORMAT, "invalid object_type" };
    }

    result = core_scene_object_kind_parse(object_type_text, &kind);
    if (result.code != CORE_OK) {
        if (has_primitive) {
            diag_write(diagnostics, diagnostics_size,
                       "objects[%zu] primitive payload requires known primitive object_type", index);
            core_free(object_type_text);
            return (CoreResult){ CORE_ERR_FORMAT, "primitive object_type unknown" };
        }
        core_free(object_type_text);
        return core_result_ok();
    }

    if (kind != CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE &&
        kind != CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE) {
        if (has_primitive) {
            diag_write(diagnostics, diagnostics_size,
                       "objects[%zu] non-primitive object_type cannot carry primitive payload", index);
            core_free(object_type_text);
            return (CoreResult){ CORE_ERR_FORMAT, "primitive payload on non-primitive object" };
        }
        core_free(object_type_text);
        return core_result_ok();
    }

    if (!has_primitive) {
        diag_write(diagnostics, diagnostics_size,
                   "objects[%zu] primitive object_type requires primitive payload", index);
        core_free(object_type_text);
        return (CoreResult){ CORE_ERR_FORMAT, "missing primitive payload" };
    }

    core_scene_object_contract_init(&contract);
    result = core_scene_object_contract_prepare(&contract, object_id_text, kind);
    if (result.code != CORE_OK) {
        diag_write(diagnostics, diagnostics_size, "objects[%zu] primitive contract prepare failed", index);
        core_free(object_type_text);
        return result;
    }

    if (json_find_top_level_value(obj->begin, "dimensional_mode", &dimensional_mode)) {
        if (kind == CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE &&
            !json_slice_eq_string(&dimensional_mode, "plane_locked")) {
            diag_write(diagnostics, diagnostics_size,
                       "objects[%zu] plane_primitive dimensional_mode must be plane_locked", index);
            core_free(object_type_text);
            return (CoreResult){ CORE_ERR_FORMAT, "plane dimensional_mode mismatch" };
        }
        if (kind == CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE &&
            !json_slice_eq_string(&dimensional_mode, "full_3d")) {
            diag_write(diagnostics, diagnostics_size,
                       "objects[%zu] rect_prism_primitive dimensional_mode must be full_3d", index);
            core_free(object_type_text);
            return (CoreResult){ CORE_ERR_FORMAT, "rect prism dimensional_mode mismatch" };
        }
    }
    if (kind == CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE &&
        json_find_top_level_value(obj->begin, "locked_plane", &locked_plane)) {
        CoreObjectPlane plane = CORE_OBJECT_PLANE_XY;
        result = parse_primitive_locked_plane(&locked_plane, &plane);
        if (result.code != CORE_OK) {
            diag_write(diagnostics, diagnostics_size,
                       "objects[%zu] plane_primitive locked_plane invalid", index);
            core_free(object_type_text);
            return result;
        }
        result = core_object_set_plane_lock(&contract.object, plane);
        if (result.code != CORE_OK) {
            diag_write(diagnostics, diagnostics_size,
                       "objects[%zu] plane_primitive locked_plane failed to apply", index);
            core_free(object_type_text);
            return result;
        }
    }

    if (kind == CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE) {
        result = parse_plane_primitive_payload(&primitive, object_type_text, &contract);
    } else {
        result = parse_rect_prism_primitive_payload(&primitive, object_type_text, &contract);
    }
    if (result.code != CORE_OK) {
        diag_write(diagnostics, diagnostics_size,
                   "objects[%zu] invalid primitive payload: %s", index, result.message);
        core_free(object_type_text);
        return result;
    }

    result = core_scene_object_contract_validate(&contract);
    if (result.code != CORE_OK) {
        diag_write(diagnostics, diagnostics_size,
                   "objects[%zu] invalid primitive contract: %s", index, result.message);
        core_free(object_type_text);
        return result;
    }

    core_free(object_type_text);
    return core_result_ok();
}

static bool json_slice_eq_string(const JsonSlice *slice, const char *text) {
    size_t expected_len;
    if (!json_slice_is_string(slice) || !text) return false;
    expected_len = strlen(text);
    if (slice->len != expected_len + 2u) return false;
    return strncmp(slice->begin + 1, text, expected_len) == 0;
}

static bool render_normalized_array_json(const NormalizedArray *arr, char **out_json) {
    StrBuf out;
    size_t i;
    if (!arr || !out_json) return false;
    *out_json = NULL;

    sb_init(&out);
    if (!sb_append(&out, "[")) goto oom;
    for (i = 0; i < arr->count; ++i) {
        if (i > 0u && !sb_append(&out, ",")) goto oom;
        if (!sb_append(&out, arr->items[i].json)) goto oom;
    }
    if (!sb_append(&out, "]")) goto oom;
    *out_json = out.data;
    return true;

oom:
    sb_free(&out);
    return false;
}

static bool json_object_with_prefixed_string_field(const JsonSlice *object_slice,
                                                   const char *field_key,
                                                   const char *field_value,
                                                   char **out_json) {
    StrBuf out;
    const char *body_begin;
    size_t body_len;
    if (!object_slice || !field_key || !field_value || !out_json) return false;
    if (!json_slice_is_object(object_slice) || object_slice->len < 2u) return false;
    *out_json = NULL;

    body_begin = object_slice->begin + 1;
    body_len = object_slice->len - 2u;

    sb_init(&out);
    if (!sb_append(&out, "{\"")) goto oom;
    if (!sb_append(&out, field_key)) goto oom;
    if (!sb_append(&out, "\":\"")) goto oom;
    if (!sb_append(&out, field_value)) goto oom;
    if (!sb_append(&out, "\"")) goto oom;
    if (body_len > 0u) {
        if (!sb_append(&out, ",")) goto oom;
        if (!sb_appendn(&out, body_begin, body_len)) goto oom;
    }
    if (!sb_append(&out, "}")) goto oom;
    *out_json = out.data;
    return true;

oom:
    sb_free(&out);
    return false;
}

static CoreResult normalize_unique_id_array(const JsonSlice *array_slice,
                                            const char *array_name,
                                            const char *id_key,
                                            const char *fallback_prefix,
                                            StringList *out_ids,
                                            NormalizedArray *out_norm,
                                            char *diagnostics,
                                            size_t diagnostics_size) {
    char *array_text = NULL;
    const char *p;
    size_t index = 0;
    NormalizedEntry entry = {0};
    CoreResult out = core_result_ok();
    if (!array_slice || !array_name || !id_key || !out_ids || !out_norm) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }

    if (!json_slice_to_owned_text(array_slice, &array_text)) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }

    p = skip_ws(array_text);
    if (!p || *p != '[') {
        out = (CoreResult){ CORE_ERR_FORMAT, "invalid array" };
        goto done;
    }
    ++p;

    while (*p) {
        JsonSlice item = {0};
        JsonSlice item_id = {0};
        const char *value_end;
        bool id_generated = false;
        entry.id_a = NULL;
        entry.id_b = NULL;
        entry.json = NULL;

        p = skip_ws(p);
        if (*p == ']') break;
        if (*p == ',') {
            ++p;
            continue;
        }

        value_end = parse_json_value_end(p);
        if (!value_end) {
            out = (CoreResult){ CORE_ERR_FORMAT, "invalid array value" };
            goto done;
        }
        item.begin = p;
        item.len = (size_t)(value_end - p);

        if (!json_slice_is_object(&item)) {
            diag_write(diagnostics, diagnostics_size, "%s[%zu] must be object", array_name, index);
            out = (CoreResult){ CORE_ERR_FORMAT, "invalid array entry" };
            goto done;
        }
        if (!json_find_top_level_value(item.begin, id_key, &item_id) ||
            !json_slice_extract_string_copy(&item_id, &entry.id_a)) {
            if (fallback_prefix) {
                char generated[96];
                snprintf(generated, sizeof(generated), "%s_%04zu", fallback_prefix, index);
                entry.id_a = dup_cstr(generated);
                if (!entry.id_a) {
                    out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
                    goto done;
                }
                id_generated = true;
            } else {
                diag_write(diagnostics, diagnostics_size, "%s[%zu] missing %s string", array_name, index, id_key);
                out = (CoreResult){ CORE_ERR_FORMAT, "missing id" };
                goto done;
            }
        }
        if (string_list_has(out_ids, entry.id_a)) {
            diag_write(diagnostics, diagnostics_size, "duplicate %s: %s", id_key, entry.id_a);
            out = (CoreResult){ CORE_ERR_FORMAT, "duplicate id" };
            goto done;
        }
        if (id_generated) {
            if (!json_object_with_prefixed_string_field(&item, id_key, entry.id_a, &entry.json)) {
                out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
                goto done;
            }
        } else {
            if (!json_slice_extract_owned_copy(&item, &entry.json)) {
                out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
                goto done;
            }
        }
        {
            char *id_for_list = dup_cstr(entry.id_a);
            if (!id_for_list) {
                out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
                goto done;
            }
            if (!string_list_push_owned(out_ids, id_for_list)) {
                core_free(id_for_list);
                out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
                goto done;
            }
        }
        if (!normalized_array_push_owned(out_norm, &entry)) {
            out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            goto done;
        }
        p = value_end;
        ++index;
    }

done:
    core_free(array_text);
    if (out.code != CORE_OK) {
        core_free(entry.id_a);
        core_free(entry.id_b);
        core_free(entry.json);
    }
    return out;
}

static CoreResult normalize_objects_array(const JsonSlice *objects,
                                          const StringList *material_ids,
                                          StringList *object_ids,
                                          NormalizedArray *out_norm,
                                          char *diagnostics,
                                          size_t diagnostics_size) {
    char *array_text = NULL;
    const char *p;
    size_t index = 0;
    NormalizedEntry entry = {0};
    char *material_id_text = NULL;
    CoreResult out = core_result_ok();

    if (!json_slice_to_owned_text(objects, &array_text)) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }

    p = skip_ws(array_text);
    if (!p || *p != '[') {
        out = (CoreResult){ CORE_ERR_FORMAT, "invalid objects array" };
        goto done;
    }
    ++p;

    while (*p) {
        JsonSlice obj = {0};
        JsonSlice object_id = {0};
        JsonSlice material_ref = {0};
        JsonSlice material_ref_id = {0};
        JsonSlice geometry_ref = {0};
        JsonSlice geometry_ref_id = {0};
        const char *value_end;
        entry.id_a = NULL;
        entry.id_b = NULL;
        entry.json = NULL;
        core_free(material_id_text);
        material_id_text = NULL;

        p = skip_ws(p);
        if (*p == ']') break;
        if (*p == ',') {
            ++p;
            continue;
        }

        value_end = parse_json_value_end(p);
        if (!value_end) {
            out = (CoreResult){ CORE_ERR_FORMAT, "invalid objects value" };
            goto done;
        }
        obj.begin = p;
        obj.len = (size_t)(value_end - p);

        if (!json_slice_is_object(&obj)) {
            diag_write(diagnostics, diagnostics_size, "objects[%zu] must be object", index);
            out = (CoreResult){ CORE_ERR_FORMAT, "invalid object entry" };
            goto done;
        }

        if (!json_find_top_level_value(obj.begin, "object_id", &object_id) ||
            !json_slice_extract_string_copy(&object_id, &entry.id_a)) {
            diag_write(diagnostics, diagnostics_size, "objects[%zu] missing object_id string", index);
            out = (CoreResult){ CORE_ERR_FORMAT, "missing object_id" };
            goto done;
        }
        if (string_list_has(object_ids, entry.id_a)) {
            diag_write(diagnostics, diagnostics_size, "duplicate object_id: %s", entry.id_a);
            out = (CoreResult){ CORE_ERR_FORMAT, "duplicate object_id" };
            goto done;
        }

        if (json_find_top_level_value(obj.begin, "material_ref", &material_ref)) {
            if (!json_slice_is_object(&material_ref) ||
                !json_find_top_level_value(material_ref.begin, "id", &material_ref_id) ||
                !json_slice_extract_string_copy(&material_ref_id, &material_id_text)) {
                diag_write(diagnostics, diagnostics_size, "objects[%zu] has invalid material_ref.id", index);
                out = (CoreResult){ CORE_ERR_FORMAT, "invalid material_ref" };
                goto done;
            }
            if (!string_list_has(material_ids, material_id_text)) {
                diag_write(diagnostics, diagnostics_size,
                           "objects[%zu] material_ref.id unresolved: %s", index, material_id_text);
                out = (CoreResult){ CORE_ERR_FORMAT, "unresolved material_ref" };
                goto done;
            }
            core_free(material_id_text);
            material_id_text = NULL;
        }

        if (json_find_top_level_value(obj.begin, "geometry_ref", &geometry_ref)) {
            if (!json_slice_is_object(&geometry_ref) ||
                !json_find_top_level_value(geometry_ref.begin, "id", &geometry_ref_id) ||
                !json_slice_is_string(&geometry_ref_id)) {
                diag_write(diagnostics, diagnostics_size, "objects[%zu] has invalid geometry_ref.id", index);
                out = (CoreResult){ CORE_ERR_FORMAT, "invalid geometry_ref" };
                goto done;
            }
        }

        out = validate_object_primitive_contract(&obj, index, entry.id_a, diagnostics, diagnostics_size);
        if (out.code != CORE_OK) {
            goto done;
        }

        if (!json_slice_extract_owned_copy(&obj, &entry.json)) {
            out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            goto done;
        }
        {
            char *id_for_list = dup_cstr(entry.id_a);
            if (!id_for_list) {
                out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
                goto done;
            }
            if (!string_list_push_owned(object_ids, id_for_list)) {
                core_free(id_for_list);
                out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
                goto done;
            }
        }
        if (!normalized_array_push_owned(out_norm, &entry)) {
            out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            goto done;
        }
        p = value_end;
        ++index;
    }

done:
    core_free(array_text);
    core_free(material_id_text);
    if (out.code != CORE_OK) {
        core_free(entry.id_a);
        core_free(entry.id_b);
        core_free(entry.json);
    }
    return out;
}

static CoreResult normalize_hierarchy_array(const JsonSlice *hierarchy,
                                            const StringList *object_ids,
                                            NormalizedArray *out_norm,
                                            char *diagnostics,
                                            size_t diagnostics_size) {
    char *array_text = NULL;
    const char *p;
    size_t index = 0;
    NormalizedEntry entry = {0};
    CoreResult out = core_result_ok();

    if (!json_slice_to_owned_text(hierarchy, &array_text)) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }

    p = skip_ws(array_text);
    if (!p || *p != '[') {
        out = (CoreResult){ CORE_ERR_FORMAT, "invalid hierarchy array" };
        goto done;
    }
    ++p;

    while (*p) {
        JsonSlice relation = {0};
        JsonSlice parent_id = {0};
        JsonSlice child_id = {0};
        const char *value_end;
        entry.id_a = NULL;
        entry.id_b = NULL;
        entry.json = NULL;

        p = skip_ws(p);
        if (*p == ']') break;
        if (*p == ',') {
            ++p;
            continue;
        }

        value_end = parse_json_value_end(p);
        if (!value_end) {
            out = (CoreResult){ CORE_ERR_FORMAT, "invalid hierarchy value" };
            goto done;
        }
        relation.begin = p;
        relation.len = (size_t)(value_end - p);

        if (!json_slice_is_object(&relation)) {
            diag_write(diagnostics, diagnostics_size, "hierarchy[%zu] must be object", index);
            out = (CoreResult){ CORE_ERR_FORMAT, "invalid hierarchy entry" };
            goto done;
        }
        if (!json_find_top_level_value(relation.begin, "parent_object_id", &parent_id) ||
            !json_slice_extract_string_copy(&parent_id, &entry.id_a)) {
            diag_write(diagnostics, diagnostics_size, "hierarchy[%zu] missing parent_object_id string", index);
            out = (CoreResult){ CORE_ERR_FORMAT, "missing hierarchy parent" };
            goto done;
        }
        if (!json_find_top_level_value(relation.begin, "child_object_id", &child_id) ||
            !json_slice_extract_string_copy(&child_id, &entry.id_b)) {
            diag_write(diagnostics, diagnostics_size, "hierarchy[%zu] missing child_object_id string", index);
            out = (CoreResult){ CORE_ERR_FORMAT, "missing hierarchy child" };
            goto done;
        }
        if (strcmp(entry.id_a, entry.id_b) == 0) {
            diag_write(diagnostics, diagnostics_size, "hierarchy[%zu] parent and child cannot match: %s",
                       index, entry.id_a);
            out = (CoreResult){ CORE_ERR_FORMAT, "invalid hierarchy self edge" };
            goto done;
        }
        if (!string_list_has(object_ids, entry.id_a)) {
            diag_write(diagnostics, diagnostics_size, "hierarchy[%zu] parent unresolved: %s", index, entry.id_a);
            out = (CoreResult){ CORE_ERR_FORMAT, "unresolved hierarchy parent" };
            goto done;
        }
        if (!string_list_has(object_ids, entry.id_b)) {
            diag_write(diagnostics, diagnostics_size, "hierarchy[%zu] child unresolved: %s", index, entry.id_b);
            out = (CoreResult){ CORE_ERR_FORMAT, "unresolved hierarchy child" };
            goto done;
        }
        if (!json_slice_extract_owned_copy(&relation, &entry.json)) {
            out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            goto done;
        }
        if (!normalized_array_push_owned(out_norm, &entry)) {
            out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            goto done;
        }

        p = value_end;
        ++index;
    }

done:
    core_free(array_text);
    if (out.code != CORE_OK) {
        core_free(entry.id_a);
        core_free(entry.id_b);
        core_free(entry.json);
    }
    return out;
}

static long long unix_ns_now(void) {
    time_t now = time(NULL);
    if (now < 0) return 0;
    return (long long)now * 1000000000LL;
}

static CoreResult compile_inner(const char *authoring_json,
                                char **out_runtime_json,
                                char *diagnostics,
                                size_t diagnostics_size) {
    JsonSlice scene_id = {0};
    JsonSlice schema_family = {0};
    JsonSlice schema_variant = {0};
    JsonSlice schema_version = {0};
    JsonSlice space_mode_default = {0};
    JsonSlice unit_system = {0};
    JsonSlice world_scale = {0};
    JsonSlice objects = {0};
    JsonSlice hierarchy = {0};
    JsonSlice materials = {0};
    JsonSlice lights = {0};
    JsonSlice cameras = {0};
    JsonSlice constraints = {0};
    JsonSlice extensions = {0};
    StringList object_ids;
    StringList material_ids;
    StringList light_ids;
    StringList camera_ids;
    NormalizedArray norm_objects;
    NormalizedArray norm_hierarchy;
    NormalizedArray norm_materials;
    NormalizedArray norm_lights;
    NormalizedArray norm_cameras;
    StrBuf out;
    char *norm_objects_json = NULL;
    char *norm_hierarchy_json = NULL;
    char *norm_materials_json = NULL;
    char *norm_lights_json = NULL;
    char *norm_cameras_json = NULL;
    char compile_meta[192];
    double world_scale_number = 1.0;
    CoreResult validate_result;

    if (!authoring_json || !out_runtime_json) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }
    *out_runtime_json = NULL;
    if (diagnostics && diagnostics_size > 0) diagnostics[0] = '\0';

    if (!json_find_top_level_value(authoring_json, "schema_family", &schema_family) ||
        !json_slice_eq_string(&schema_family, k_schema_family)) {
        diag_write(diagnostics, diagnostics_size, "invalid or missing schema_family");
        return (CoreResult){ CORE_ERR_FORMAT, "invalid schema_family" };
    }

    if (!json_find_top_level_value(authoring_json, "schema_variant", &schema_variant) ||
        !json_slice_eq_string(&schema_variant, k_authoring_variant)) {
        diag_write(diagnostics, diagnostics_size, "expected schema_variant=%s", k_authoring_variant);
        return (CoreResult){ CORE_ERR_FORMAT, "invalid schema_variant" };
    }

    if (!json_find_top_level_value(authoring_json, "schema_version", &schema_version)) {
        diag_write(diagnostics, diagnostics_size, "missing schema_version");
        return (CoreResult){ CORE_ERR_FORMAT, "missing schema_version" };
    }

    if (!json_find_top_level_value(authoring_json, "scene_id", &scene_id) || !json_slice_is_string(&scene_id)) {
        diag_write(diagnostics, diagnostics_size, "missing or invalid scene_id");
        return (CoreResult){ CORE_ERR_FORMAT, "missing scene_id" };
    }

    if (!json_find_top_level_value(authoring_json, "objects", &objects) || !json_slice_is_array(&objects)) {
        diag_write(diagnostics, diagnostics_size, "missing or invalid objects array");
        return (CoreResult){ CORE_ERR_FORMAT, "missing objects" };
    }
    if (!json_find_top_level_value(authoring_json, "hierarchy", &hierarchy)) {
        hierarchy.begin = "[]";
        hierarchy.len = 2;
    }

    if (!json_find_top_level_value(authoring_json, "space_mode_default", &space_mode_default)) {
        space_mode_default.begin = "\"2d\"";
        space_mode_default.len = 4;
    }
    if (!json_find_top_level_value(authoring_json, "unit_system", &unit_system)) {
        unit_system.begin = "\"meters\"";
        unit_system.len = 8;
    }
    if (!json_find_top_level_value(authoring_json, "world_scale", &world_scale)) {
        world_scale.begin = "1.0";
        world_scale.len = 3;
    }
    if (!json_find_top_level_value(authoring_json, "materials", &materials)) {
        materials.begin = "[]";
        materials.len = 2;
    }
    if (!json_find_top_level_value(authoring_json, "lights", &lights)) {
        lights.begin = "[]";
        lights.len = 2;
    }
    if (!json_find_top_level_value(authoring_json, "cameras", &cameras)) {
        cameras.begin = "[]";
        cameras.len = 2;
    }
    if (!json_find_top_level_value(authoring_json, "constraints", &constraints)) {
        constraints.begin = "[]";
        constraints.len = 2;
    }
    if (!json_find_top_level_value(authoring_json, "extensions", &extensions)) {
        extensions.begin = "{}";
        extensions.len = 2;
    }

    if (!json_slice_is_array(&hierarchy) || !json_slice_is_array(&materials) || !json_slice_is_array(&lights) ||
        !json_slice_is_array(&cameras) || !json_slice_is_array(&constraints)) {
        diag_write(diagnostics, diagnostics_size,
                   "hierarchy/materials/lights/cameras/constraints must be arrays");
        return (CoreResult){ CORE_ERR_FORMAT, "invalid array fields" };
    }
    if (!json_slice_is_object(&extensions)) {
        diag_write(diagnostics, diagnostics_size, "extensions must be object");
        return (CoreResult){ CORE_ERR_FORMAT, "invalid extensions" };
    }
    if (!json_slice_is_string(&space_mode_default) ||
        !(json_slice_eq_string(&space_mode_default, "2d") || json_slice_eq_string(&space_mode_default, "3d"))) {
        diag_write(diagnostics, diagnostics_size, "space_mode_default must be \"2d\" or \"3d\"");
        return (CoreResult){ CORE_ERR_FORMAT, "invalid space_mode_default" };
    }
    if (!json_slice_is_string(&unit_system)) {
        diag_write(diagnostics, diagnostics_size, "unit_system must be string");
        return (CoreResult){ CORE_ERR_FORMAT, "invalid unit_system" };
    }
    if (!json_slice_parse_number(&world_scale, &world_scale_number) || world_scale_number <= 0.0) {
        diag_write(diagnostics, diagnostics_size, "world_scale must be a positive number");
        return (CoreResult){ CORE_ERR_FORMAT, "invalid world_scale" };
    }

    string_list_init(&object_ids);
    string_list_init(&material_ids);
    string_list_init(&light_ids);
    string_list_init(&camera_ids);
    normalized_array_init(&norm_objects);
    normalized_array_init(&norm_hierarchy);
    normalized_array_init(&norm_materials);
    normalized_array_init(&norm_lights);
    normalized_array_init(&norm_cameras);

    validate_result =
        normalize_unique_id_array(&materials,
                                  "materials",
                                  "material_id",
                                  NULL,
                                  &material_ids,
                                  &norm_materials,
                                  diagnostics,
                                  diagnostics_size);
    if (validate_result.code != CORE_OK) {
        goto validation_fail;
    }
    validate_result =
        normalize_objects_array(&objects, &material_ids, &object_ids, &norm_objects, diagnostics, diagnostics_size);
    if (validate_result.code != CORE_OK) {
        goto validation_fail;
    }
    validate_result = normalize_unique_id_array(&lights,
                                                "lights",
                                                "light_id",
                                                "light",
                                                &light_ids,
                                                &norm_lights,
                                                diagnostics,
                                                diagnostics_size);
    if (validate_result.code != CORE_OK) {
        goto validation_fail;
    }
    validate_result = normalize_unique_id_array(&cameras,
                                                "cameras",
                                                "camera_id",
                                                "camera",
                                                &camera_ids,
                                                &norm_cameras,
                                                diagnostics,
                                                diagnostics_size);
    if (validate_result.code != CORE_OK) {
        goto validation_fail;
    }
    validate_result =
        normalize_hierarchy_array(&hierarchy, &object_ids, &norm_hierarchy, diagnostics, diagnostics_size);
    if (validate_result.code != CORE_OK) {
        goto validation_fail;
    }

    if (!normalized_array_sort_by_id(&norm_objects) || !normalized_array_sort_by_id(&norm_materials) ||
        !normalized_array_sort_by_id(&norm_lights) || !normalized_array_sort_by_id(&norm_cameras) ||
        !normalized_array_sort_by_pair(&norm_hierarchy)) {
        validate_result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        diag_write(diagnostics, diagnostics_size, "out of memory while sorting normalized arrays");
        goto validation_fail;
    }

    if (!render_normalized_array_json(&norm_objects, &norm_objects_json) ||
        !render_normalized_array_json(&norm_hierarchy, &norm_hierarchy_json) ||
        !render_normalized_array_json(&norm_materials, &norm_materials_json) ||
        !render_normalized_array_json(&norm_lights, &norm_lights_json) ||
        !render_normalized_array_json(&norm_cameras, &norm_cameras_json)) {
        validate_result = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        diag_write(diagnostics, diagnostics_size, "out of memory while rendering normalized arrays");
        goto validation_fail;
    }

    snprintf(compile_meta, sizeof(compile_meta),
             "{\"compiler_version\":\"%s\",\"compiled_at_ns\":%lld,\"normalization\":\"v0.3_sorted_lanes_primitive_contract\"}",
             k_compiler_version,
             unix_ns_now());

    sb_init(&out);
    if (!sb_append(&out, "{\n")) goto oom;
    if (!sb_append(&out, "  \"schema_family\":\"codework_scene\",\n")) goto oom;
    if (!sb_append(&out, "  \"schema_variant\":\"scene_runtime_v1\",\n")) goto oom;
    if (!sb_append(&out, "  \"schema_version\":1,\n")) goto oom;
    if (!sb_append(&out, "  \"scene_id\":")) goto oom;
    if (!sb_appendn(&out, scene_id.begin, scene_id.len)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"source_scene_id\":")) goto oom;
    if (!sb_appendn(&out, scene_id.begin, scene_id.len)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"compile_meta\":")) goto oom;
    if (!sb_append(&out, compile_meta)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"space_mode_default\":")) goto oom;
    if (!sb_appendn(&out, space_mode_default.begin, space_mode_default.len)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"unit_system\":")) goto oom;
    if (!sb_appendn(&out, unit_system.begin, unit_system.len)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"world_scale\":")) goto oom;
    if (!sb_appendn(&out, world_scale.begin, world_scale.len)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"objects\":")) goto oom;
    if (!sb_append(&out, norm_objects_json)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"hierarchy\":")) goto oom;
    if (!sb_append(&out, norm_hierarchy_json)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"materials\":")) goto oom;
    if (!sb_append(&out, norm_materials_json)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"lights\":")) goto oom;
    if (!sb_append(&out, norm_lights_json)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"cameras\":")) goto oom;
    if (!sb_append(&out, norm_cameras_json)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"constraints\":")) goto oom;
    if (!sb_appendn(&out, constraints.begin, constraints.len)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"extensions\":")) goto oom;
    if (!sb_appendn(&out, extensions.begin, extensions.len)) goto oom;
    if (!sb_append(&out, "\n}\n")) goto oom;

    *out_runtime_json = out.data;
    core_free(norm_objects_json);
    core_free(norm_hierarchy_json);
    core_free(norm_materials_json);
    core_free(norm_lights_json);
    core_free(norm_cameras_json);
    normalized_array_free(&norm_objects);
    normalized_array_free(&norm_hierarchy);
    normalized_array_free(&norm_materials);
    normalized_array_free(&norm_lights);
    normalized_array_free(&norm_cameras);
    string_list_free(&object_ids);
    string_list_free(&material_ids);
    string_list_free(&light_ids);
    string_list_free(&camera_ids);
    return core_result_ok();

oom:
    sb_free(&out);
    core_free(norm_objects_json);
    core_free(norm_hierarchy_json);
    core_free(norm_materials_json);
    core_free(norm_lights_json);
    core_free(norm_cameras_json);
    normalized_array_free(&norm_objects);
    normalized_array_free(&norm_hierarchy);
    normalized_array_free(&norm_materials);
    normalized_array_free(&norm_lights);
    normalized_array_free(&norm_cameras);
    string_list_free(&object_ids);
    string_list_free(&material_ids);
    string_list_free(&light_ids);
    string_list_free(&camera_ids);
    diag_write(diagnostics, diagnostics_size, "out of memory while compiling scene");
    return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };

validation_fail:
    core_free(norm_objects_json);
    core_free(norm_hierarchy_json);
    core_free(norm_materials_json);
    core_free(norm_lights_json);
    core_free(norm_cameras_json);
    normalized_array_free(&norm_objects);
    normalized_array_free(&norm_hierarchy);
    normalized_array_free(&norm_materials);
    normalized_array_free(&norm_lights);
    normalized_array_free(&norm_cameras);
    string_list_free(&object_ids);
    string_list_free(&material_ids);
    string_list_free(&light_ids);
    string_list_free(&camera_ids);
    return validate_result;
}

CoreResult core_scene_compile_authoring_to_runtime(const char *authoring_json,
                                                   char **out_runtime_json,
                                                   char *diagnostics,
                                                   size_t diagnostics_size) {
    return compile_inner(authoring_json, out_runtime_json, diagnostics, diagnostics_size);
}

CoreResult core_scene_compile_authoring_file_to_runtime_file(const char *authoring_path,
                                                             const char *runtime_path,
                                                             char *diagnostics,
                                                             size_t diagnostics_size) {
    CoreBuffer data = {0};
    CoreResult r;
    char *text;
    char *runtime_json = NULL;

    if (!authoring_path || !runtime_path) {
        return (CoreResult){ CORE_ERR_INVALID_ARG, "invalid argument" };
    }

    r = core_io_read_all(authoring_path, &data);
    if (r.code != CORE_OK) {
        diag_write(diagnostics, diagnostics_size, "failed to read authoring file: %s", authoring_path);
        return r;
    }

    text = (char *)core_alloc(data.size + 1u);
    if (!text) {
        core_io_buffer_free(&data);
        diag_write(diagnostics, diagnostics_size, "out of memory while reading authoring file");
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }
    memcpy(text, data.data, data.size);
    text[data.size] = '\0';
    core_io_buffer_free(&data);

    r = compile_inner(text, &runtime_json, diagnostics, diagnostics_size);
    core_free(text);
    if (r.code != CORE_OK) return r;

    r = core_io_write_all(runtime_path, runtime_json, strlen(runtime_json));
    core_free(runtime_json);
    if (r.code != CORE_OK) {
        diag_write(diagnostics, diagnostics_size, "failed to write runtime file: %s", runtime_path);
        return r;
    }

    return core_result_ok();
}
