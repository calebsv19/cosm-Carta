#include <json-c/json.h>

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct DiffCtx {
    int diff_count;
    int printed;
    int max_print;
} DiffCtx;

static bool compare_json(json_object *expected, json_object *actual, const char *path, DiffCtx *ctx);

static bool type_is_number(enum json_type t) {
    return t == json_type_int || t == json_type_double;
}

static const char *safe_path(const char *path) {
    if (!path || path[0] == '\0') return "/";
    return path;
}

static void diff_report(DiffCtx *ctx, const char *path, const char *fmt, ...) {
    va_list args;
    if (!ctx || !fmt) return;
    ctx->diff_count += 1;
    if (ctx->printed >= ctx->max_print) return;
    fprintf(stderr, "[scene_contract_diff] %s: ", safe_path(path));
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
    ctx->printed += 1;
}

static void make_key_path(char *out, size_t out_size, const char *base, const char *key) {
    if (!out || out_size == 0 || !key) return;
    if (!base || base[0] == '\0') {
        snprintf(out, out_size, "/%s", key);
    } else {
        snprintf(out, out_size, "%s/%s", base, key);
    }
}

static void make_index_path(char *out, size_t out_size, const char *base, size_t index) {
    if (!out || out_size == 0) return;
    if (!base || base[0] == '\0') {
        snprintf(out, out_size, "/[%zu]", index);
    } else {
        snprintf(out, out_size, "%s[%zu]", base, index);
    }
}

static void make_id_path(char *out, size_t out_size, const char *base, const char *id_key, const char *id_value) {
    if (!out || out_size == 0 || !id_key || !id_value) return;
    if (!base || base[0] == '\0') {
        snprintf(out, out_size, "/{%s=%s}", id_key, id_value);
    } else {
        snprintf(out, out_size, "%s{%s=%s}", base, id_key, id_value);
    }
}

static bool path_is_ignored(const char *path) {
    static const char *ignore_exact[] = {
        "/compile_meta/compiled_at_ns"
    };
    size_t i;
    if (!path || path[0] == '\0') return false;
    for (i = 0; i < sizeof(ignore_exact) / sizeof(ignore_exact[0]); ++i) {
        if (strcmp(path, ignore_exact[i]) == 0) return true;
    }
    if (strncmp(path, "/extensions/overlay_merge", strlen("/extensions/overlay_merge")) == 0) {
        size_t n = strlen("/extensions/overlay_merge");
        return path[n] == '\0' || path[n] == '/';
    }
    return false;
}

static const char *array_id_key_for_path(const char *path) {
    if (!path) return NULL;
    if (strcmp(path, "/objects") == 0) return "object_id";
    if (strcmp(path, "/materials") == 0) return "material_id";
    if (strcmp(path, "/lights") == 0) return "light_id";
    if (strcmp(path, "/cameras") == 0) return "camera_id";
    if (strcmp(path, "/constraints") == 0) return "constraint_id";
    return NULL;
}

static const char *object_id_value(json_object *obj, const char *id_key) {
    json_object *id_obj = NULL;
    if (!obj || !id_key || !json_object_is_type(obj, json_type_object)) return NULL;
    if (!json_object_object_get_ex(obj, id_key, &id_obj)) return NULL;
    if (!id_obj || !json_object_is_type(id_obj, json_type_string)) return NULL;
    return json_object_get_string(id_obj);
}

static ssize_t find_id_index(json_object *array_obj, const char *id_key, const char *id_value) {
    size_t i;
    size_t len;
    if (!array_obj || !id_key || !id_value || !json_object_is_type(array_obj, json_type_array)) return -1;
    len = json_object_array_length(array_obj);
    for (i = 0; i < len; ++i) {
        const char *candidate = object_id_value(json_object_array_get_idx(array_obj, i), id_key);
        if (candidate && strcmp(candidate, id_value) == 0) return (ssize_t)i;
    }
    return -1;
}

static bool array_has_duplicate_ids(json_object *array_obj, const char *id_key, const char *path, DiffCtx *ctx) {
    size_t i;
    size_t len;
    if (!array_obj || !id_key || !json_object_is_type(array_obj, json_type_array)) return false;
    len = json_object_array_length(array_obj);
    for (i = 0; i < len; ++i) {
        const char *id_i = object_id_value(json_object_array_get_idx(array_obj, i), id_key);
        size_t j;
        if (!id_i) continue;
        for (j = i + 1; j < len; ++j) {
            const char *id_j = object_id_value(json_object_array_get_idx(array_obj, j), id_key);
            if (id_j && strcmp(id_i, id_j) == 0) {
                diff_report(ctx, path, "duplicate %s value '%s' in array", id_key, id_i);
                return true;
            }
        }
    }
    return false;
}

static bool compare_array_indexed(json_object *expected, json_object *actual, const char *path, DiffCtx *ctx) {
    size_t len_expected;
    size_t len_actual;
    size_t i;
    bool ok = true;
    len_expected = json_object_array_length(expected);
    len_actual = json_object_array_length(actual);
    if (len_expected != len_actual) {
        diff_report(ctx, path, "array length mismatch expected=%zu actual=%zu", len_expected, len_actual);
        ok = false;
    }
    for (i = 0; i < len_expected && i < len_actual; ++i) {
        char child_path[512];
        make_index_path(child_path, sizeof(child_path), path, i);
        if (!compare_json(json_object_array_get_idx(expected, i), json_object_array_get_idx(actual, i), child_path, ctx)) {
            ok = false;
        }
    }
    return ok;
}

static bool compare_array_by_id(json_object *expected,
                                json_object *actual,
                                const char *path,
                                const char *id_key,
                                DiffCtx *ctx) {
    size_t i;
    size_t len_expected;
    size_t len_actual;
    bool ok = true;
    len_expected = json_object_array_length(expected);
    len_actual = json_object_array_length(actual);
    if (len_expected != len_actual) {
        diff_report(ctx, path, "array length mismatch expected=%zu actual=%zu", len_expected, len_actual);
        ok = false;
    }
    if (array_has_duplicate_ids(expected, id_key, path, ctx)) ok = false;
    if (array_has_duplicate_ids(actual, id_key, path, ctx)) ok = false;

    for (i = 0; i < len_expected; ++i) {
        json_object *exp_item = json_object_array_get_idx(expected, i);
        const char *id_value = object_id_value(exp_item, id_key);
        ssize_t actual_index;
        char child_path[512];
        if (!id_value) {
            diff_report(ctx, path, "expected element missing %s; falling back to index compare", id_key);
            return compare_array_indexed(expected, actual, path, ctx);
        }
        actual_index = find_id_index(actual, id_key, id_value);
        if (actual_index < 0) {
            make_id_path(child_path, sizeof(child_path), path, id_key, id_value);
            diff_report(ctx, child_path, "missing item in actual array");
            ok = false;
            continue;
        }
        make_id_path(child_path, sizeof(child_path), path, id_key, id_value);
        if (!compare_json(exp_item, json_object_array_get_idx(actual, (size_t)actual_index), child_path, ctx)) {
            ok = false;
        }
    }

    for (i = 0; i < len_actual; ++i) {
        const char *id_value = object_id_value(json_object_array_get_idx(actual, i), id_key);
        if (id_value && find_id_index(expected, id_key, id_value) < 0) {
            char child_path[512];
            make_id_path(child_path, sizeof(child_path), path, id_key, id_value);
            diff_report(ctx, child_path, "extra item present in actual array");
            ok = false;
        }
    }
    return ok;
}

static bool compare_object(json_object *expected, json_object *actual, const char *path, DiffCtx *ctx) {
    bool ok = true;
    {
        json_object_object_foreach(expected, key_expected, value_expected) {
            json_object *value_actual = NULL;
            char child_path[512];
            make_key_path(child_path, sizeof(child_path), path, key_expected);
            if (path_is_ignored(child_path)) continue;
            if (!json_object_object_get_ex(actual, key_expected, &value_actual)) {
                diff_report(ctx, child_path, "missing key in actual");
                ok = false;
                continue;
            }
            if (!compare_json(value_expected, value_actual, child_path, ctx)) ok = false;
        }
    }

    {
        json_object_object_foreach(actual, key_actual, value_actual) {
            json_object *value_expected = NULL;
            char child_path[512];
            (void)value_actual;
            make_key_path(child_path, sizeof(child_path), path, key_actual);
            if (path_is_ignored(child_path)) continue;
            if (!json_object_object_get_ex(expected, key_actual, &value_expected)) {
                diff_report(ctx, child_path, "extra key in actual");
                ok = false;
            }
        }
    }
    return ok;
}

static bool compare_json(json_object *expected, json_object *actual, const char *path, DiffCtx *ctx) {
    enum json_type t_expected;
    enum json_type t_actual;
    if (path_is_ignored(path)) return true;
    if (!expected && !actual) return true;
    if (!expected || !actual) {
        diff_report(ctx, path, "one side is null");
        return false;
    }

    t_expected = json_object_get_type(expected);
    t_actual = json_object_get_type(actual);

    if (!(type_is_number(t_expected) && type_is_number(t_actual)) && t_expected != t_actual) {
        diff_report(ctx, path, "type mismatch expected=%d actual=%d", (int)t_expected, (int)t_actual);
        return false;
    }

    if (type_is_number(t_expected) && type_is_number(t_actual)) {
        double a = json_object_get_double(expected);
        double b = json_object_get_double(actual);
        if (fabs(a - b) > 1e-9) {
            diff_report(ctx, path, "numeric mismatch expected=%.12g actual=%.12g", a, b);
            return false;
        }
        return true;
    }

    switch (t_expected) {
        case json_type_object:
            return compare_object(expected, actual, path, ctx);
        case json_type_array: {
            const char *id_key = array_id_key_for_path(path);
            if (id_key) return compare_array_by_id(expected, actual, path, id_key, ctx);
            return compare_array_indexed(expected, actual, path, ctx);
        }
        case json_type_string: {
            const char *a = json_object_get_string(expected);
            const char *b = json_object_get_string(actual);
            if ((!a && b) || (a && !b) || (a && b && strcmp(a, b) != 0)) {
                diff_report(ctx, path, "string mismatch expected='%s' actual='%s'", a ? a : "(null)", b ? b : "(null)");
                return false;
            }
            return true;
        }
        case json_type_boolean: {
            bool a = json_object_get_boolean(expected) ? true : false;
            bool b = json_object_get_boolean(actual) ? true : false;
            if (a != b) {
                diff_report(ctx, path, "boolean mismatch expected=%s actual=%s", a ? "true" : "false", b ? "true" : "false");
                return false;
            }
            return true;
        }
        case json_type_null:
            return true;
        default:
            return true;
    }
}

int main(int argc, char **argv) {
    json_object *expected = NULL;
    json_object *actual = NULL;
    DiffCtx ctx;
    bool ok;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <expected.json> <actual.json>\n", argv[0] ? argv[0] : "scene_contract_diff");
        return 2;
    }

    expected = json_object_from_file(argv[1]);
    if (!expected) {
        fprintf(stderr, "failed to parse expected JSON: %s\n", argv[1]);
        return 2;
    }
    actual = json_object_from_file(argv[2]);
    if (!actual) {
        fprintf(stderr, "failed to parse actual JSON: %s\n", argv[2]);
        json_object_put(expected);
        return 2;
    }

    ctx.diff_count = 0;
    ctx.printed = 0;
    ctx.max_print = 64;
    ok = compare_json(expected, actual, "", &ctx);
    if (ctx.diff_count > ctx.printed) {
        fprintf(stderr, "[scene_contract_diff] ... %d additional diff(s) not shown ...\n", ctx.diff_count - ctx.printed);
    }
    json_object_put(expected);
    json_object_put(actual);

    if (!ok || ctx.diff_count > 0) {
        fprintf(stderr, "[scene_contract_diff] FAILED with %d diff(s)\n", ctx.diff_count);
        return 1;
    }

    printf("scene_contract_diff: PASS\n");
    return 0;
}
