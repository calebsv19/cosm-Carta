#ifndef CORE_SCENE_OVERLAY_MERGE_SHARED_H
#define CORE_SCENE_OVERLAY_MERGE_SHARED_H

#include <json-c/json.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct CoreSceneOverlayMeta {
    const char *producer;
    long long logical_clock;
} CoreSceneOverlayMeta;

static void core_scene_overlay_diag(char *out_diagnostics,
                                    size_t out_diagnostics_size,
                                    const char *message) {
    if (!out_diagnostics || out_diagnostics_size == 0 || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static bool core_scene_overlay_deep_copy_assign(json_object *target,
                                                const char *key,
                                                json_object *src_value) {
    json_object *copy = NULL;
    const char *serialized = NULL;
    if (!target || !key || !src_value) return false;
    serialized = json_object_to_json_string_ext(src_value, JSON_C_TO_STRING_PLAIN);
    if (!serialized) return false;
    copy = json_tokener_parse(serialized);
    if (!copy) return false;
    json_object_object_del(target, key);
    json_object_object_add(target, key, copy);
    return true;
}

static bool core_scene_overlay_ensure_object_child(json_object *parent,
                                                   const char *key,
                                                   json_object **out_child) {
    json_object *child = NULL;
    if (!parent || !key || !out_child) return false;
    if (json_object_object_get_ex(parent, key, &child)) {
        if (!json_object_is_type(child, json_type_object)) return false;
        *out_child = child;
        return true;
    }
    child = json_object_new_object();
    if (!child) return false;
    json_object_object_add(parent, key, child);
    *out_child = child;
    return true;
}

static bool core_scene_overlay_validate_space_mode_value(json_object *space_mode_obj) {
    const char *mode = NULL;
    if (!space_mode_obj || !json_object_is_type(space_mode_obj, json_type_string)) return false;
    mode = json_object_get_string(space_mode_obj);
    return mode && (strcmp(mode, "2d") == 0 || strcmp(mode, "3d") == 0);
}

static bool core_scene_overlay_parse_meta(json_object *overlay_root,
                                          const char *expected_producer,
                                          CoreSceneOverlayMeta *out_meta,
                                          char *out_diagnostics,
                                          size_t out_diagnostics_size) {
    json_object *overlay_meta = NULL;
    json_object *producer = NULL;
    json_object *logical_clock = NULL;
    const char *producer_str = NULL;
    long long logical_clock_val = 0;

    if (!overlay_root || !expected_producer || !out_meta) return false;
    memset(out_meta, 0, sizeof(*out_meta));

    if (!json_object_object_get_ex(overlay_root, "overlay_meta", &overlay_meta) ||
        !json_object_is_type(overlay_meta, json_type_object)) {
        core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "overlay_meta object is required");
        return false;
    }
    if (!json_object_object_get_ex(overlay_meta, "producer", &producer) ||
        !json_object_is_type(producer, json_type_string)) {
        core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "overlay_meta.producer must be string");
        return false;
    }
    producer_str = json_object_get_string(producer);
    if (!producer_str || strcmp(producer_str, expected_producer) != 0) {
        core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "overlay_meta.producer not allowed for this bridge");
        return false;
    }
    if (!json_object_object_get_ex(overlay_meta, "logical_clock", &logical_clock) ||
        !json_object_is_type(logical_clock, json_type_int)) {
        core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "overlay_meta.logical_clock must be integer");
        return false;
    }
    logical_clock_val = json_object_get_int64(logical_clock);
    if (logical_clock_val < 0) {
        core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "overlay_meta.logical_clock must be >= 0");
        return false;
    }

    out_meta->producer = producer_str;
    out_meta->logical_clock = logical_clock_val;
    return true;
}

static bool core_scene_overlay_has_forbidden_top_level_key(json_object *overlay_root,
                                                           const char **out_key) {
    static const char *k_allowed[] = {
        "overlay_meta",
        "extensions",
        "space_mode_default"
    };
    json_object_object_foreach(overlay_root, key, value) {
        bool allowed = false;
        size_t i;
        (void)value;
        for (i = 0; i < sizeof(k_allowed) / sizeof(k_allowed[0]); ++i) {
            if (strcmp(key, k_allowed[i]) == 0) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            if (out_key) *out_key = key;
            return true;
        }
    }
    return false;
}

static bool core_scene_overlay_validate_extensions_only_app(json_object *extensions_obj,
                                                            const char *app_namespace,
                                                            const char **out_bad_key,
                                                            const char **out_reason) {
    json_object_object_foreach(extensions_obj, key, value) {
        if (strcmp(key, app_namespace) != 0) {
            if (out_bad_key) *out_bad_key = key;
            if (out_reason) *out_reason = "namespace";
            return false;
        }
        if (!json_object_is_type(value, json_type_object)) {
            if (out_bad_key) *out_bad_key = key;
            if (out_reason) *out_reason = "payload_type";
            return false;
        }
    }
    return true;
}

static bool core_scene_overlay_passes_producer_clock_guard(json_object *runtime_root,
                                                           const CoreSceneOverlayMeta *meta,
                                                           char *out_diagnostics,
                                                           size_t out_diagnostics_size) {
    json_object *extensions = NULL;
    json_object *overlay_merge = NULL;
    json_object *producer_clocks = NULL;
    json_object *current_clock_obj = NULL;
    long long current_clock = -1;
    if (!runtime_root || !meta || !meta->producer) return false;

    if (json_object_object_get_ex(runtime_root, "extensions", &extensions) &&
        json_object_is_type(extensions, json_type_object) &&
        json_object_object_get_ex(extensions, "overlay_merge", &overlay_merge) &&
        json_object_is_type(overlay_merge, json_type_object) &&
        json_object_object_get_ex(overlay_merge, "producer_clocks", &producer_clocks) &&
        json_object_is_type(producer_clocks, json_type_object) &&
        json_object_object_get_ex(producer_clocks, meta->producer, &current_clock_obj) &&
        json_object_is_type(current_clock_obj, json_type_int)) {
        current_clock = json_object_get_int64(current_clock_obj);
    }

    if (current_clock >= 0 && meta->logical_clock <= current_clock) {
        core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "overlay logical_clock is stale for producer");
        return false;
    }
    return true;
}

static bool core_scene_overlay_apply_space_mode_policy(json_object *runtime_root,
                                                       json_object *overlay_space_mode,
                                                       const CoreSceneOverlayMeta *meta,
                                                       char *out_diagnostics,
                                                       size_t out_diagnostics_size) {
    json_object *extensions = NULL;
    json_object *overlay_merge = NULL;
    json_object *space_mode_state = NULL;
    json_object *current_clock_obj = NULL;
    json_object *current_producer_obj = NULL;
    long long current_clock = -1;
    const char *current_producer = NULL;

    if (!runtime_root || !overlay_space_mode || !meta || !meta->producer) return false;

    if (json_object_object_get_ex(runtime_root, "extensions", &extensions) &&
        json_object_is_type(extensions, json_type_object) &&
        json_object_object_get_ex(extensions, "overlay_merge", &overlay_merge) &&
        json_object_is_type(overlay_merge, json_type_object) &&
        json_object_object_get_ex(overlay_merge, "space_mode_default", &space_mode_state) &&
        json_object_is_type(space_mode_state, json_type_object)) {
        if (json_object_object_get_ex(space_mode_state, "logical_clock", &current_clock_obj) &&
            json_object_is_type(current_clock_obj, json_type_int)) {
            current_clock = json_object_get_int64(current_clock_obj);
        }
        if (json_object_object_get_ex(space_mode_state, "producer", &current_producer_obj) &&
            json_object_is_type(current_producer_obj, json_type_string)) {
            current_producer = json_object_get_string(current_producer_obj);
        }
    }

    if (current_clock >= 0) {
        if (meta->logical_clock < current_clock) {
            core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "overlay conflict: stale logical_clock for space_mode_default");
            return false;
        }
        if (meta->logical_clock == current_clock &&
            current_producer &&
            strcmp(meta->producer, current_producer) > 0) {
            core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "overlay conflict: tie-break lost for space_mode_default");
            return false;
        }
    }

    if (!core_scene_overlay_deep_copy_assign(runtime_root, "space_mode_default", overlay_space_mode)) {
        core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "failed to apply space_mode_default");
        return false;
    }
    if (!core_scene_overlay_ensure_object_child(runtime_root, "extensions", &extensions)) return false;
    if (!core_scene_overlay_ensure_object_child(extensions, "overlay_merge", &overlay_merge)) return false;
    if (!core_scene_overlay_ensure_object_child(overlay_merge, "space_mode_default", &space_mode_state)) return false;

    json_object_object_del(space_mode_state, "producer");
    json_object_object_add(space_mode_state, "producer", json_object_new_string(meta->producer));
    json_object_object_del(space_mode_state, "logical_clock");
    json_object_object_add(space_mode_state, "logical_clock", json_object_new_int64(meta->logical_clock));
    return true;
}

static bool core_scene_overlay_record_producer_clock(json_object *runtime_root,
                                                     const CoreSceneOverlayMeta *meta) {
    json_object *extensions = NULL;
    json_object *overlay_merge = NULL;
    json_object *producer_clocks = NULL;
    if (!runtime_root || !meta || !meta->producer) return false;
    if (!core_scene_overlay_ensure_object_child(runtime_root, "extensions", &extensions)) return false;
    if (!core_scene_overlay_ensure_object_child(extensions, "overlay_merge", &overlay_merge)) return false;
    if (!core_scene_overlay_ensure_object_child(overlay_merge, "producer_clocks", &producer_clocks)) return false;
    json_object_object_del(producer_clocks, meta->producer);
    json_object_object_add(producer_clocks, meta->producer, json_object_new_int64(meta->logical_clock));
    return true;
}

static bool core_scene_overlay_merge_apply(json_object *runtime_root,
                                           json_object *overlay_root,
                                           const char *app_namespace,
                                           const char *expected_producer,
                                           char *out_diagnostics,
                                           size_t out_diagnostics_size) {
    CoreSceneOverlayMeta meta;
    json_object *overlay_extensions = NULL;
    json_object *overlay_app_extension = NULL;
    json_object *overlay_space_mode = NULL;
    json_object *runtime_extensions = NULL;
    const char *forbidden_key = NULL;
    const char *bad_extension_key = NULL;
    const char *bad_extension_reason = NULL;

    if (!runtime_root || !overlay_root || !app_namespace || !expected_producer) {
        core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "invalid input");
        return false;
    }
    if (!core_scene_overlay_parse_meta(overlay_root,
                                       expected_producer,
                                       &meta,
                                       out_diagnostics,
                                       out_diagnostics_size)) {
        return false;
    }
    if (!core_scene_overlay_passes_producer_clock_guard(runtime_root, &meta, out_diagnostics, out_diagnostics_size)) {
        return false;
    }
    if (core_scene_overlay_has_forbidden_top_level_key(overlay_root, &forbidden_key)) {
        char tmp[192];
        snprintf(tmp, sizeof(tmp), "overlay key not allowed: %s", forbidden_key ? forbidden_key : "(unknown)");
        core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, tmp);
        return false;
    }

    if (json_object_object_get_ex(overlay_root, "extensions", &overlay_extensions)) {
        if (!json_object_is_type(overlay_extensions, json_type_object)) {
            core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "overlay extensions must be object");
            return false;
        }
        if (!core_scene_overlay_validate_extensions_only_app(overlay_extensions,
                                                             app_namespace,
                                                             &bad_extension_key,
                                                             &bad_extension_reason)) {
            char tmp[192];
            if (bad_extension_reason && strcmp(bad_extension_reason, "payload_type") == 0) {
                snprintf(tmp, sizeof(tmp), "overlay extension payload must be object: %s",
                         bad_extension_key ? bad_extension_key : "(unknown)");
            } else {
                snprintf(tmp, sizeof(tmp), "overlay extension namespace not allowed: %s",
                         bad_extension_key ? bad_extension_key : "(unknown)");
            }
            core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, tmp);
            return false;
        }
    }

    if (json_object_object_get_ex(overlay_root, "space_mode_default", &overlay_space_mode)) {
        if (!core_scene_overlay_validate_space_mode_value(overlay_space_mode)) {
            core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "space_mode_default must be '2d' or '3d'");
            return false;
        }
        if (!core_scene_overlay_apply_space_mode_policy(runtime_root,
                                                        overlay_space_mode,
                                                        &meta,
                                                        out_diagnostics,
                                                        out_diagnostics_size)) {
            return false;
        }
    }

    if (json_object_object_get_ex(overlay_root, "extensions", &overlay_extensions)) {
        if (!json_object_object_get_ex(runtime_root, "extensions", &runtime_extensions) ||
            !json_object_is_type(runtime_extensions, json_type_object)) {
            runtime_extensions = json_object_new_object();
            if (!runtime_extensions) {
                core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "failed to create runtime extensions");
                return false;
            }
            json_object_object_add(runtime_root, "extensions", runtime_extensions);
        }
        if (json_object_object_get_ex(overlay_extensions, app_namespace, &overlay_app_extension)) {
            if (!core_scene_overlay_deep_copy_assign(runtime_extensions, app_namespace, overlay_app_extension)) {
                char tmp[192];
                snprintf(tmp, sizeof(tmp), "failed to apply %s extension", app_namespace);
                core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, tmp);
                return false;
            }
        }
    }

    if (!core_scene_overlay_record_producer_clock(runtime_root, &meta)) {
        core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "failed to persist overlay merge metadata");
        return false;
    }

    core_scene_overlay_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

#endif
