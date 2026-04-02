/*
 * core_scene_compile.c
 * Part of the CodeWork Shared Libraries
 */

#include "core_scene_compile.h"

#include "core_base.h"
#include "core_io.h"

#include <ctype.h>
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

static const char *k_schema_family = "codework_scene";
static const char *k_authoring_variant = "scene_authoring_v1";
static const char *k_compiler_version = "0.1.0";

static bool json_slice_is_string(const JsonSlice *slice);

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

static bool json_slice_eq_string(const JsonSlice *slice, const char *text) {
    size_t expected_len;
    if (!json_slice_is_string(slice) || !text) return false;
    expected_len = strlen(text);
    if (slice->len != expected_len + 2u) return false;
    return strncmp(slice->begin + 1, text, expected_len) == 0;
}

static CoreResult collect_material_ids(const JsonSlice *materials,
                                       StringList *material_ids,
                                       char *diagnostics,
                                       size_t diagnostics_size) {
    char *array_text = NULL;
    const char *p;
    size_t index = 0;
    CoreResult out = core_result_ok();

    if (!json_slice_to_owned_text(materials, &array_text)) {
        return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
    }

    p = skip_ws(array_text);
    if (!p || *p != '[') {
        out = (CoreResult){ CORE_ERR_FORMAT, "invalid materials array" };
        goto done;
    }
    ++p;

    while (*p) {
        JsonSlice mat = {0};
        JsonSlice material_id = {0};
        char *id_text = NULL;
        const char *value_end;

        p = skip_ws(p);
        if (*p == ']') break;
        if (*p == ',') {
            ++p;
            continue;
        }

        value_end = parse_json_value_end(p);
        if (!value_end) {
            out = (CoreResult){ CORE_ERR_FORMAT, "invalid materials value" };
            goto done;
        }
        mat.begin = p;
        mat.len = (size_t)(value_end - p);

        if (!json_slice_is_object(&mat)) {
            diag_write(diagnostics, diagnostics_size, "materials[%zu] must be object", index);
            out = (CoreResult){ CORE_ERR_FORMAT, "invalid material entry" };
            goto done;
        }
        if (!json_find_top_level_value(mat.begin, "material_id", &material_id) ||
            !json_slice_extract_string_copy(&material_id, &id_text)) {
            diag_write(diagnostics, diagnostics_size, "materials[%zu] missing material_id string", index);
            out = (CoreResult){ CORE_ERR_FORMAT, "missing material_id" };
            goto done;
        }
        if (string_list_has(material_ids, id_text)) {
            diag_write(diagnostics, diagnostics_size, "duplicate material_id: %s", id_text);
            core_free(id_text);
            out = (CoreResult){ CORE_ERR_FORMAT, "duplicate material_id" };
            goto done;
        }
        if (!string_list_push_owned(material_ids, id_text)) {
            core_free(id_text);
            out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            goto done;
        }

        p = value_end;
        ++index;
    }

done:
    core_free(array_text);
    return out;
}

static CoreResult validate_objects(const JsonSlice *objects,
                                   const StringList *material_ids,
                                   char *diagnostics,
                                   size_t diagnostics_size) {
    char *array_text = NULL;
    const char *p;
    size_t index = 0;
    StringList object_ids;
    CoreResult out = core_result_ok();

    string_list_init(&object_ids);
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
        char *object_id_text = NULL;
        char *material_id_text = NULL;
        const char *value_end;

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
            !json_slice_extract_string_copy(&object_id, &object_id_text)) {
            diag_write(diagnostics, diagnostics_size, "objects[%zu] missing object_id string", index);
            out = (CoreResult){ CORE_ERR_FORMAT, "missing object_id" };
            goto done;
        }
        if (string_list_has(&object_ids, object_id_text)) {
            diag_write(diagnostics, diagnostics_size, "duplicate object_id: %s", object_id_text);
            core_free(object_id_text);
            out = (CoreResult){ CORE_ERR_FORMAT, "duplicate object_id" };
            goto done;
        }
        if (!string_list_push_owned(&object_ids, object_id_text)) {
            core_free(object_id_text);
            out = (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
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
                core_free(material_id_text);
                out = (CoreResult){ CORE_ERR_FORMAT, "unresolved material_ref" };
                goto done;
            }
            core_free(material_id_text);
            material_id_text = NULL;
        }

        p = value_end;
        ++index;
    }

done:
    core_free(array_text);
    string_list_free(&object_ids);
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
    JsonSlice materials = {0};
    JsonSlice lights = {0};
    JsonSlice cameras = {0};
    JsonSlice constraints = {0};
    JsonSlice extensions = {0};
    StringList material_ids;
    StrBuf out;
    char compile_meta[192];
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

    if (!json_slice_is_array(&materials) || !json_slice_is_array(&lights) ||
        !json_slice_is_array(&cameras) || !json_slice_is_array(&constraints)) {
        diag_write(diagnostics, diagnostics_size, "materials/lights/cameras/constraints must be arrays");
        return (CoreResult){ CORE_ERR_FORMAT, "invalid array fields" };
    }
    if (!json_slice_is_object(&extensions)) {
        diag_write(diagnostics, diagnostics_size, "extensions must be object");
        return (CoreResult){ CORE_ERR_FORMAT, "invalid extensions" };
    }

    string_list_init(&material_ids);
    validate_result = collect_material_ids(&materials, &material_ids, diagnostics, diagnostics_size);
    if (validate_result.code != CORE_OK) {
        string_list_free(&material_ids);
        return validate_result;
    }
    validate_result = validate_objects(&objects, &material_ids, diagnostics, diagnostics_size);
    if (validate_result.code != CORE_OK) {
        string_list_free(&material_ids);
        return validate_result;
    }
    string_list_free(&material_ids);

    snprintf(compile_meta, sizeof(compile_meta),
             "{\"compiler_version\":\"%s\",\"compiled_at_ns\":%lld}",
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
    if (!sb_appendn(&out, objects.begin, objects.len)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"materials\":")) goto oom;
    if (!sb_appendn(&out, materials.begin, materials.len)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"lights\":")) goto oom;
    if (!sb_appendn(&out, lights.begin, lights.len)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"cameras\":")) goto oom;
    if (!sb_appendn(&out, cameras.begin, cameras.len)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"constraints\":")) goto oom;
    if (!sb_appendn(&out, constraints.begin, constraints.len)) goto oom;
    if (!sb_append(&out, ",\n")) goto oom;
    if (!sb_append(&out, "  \"extensions\":")) goto oom;
    if (!sb_appendn(&out, extensions.begin, extensions.len)) goto oom;
    if (!sb_append(&out, "\n}\n")) goto oom;

    *out_runtime_json = out.data;
    return core_result_ok();

oom:
    sb_free(&out);
    diag_write(diagnostics, diagnostics_size, "out of memory while compiling scene");
    return (CoreResult){ CORE_ERR_OUT_OF_MEMORY, "out of memory" };
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
