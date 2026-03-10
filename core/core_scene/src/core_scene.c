/*
 * core_scene.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_scene.h"

#include <stdio.h>
#include <string.h>

static bool path_has_extension(const char *path, const char *ext) {
    if (!path || !ext) return false;
    size_t path_len = strlen(path);
    size_t ext_len = strlen(ext);
    if (path_len < ext_len) return false;
    return strcmp(path + path_len - ext_len, ext) == 0;
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
