#include "app/app_persist_state.h"

#include "core/log.h"
#include "ui/shared_theme_font_adapter.h"

#include <json-c/json.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char *kAppConfigDefaultPath = "config/app.config.json";
static const char *kAppConfigRuntimePath = "data/runtime/app_state.json";

static bool app_ensure_dir_recursive(const char *path) {
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
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return false;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static const char *app_runtime_config_path(void) {
    const char *override_path = getenv("MAPFORGE_RUNTIME_CONFIG_PATH");
    const char *runtime_dir = getenv("MAPFORGE_RUNTIME_DIR");
    static char path_buf[PATH_MAX];
    if (override_path && override_path[0] != '\0') {
        return override_path;
    }
    if (runtime_dir && runtime_dir[0] != '\0') {
        if (snprintf(path_buf, sizeof(path_buf), "%s/app_state.json", runtime_dir) > 0) {
            return path_buf;
        }
    }
    return kAppConfigRuntimePath;
}

static float app_clamp_float(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static uint16_t app_clamp_milli(int value) {
    if (value < 0) {
        return 0u;
    }
    if (value > 1000) {
        return 1000u;
    }
    return (uint16_t)value;
}

static struct json_object *app_load_config_root(void) {
    const char *runtime_path = app_runtime_config_path();
    struct json_object *root = json_object_from_file(runtime_path);
    if (root && json_object_is_type(root, json_type_object)) {
        return root;
    }
    if (root) {
        json_object_put(root);
    }

    root = json_object_from_file(kAppConfigDefaultPath);
    if (root && json_object_is_type(root, json_type_object)) {
        return root;
    }
    if (root) {
        json_object_put(root);
    }
    return NULL;
}

static bool app_ensure_runtime_config_dir(void) {
    const char *runtime_dir = getenv("MAPFORGE_RUNTIME_DIR");
    if (runtime_dir && runtime_dir[0] != '\0') {
        return app_ensure_dir_recursive(runtime_dir);
    }
    if (mkdir("data", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir("data/runtime", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static bool app_json_get_bool(struct json_object *obj, const char *key, bool *out_value) {
    struct json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) || !value) {
        return false;
    }
    if (!json_object_is_type(value, json_type_boolean)) {
        return false;
    }
    *out_value = json_object_get_boolean(value) ? true : false;
    return true;
}

static bool app_json_get_float(struct json_object *obj, const char *key, float *out_value) {
    struct json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) || !value) {
        return false;
    }
    if (!json_object_is_type(value, json_type_double) &&
        !json_object_is_type(value, json_type_int)) {
        return false;
    }
    *out_value = (float)json_object_get_double(value);
    return true;
}

static bool app_json_get_int(struct json_object *obj, const char *key, int *out_value) {
    struct json_object *value = NULL;
    if (!obj || !key || !out_value) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) || !value) {
        return false;
    }
    if (!json_object_is_type(value, json_type_int)) {
        return false;
    }
    *out_value = json_object_get_int(value);
    return true;
}

static bool app_json_get_string(struct json_object *obj, const char *key, char *out_value, size_t out_cap) {
    struct json_object *value = NULL;
    const char *src = NULL;
    if (!obj || !key || !out_value || out_cap == 0u) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) || !value ||
        !json_object_is_type(value, json_type_string)) {
        return false;
    }
    src = json_object_get_string(value);
    if (!src || src[0] == '\0') {
        return false;
    }
    snprintf(out_value, out_cap, "%s", src);
    return true;
}

static bool app_json_get_u16_array(struct json_object *obj, const char *key,
                                   uint16_t *out_values, size_t out_count) {
    struct json_object *value = NULL;
    if (!obj || !key || !out_values || out_count == 0u) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) || !value ||
        !json_object_is_type(value, json_type_array)) {
        return false;
    }
    size_t n = json_object_array_length(value);
    if (n < out_count) {
        return false;
    }
    for (size_t i = 0; i < out_count; ++i) {
        struct json_object *item = json_object_array_get_idx(value, (int)i);
        if (!item || !json_object_is_type(item, json_type_int)) {
            return false;
        }
        out_values[i] = app_clamp_milli(json_object_get_int(item));
    }
    return true;
}

static bool app_json_get_bool_array(struct json_object *obj, const char *key,
                                    bool *out_values, size_t out_count) {
    struct json_object *value = NULL;
    if (!obj || !key || !out_values || out_count == 0u) {
        return false;
    }
    if (!json_object_object_get_ex(obj, key, &value) || !value ||
        !json_object_is_type(value, json_type_array)) {
        return false;
    }
    size_t n = json_object_array_length(value);
    if (n < out_count) {
        return false;
    }
    for (size_t i = 0; i < out_count; ++i) {
        struct json_object *item = json_object_array_get_idx(value, (int)i);
        if (!item || !json_object_is_type(item, json_type_boolean)) {
            return false;
        }
        out_values[i] = json_object_get_boolean(item) ? true : false;
    }
    return true;
}

void app_load_persisted_view_state(AppState *app) {
    if (!app) {
        return;
    }
    struct json_object *root = app_load_config_root();
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) {
            json_object_put(root);
        }
        return;
    }

    struct json_object *view = NULL;
    if (!json_object_object_get_ex(root, "map_view", &view) ||
        !view || !json_object_is_type(view, json_type_object)) {
        json_object_put(root);
        return;
    }

    float zoom = 0.0f;
    if (app_json_get_float(view, "zoom", &zoom)) {
        float clamped_zoom = app_clamp_float(zoom, 10.0f, 18.0f);
        app->view_state_bridge.camera.zoom = clamped_zoom;
        app->view_state_bridge.camera.zoom_target = clamped_zoom;
    }

    bool zoom_logic_enabled = false;
    if (app_json_get_bool(view, "zoom_logic_enabled", &zoom_logic_enabled)) {
        app->view_state_bridge.zoom_logic_enabled = zoom_logic_enabled;
    }
    int text_zoom_step = 0;
    if (app_json_get_int(view, "text_zoom_step", &text_zoom_step)) {
        if (mapforge_shared_font_set_zoom_step(text_zoom_step)) {
            app_apply_shared_ui_font(app);
        }
    }

    bool layer_enabled[TILE_LAYER_COUNT] = {0};
    uint16_t layer_opacity[TILE_LAYER_COUNT] = {0};
    uint16_t layer_fade_start[TILE_LAYER_COUNT] = {0};
    uint16_t layer_fade_speed[TILE_LAYER_COUNT] = {0};
    if (app_json_get_bool_array(view, "layer_enabled", layer_enabled, TILE_LAYER_COUNT)) {
        for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
            app->view_state_bridge.layer_user_enabled[i] = layer_enabled[i];
        }
    }
    if (app_json_get_u16_array(view, "layer_opacity_milli", layer_opacity, TILE_LAYER_COUNT)) {
        bool all_zero = true;
        for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
            if (layer_opacity[i] > 0u) {
                all_zero = false;
                break;
            }
        }
        for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
            app->view_state_bridge.layer_opacity_milli[i] = all_zero ? 1000u : layer_opacity[i];
        }
    }
    if (app_json_get_u16_array(view, "layer_fade_start_milli", layer_fade_start, TILE_LAYER_COUNT)) {
        for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
            app->view_state_bridge.layer_fade_start_milli[i] = layer_fade_start[i];
        }
    }
    if (app_json_get_u16_array(view, "layer_fade_speed_milli", layer_fade_speed, TILE_LAYER_COUNT)) {
        for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
            uint16_t speed = layer_fade_speed[i];
            if (speed < 1u) {
                speed = 1u;
            }
            app->view_state_bridge.layer_fade_speed_milli[i] = speed;
        }
    }

    struct json_object *hardening = NULL;
    if (json_object_object_get_ex(root, "runtime_hardening", &hardening) &&
        hardening && json_object_is_type(hardening, json_type_object)) {
        bool presenter_invariants_enabled = app->tile_state_bridge.presenter_invariants_enabled;
        if (app_json_get_bool(hardening, "presenter_invariants_enabled", &presenter_invariants_enabled)) {
            app->tile_state_bridge.presenter_invariants_enabled = presenter_invariants_enabled;
        }
        bool contour_enabled = app->tile_state_bridge.contour_runtime_enabled;
        if (app_json_get_bool(hardening, "contour_enabled", &contour_enabled)) {
            app->tile_state_bridge.contour_runtime_enabled = contour_enabled;
        }
    }

    struct json_object *data_roots = NULL;
    if (json_object_object_get_ex(root, "data_roots", &data_roots) &&
        data_roots && json_object_is_type(data_roots, json_type_object)) {
        (void)app_json_get_string(data_roots, "input_root", app->input_root, sizeof(app->input_root));
        (void)app_json_get_string(data_roots, "latest_imported_region", app->latest_imported_region, sizeof(app->latest_imported_region));
    }

    json_object_put(root);
}

void app_save_persisted_view_state(const AppState *app) {
    if (!app) {
        return;
    }

    struct json_object *root = app_load_config_root();
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) {
            json_object_put(root);
        }
        root = json_object_new_object();
    }
    if (!root) {
        return;
    }

    struct json_object *view = json_object_new_object();
    if (!view) {
        json_object_put(root);
        return;
    }

    json_object_object_add(view, "zoom", json_object_new_double((double)app->view_state_bridge.camera.zoom_target));
    json_object_object_add(view, "zoom_logic_enabled", json_object_new_boolean(app->view_state_bridge.zoom_logic_enabled ? 1 : 0));
    json_object_object_add(view, "text_zoom_step", json_object_new_int(mapforge_shared_font_zoom_step()));

    struct json_object *enabled_arr = json_object_new_array();
    struct json_object *opacity_arr = json_object_new_array();
    struct json_object *fade_start_arr = json_object_new_array();
    struct json_object *fade_speed_arr = json_object_new_array();
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        json_object_array_add(enabled_arr, json_object_new_boolean(app->view_state_bridge.layer_user_enabled[i] ? 1 : 0));
        json_object_array_add(opacity_arr, json_object_new_int((int)app->view_state_bridge.layer_opacity_milli[i]));
        json_object_array_add(fade_start_arr, json_object_new_int((int)app->view_state_bridge.layer_fade_start_milli[i]));
        json_object_array_add(fade_speed_arr, json_object_new_int((int)app->view_state_bridge.layer_fade_speed_milli[i]));
    }
    json_object_object_add(view, "layer_enabled", enabled_arr);
    json_object_object_add(view, "layer_opacity_milli", opacity_arr);
    json_object_object_add(view, "layer_fade_start_milli", fade_start_arr);
    json_object_object_add(view, "layer_fade_speed_milli", fade_speed_arr);

    json_object_object_add(root, "map_view", view);
    struct json_object *hardening = json_object_new_object();
    if (hardening) {
        json_object_object_add(hardening, "presenter_invariants_enabled",
                               json_object_new_boolean(app->tile_state_bridge.presenter_invariants_enabled ? 1 : 0));
        json_object_object_add(hardening, "contour_enabled",
                               json_object_new_boolean(app->tile_state_bridge.contour_runtime_enabled ? 1 : 0));
        json_object_object_add(root, "runtime_hardening", hardening);
    }

    struct json_object *data_roots = json_object_new_object();
    if (data_roots) {
        json_object_object_add(data_roots, "input_root", json_object_new_string(app->input_root));
        json_object_object_add(data_roots, "latest_imported_region", json_object_new_string(app->latest_imported_region));
        json_object_object_add(root, "data_roots", data_roots);
    }

    const char *runtime_path = app_runtime_config_path();
    if (!app_ensure_runtime_config_dir()) {
        log_error("Failed to create runtime config directory for %s", runtime_path);
        json_object_put(root);
        return;
    }
    if (json_object_to_file_ext(runtime_path, root, JSON_C_TO_STRING_PRETTY) != 0) {
        log_error("Failed to persist map view state to %s", runtime_path);
    }
    json_object_put(root);
}
