#include "app_headless_util.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool map_forge_headless_fail(char *out_error, size_t out_error_size, const char *message) {
    map_forge_headless_diag_set(out_error, out_error_size, message ? message : "unknown error");
    return false;
}

void map_forge_headless_diag_set(char *out, size_t out_size, const char *message) {
    if (!out || out_size == 0u || !message) {
        return;
    }
    snprintf(out, out_size, "%s", message);
}

bool map_forge_headless_copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0u || !src) {
        return false;
    }
    if (snprintf(dst, dst_size, "%s", src) >= (int)dst_size) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

bool map_forge_headless_ensure_dir_recursive_mode(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    size_t len = 0u;
    if (!path || path[0] == '\0') {
        return false;
    }
    len = strnlen(path, sizeof(tmp) - 1u);
    if (len == 0u || len >= sizeof(tmp)) {
        return false;
    }
    memcpy(tmp, path, len);
    tmp[len] = '\0';
    for (char *p = tmp + 1; *p; ++p) {
        if (*p != '/') {
            continue;
        }
        *p = '\0';
        if (tmp[0] != '\0' && mkdir(tmp, mode) != 0 && errno != EEXIST) {
            return false;
        }
        *p = '/';
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

bool map_forge_headless_ensure_parent_dir_mode(const char *path, mode_t mode) {
    char dir[PATH_MAX];
    if (!map_forge_headless_parent_dir(path, dir, sizeof(dir))) {
        return false;
    }
    return map_forge_headless_ensure_dir_recursive_mode(dir, mode);
}

bool map_forge_headless_parent_dir(const char *path, char *out_dir, size_t out_size) {
    const char *slash = NULL;
    if (!path || !out_dir || out_size == 0u) {
        return false;
    }
    slash = strrchr(path, '/');
    if (!slash) {
        return map_forge_headless_copy_string(out_dir, out_size, ".");
    }
    if (slash == path) {
        return map_forge_headless_copy_string(out_dir, out_size, "/");
    }
    {
        size_t len = (size_t)(slash - path);
        if (len >= out_size) {
            return false;
        }
        memcpy(out_dir, path, len);
        out_dir[len] = '\0';
    }
    return true;
}

bool map_forge_headless_realpath_or_copy(const char *path, char *out_path, size_t out_size) {
    char resolved[PATH_MAX];
    if (!path || !out_path || out_size == 0u) {
        return false;
    }
    if (realpath(path, resolved)) {
        return map_forge_headless_copy_string(out_path, out_size, resolved);
    }
    return map_forge_headless_copy_string(out_path, out_size, path);
}

bool map_forge_headless_resolve_relative(const char *base_dir,
                                         const char *path,
                                         char *out_path,
                                         size_t out_size) {
    if (!path || !out_path || out_size == 0u) {
        return false;
    }
    if (path[0] == '/') {
        return map_forge_headless_realpath_or_copy(path, out_path, out_size);
    }
    if (!base_dir || base_dir[0] == '\0' || strcmp(base_dir, ".") == 0) {
        return map_forge_headless_realpath_or_copy(path, out_path, out_size);
    }
    {
        char current_dir[PATH_MAX];
        char parent_dir[PATH_MAX];

        if (!map_forge_headless_copy_string(current_dir, sizeof(current_dir), base_dir)) {
            return false;
        }
        while (current_dir[0] != '\0') {
            char joined[PATH_MAX];
            if (snprintf(joined, sizeof(joined), "%s/%s", current_dir, path) >= (int)sizeof(joined)) {
                return false;
            }
            if (realpath(joined, out_path)) {
                return true;
            }
            if (!map_forge_headless_parent_dir(current_dir, parent_dir, sizeof(parent_dir))) {
                break;
            }
            if (strcmp(parent_dir, current_dir) == 0) {
                break;
            }
            if (!map_forge_headless_copy_string(current_dir, sizeof(current_dir), parent_dir)) {
                return false;
            }
            if (strcmp(current_dir, "/") == 0) {
                if (snprintf(joined, sizeof(joined), "/%s", path) >= (int)sizeof(joined)) {
                    return false;
                }
                if (realpath(joined, out_path)) {
                    return true;
                }
                break;
            }
        }
    }
    return map_forge_headless_realpath_or_copy(path, out_path, out_size);
}

void map_forge_headless_json_write_string(FILE *file, const char *value) {
    const unsigned char *cursor = (const unsigned char *)(value ? value : "");
    fputc('"', file);
    while (*cursor) {
        switch (*cursor) {
            case '\\':
                fputs("\\\\", file);
                break;
            case '"':
                fputs("\\\"", file);
                break;
            case '\n':
                fputs("\\n", file);
                break;
            case '\r':
                fputs("\\r", file);
                break;
            case '\t':
                fputs("\\t", file);
                break;
            default:
                if (*cursor < 0x20u) {
                    fprintf(file, "\\u%04x", (unsigned int)*cursor);
                } else {
                    fputc((int)*cursor, file);
                }
                break;
        }
        cursor++;
    }
    fputc('"', file);
}

bool map_forge_headless_json_get_object(json_object *owner, const char *key, json_object **out_obj) {
    json_object *obj = NULL;
    if (out_obj) {
        *out_obj = NULL;
    }
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_object)) {
        return false;
    }
    if (out_obj) {
        *out_obj = obj;
    }
    return true;
}

bool map_forge_headless_json_get_string_ref(json_object *owner, const char *key, const char **out_value) {
    json_object *obj = NULL;
    if (out_value) {
        *out_value = NULL;
    }
    if (!owner || !key || !json_object_object_get_ex(owner, key, &obj) ||
        !json_object_is_type(obj, json_type_string)) {
        return false;
    }
    if (out_value) {
        *out_value = json_object_get_string(obj);
    }
    return true;
}

bool map_forge_headless_json_get_required_string(json_object *obj,
                                                 const char *key,
                                                 char *out_value,
                                                 size_t out_size,
                                                 char *out_error,
                                                 size_t out_error_size) {
    json_object *value = NULL;
    const char *raw = NULL;
    if (!obj || !key || !out_value || out_size == 0u) {
        return map_forge_headless_fail(out_error, out_error_size, "invalid string parse request");
    }
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_string)) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "missing or invalid string field: %s", key);
        return map_forge_headless_fail(out_error, out_error_size, buffer);
    }
    raw = json_object_get_string(value);
    if (!raw || raw[0] == '\0') {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "empty string field: %s", key);
        return map_forge_headless_fail(out_error, out_error_size, buffer);
    }
    return map_forge_headless_copy_string(out_value, out_size, raw);
}

bool map_forge_headless_json_get_required_u32(json_object *obj,
                                              const char *key,
                                              uint32_t *out_value,
                                              char *out_error,
                                              size_t out_error_size) {
    json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return map_forge_headless_fail(out_error, out_error_size, "invalid integer parse request");
    }
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_int)) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "missing or invalid integer field: %s", key);
        return map_forge_headless_fail(out_error, out_error_size, buffer);
    }
    *out_value = (uint32_t)json_object_get_int(value);
    return true;
}

bool map_forge_headless_json_get_optional_string(json_object *obj,
                                                 const char *key,
                                                 char *out_value,
                                                 size_t out_size) {
    const char *raw = NULL;
    if (!map_forge_headless_json_get_string_ref(obj, key, &raw) || !raw || raw[0] == '\0') {
        return false;
    }
    return map_forge_headless_copy_string(out_value, out_size, raw);
}

bool map_forge_headless_json_get_optional_bool(json_object *obj, const char *key, bool *out_value) {
    json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_boolean)) {
        return false;
    }
    *out_value = json_object_get_boolean(value) ? true : false;
    return true;
}

bool map_forge_headless_json_get_optional_int(json_object *obj, const char *key, int *out_value) {
    json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) || !json_object_is_type(value, json_type_int)) {
        return false;
    }
    *out_value = json_object_get_int(value);
    return true;
}

bool map_forge_headless_json_get_optional_float(json_object *obj, const char *key, float *out_value) {
    json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) ||
        (!json_object_is_type(value, json_type_double) && !json_object_is_type(value, json_type_int))) {
        return false;
    }
    *out_value = (float)json_object_get_double(value);
    return true;
}
