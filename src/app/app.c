#include "app/app.h"
#include "app/app_internal.h"

#include "core/log.h"
#include "core/time.h"
#include "app/region_loader.h"
#include "map/road_renderer.h"
#include "route/route_render.h"
#include "ui/font.h"
#include "ui/shared_theme_font_adapter.h"

#include <SDL.h>
#include <SDL2/SDL_ttf.h>
#include <json-c/json.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

static const char *kTraceLaneLifecycle = "lifecycle";
static const char *kAppConfigPath = "config/app.config.json";

static uint32_t app_sum_road_classes(const uint32_t *values, int first_class, int last_class) {
    if (!values || first_class < 0 || last_class < first_class) {
        return 0;
    }
    uint32_t sum = 0;
    for (int i = first_class; i <= last_class; ++i) {
        sum += values[i];
    }
    return sum;
}

static bool app_env_flag_enabled(const char *name) {
    if (!name) {
        return false;
    }
    const char *value = getenv(name);
    if (!value || value[0] == '\0') {
        return false;
    }
    if (strcmp(value, "1") == 0 ||
        strcmp(value, "true") == 0 ||
        strcmp(value, "TRUE") == 0 ||
        strcmp(value, "yes") == 0 ||
        strcmp(value, "YES") == 0 ||
        strcmp(value, "on") == 0 ||
        strcmp(value, "ON") == 0) {
        return true;
    }
    return false;
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

static void app_load_persisted_view_state(AppState *app) {
    if (!app) {
        return;
    }
    struct json_object *root = json_object_from_file(kAppConfigPath);
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

    json_object_put(root);
}

static void app_save_persisted_view_state(const AppState *app) {
    if (!app) {
        return;
    }

    struct json_object *root = json_object_from_file(kAppConfigPath);
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

    if (json_object_to_file_ext(kAppConfigPath, root, JSON_C_TO_STRING_PRETTY) != 0) {
        log_error("Failed to persist map view state to %s", kAppConfigPath);
    }
    json_object_put(root);
}

static int app_find_first_routable_region_index(void) {
    int total = region_count();
    for (int i = 0; i < total; ++i) {
        const RegionInfo *info = region_get(i);
        if (info && region_has_graph(info)) {
            return i;
        }
    }
    return -1;
}

static bool app_trace_ensure_dirs(void) {
    if (mkdir("build", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir("build/traces", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

static void app_trace_emit_frame_samples(AppState *app, double rel_time_s) {
    if (!app || !app->trace_enabled) {
        return;
    }
    core_trace_emit_sample_f32(&app->trace_session, "frame", rel_time_s, (float)app->frame_timings.frame_ms);
    core_trace_emit_sample_f32(&app->trace_session, "events", rel_time_s, (float)app->frame_timings.events_ms);
    core_trace_emit_sample_f32(&app->trace_session, "update", rel_time_s, (float)app->frame_timings.update_ms);
    core_trace_emit_sample_f32(&app->trace_session, "queue", rel_time_s, (float)app->frame_timings.queue_ms);
    core_trace_emit_sample_f32(&app->trace_session, "integrate", rel_time_s, (float)app->frame_timings.integrate_ms);
    core_trace_emit_sample_f32(&app->trace_session, "route", rel_time_s, (float)app->frame_timings.route_ms);
    core_trace_emit_sample_f32(&app->trace_session, "render", rel_time_s, (float)app->frame_timings.render_ms);
    core_trace_emit_sample_f32(&app->trace_session, "present", rel_time_s, (float)app->frame_timings.present_ms);
}

static void app_trace_emit_queue_markers(AppState *app, double rel_time_s) {
    if (!app || !app->trace_enabled) {
        return;
    }

    TileLoaderStats stats = {0};
    tile_loader_get_stats(&app->tile_state_bridge.tile_loader, &stats);
    if (stats.enqueue_drop_count > app->trace_last_tile_enqueue_drop_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "tile_enq_drop");
    }
    if (stats.enqueue_evict_count > app->trace_last_tile_enqueue_evict_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "tile_enq_evict");
    }
    if (stats.result_drop_count > app->trace_last_tile_result_drop_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "tile_res_drop");
    }
    if (stats.result_evict_count > app->trace_last_tile_result_evict_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "tile_res_evict");
    }
    if (app->worker_state_bridge.vk_asset_job_drop_count > app->trace_last_vk_asset_drop_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "vk_job_drop");
    }
    if (app->worker_state_bridge.vk_asset_job_evict_count > app->trace_last_vk_asset_evict_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "vk_job_evict");
    }
    if (app->worker_state_bridge.vk_asset_stage_drop_count > app->trace_last_vk_asset_stage_drop_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "vk_stage_drop");
    }
    if (app->worker_state_bridge.vk_asset_stage_evict_count > app->trace_last_vk_asset_stage_evict_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "vk_stage_evict");
    }

    app->trace_last_tile_enqueue_drop_count = stats.enqueue_drop_count;
    app->trace_last_tile_enqueue_evict_count = stats.enqueue_evict_count;
    app->trace_last_tile_result_drop_count = stats.result_drop_count;
    app->trace_last_tile_result_evict_count = stats.result_evict_count;
    app->trace_last_vk_asset_drop_count = app->worker_state_bridge.vk_asset_job_drop_count;
    app->trace_last_vk_asset_evict_count = app->worker_state_bridge.vk_asset_job_evict_count;
    app->trace_last_vk_asset_stage_drop_count = app->worker_state_bridge.vk_asset_stage_drop_count;
    app->trace_last_vk_asset_stage_evict_count = app->worker_state_bridge.vk_asset_stage_evict_count;
}

static void app_trace_shutdown(AppState *app) {
    if (!app || !app->trace_enabled) {
        return;
    }

    {
        double rel_time_s = time_now_seconds() - app->trace_start_time;
        if (rel_time_s < 0.0) {
            rel_time_s = 0.0;
        }
        core_trace_emit_marker(&app->trace_session, kTraceLaneLifecycle, rel_time_s, "trace_end");
    }

    CoreResult final_result = core_trace_finalize(&app->trace_session);
    if (final_result.code != CORE_OK) {
        log_error("core_trace_finalize failed: %s", final_result.message);
    } else if (!app_trace_ensure_dirs()) {
        log_error("failed to create build/traces directory");
    } else {
        time_t now = time(NULL);
        struct tm local_tm = {0};
        localtime_r(&now, &local_tm);
        char path[256];
        strftime(path, sizeof(path), "build/traces/mapforge_trace_%Y%m%d_%H%M%S.pack", &local_tm);
        CoreResult export_result = core_trace_export_pack(&app->trace_session, path);
        if (export_result.code != CORE_OK) {
            log_error("core_trace_export_pack failed: %s", export_result.message);
        } else {
            log_info("trace exported: %s", path);
        }
    }

    core_trace_session_reset(&app->trace_session);
    app->trace_enabled = false;
}

static bool app_init(AppState *app) {
    if (!app) {
        return false;
    }

    app_worker_contract_init(app);

    mapforge_shared_theme_load_persisted();
    app->width = 1280;
    app->height = 720;
    renderer_set_backend(&app->renderer, RENDERER_BACKEND_SDL);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        log_error("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    const char *backend_env = getenv("MAPFORGE_RENDER_BACKEND");
    if (backend_env && strcmp(backend_env, "vulkan") == 0) {
        renderer_set_backend(&app->renderer, RENDERER_BACKEND_VULKAN);
    }
    log_info("Requested render backend env='%s' resolved='%s'",
             backend_env ? backend_env : "",
             renderer_backend_name(renderer_get_backend(&app->renderer)));

    uint32_t window_flags = SDL_WINDOW_SHOWN;
    if (renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN) {
        window_flags |= SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    }

    app->window = SDL_CreateWindow(
        "MapForge",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        app->width,
        app->height,
        (int)window_flags
    );

    if (!app->window) {
        log_error("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    if (!renderer_init(&app->renderer, app->window, app->width, app->height)) {
        if (renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN) {
            log_error("renderer_init vulkan path failed, retrying SDL fallback: %s", SDL_GetError());
            SDL_DestroyWindow(app->window);
            app->window = SDL_CreateWindow(
                "MapForge",
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                app->width,
                app->height,
                SDL_WINDOW_SHOWN
            );
            if (!app->window) {
                log_error("SDL_CreateWindow fallback failed: %s", SDL_GetError());
                return false;
            }
            renderer_set_backend(&app->renderer, RENDERER_BACKEND_SDL);
            if (!renderer_init(&app->renderer, app->window, app->width, app->height)) {
                log_error("renderer_init SDL fallback failed: %s", SDL_GetError());
                return false;
            }
        } else {
            log_error("renderer_init failed: %s", SDL_GetError());
            return false;
        }
    }
    log_info("Render backend: %s (vulkan_available=%s)",
             renderer_backend_name(renderer_get_backend(&app->renderer)),
             app->renderer.vulkan_available ? "yes" : "no");

    app->tile_state_bridge.vk_assets_enabled = renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN;
    if (!vk_tile_cache_init(&app->tile_state_bridge.vk_tile_cache, 2048)) {
        log_error("vk_tile_cache_init failed");
        return false;
    }
    if (!app_vk_poly_prep_init(app)) {
        log_error("app_vk_poly_prep_init failed");
        return false;
    }
    if (!app_vk_asset_worker_init(app)) {
        log_error("app_vk_asset_worker_init failed");
        return false;
    }
    if (!app_route_worker_init(app)) {
        log_error("app_route_worker_init failed");
        return false;
    }
    if (TTF_Init() != 0) {
        log_error("TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    {
        char shared_font_path[384] = {0};
        int shared_font_size = 0;
        if (mapforge_shared_font_resolve_ui_regular(shared_font_path, sizeof(shared_font_path), &shared_font_size)) {
            ui_font_set(shared_font_path, shared_font_size);
        } else {
            ui_font_set("assets/fonts/Montserrat-Regular.ttf", 10);
        }
    }

    int total_regions = region_count();
    if (total_regions <= 0) {
        log_error("No region configured");
        return false;
    }

    app->region_index = app_find_first_routable_region_index();
    if (app->region_index < 0) {
        app->region_index = 0;
        log_error("No routable region found (missing graph/graph.bin in all regions); route placement is disabled until graph build completes");
    }
    const RegionInfo *info = region_get(app->region_index);
    if (!info) {
        log_error("No region configured");
        return false;
    }

    app->region = *info;
    region_load_meta(info, &app->region);
    if (app->region.tiles_dir[0] == '\0') {
        log_error("Failed to resolve tiles directory for region: %s", app->region.name);
        return false;
    }
    log_info("Region data root: %s", region_data_root());

    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        if (!tile_manager_init(&app->tile_state_bridge.tile_managers[i], 256, app->region.tiles_dir)) {
            log_error("tile_manager_init failed");
            return false;
        }
    }
    if (!tile_loader_init(&app->tile_state_bridge.tile_loader, app->region.tiles_dir)) {
        log_error("tile_loader_init failed");
        return false;
    }
    app->trace_enabled = false;
    CoreTraceConfig trace_cfg = {
        .sample_capacity = APP_TRACE_SAMPLE_CAPACITY,
        .marker_capacity = APP_TRACE_MARKER_CAPACITY
    };
    CoreResult trace_init = core_trace_session_init(&app->trace_session, &trace_cfg);
    if (trace_init.code != CORE_OK) {
        log_error("core_trace_session_init failed: %s", trace_init.message);
    } else {
        TileLoaderStats trace_stats = {0};
        tile_loader_get_stats(&app->tile_state_bridge.tile_loader, &trace_stats);
        app->trace_enabled = true;
        app->trace_start_time = time_now_seconds();
        core_trace_emit_marker(&app->trace_session, kTraceLaneLifecycle, 0.0, "trace_start");
        app->trace_last_tile_enqueue_drop_count = trace_stats.enqueue_drop_count;
        app->trace_last_tile_enqueue_evict_count = trace_stats.enqueue_evict_count;
        app->trace_last_tile_result_drop_count = trace_stats.result_drop_count;
        app->trace_last_tile_result_evict_count = trace_stats.result_evict_count;
        app->trace_last_vk_asset_drop_count = app->worker_state_bridge.vk_asset_job_drop_count;
        app->trace_last_vk_asset_evict_count = app->worker_state_bridge.vk_asset_job_evict_count;
        app->trace_last_vk_asset_stage_drop_count = app->worker_state_bridge.vk_asset_stage_drop_count;
        app->trace_last_vk_asset_stage_evict_count = app->worker_state_bridge.vk_asset_stage_evict_count;
    }

    input_init(&app->ui_state_bridge.input);
    camera_init(&app->view_state_bridge.camera);
    if (app->region.has_center) {
        MercatorMeters center = mercator_from_latlon((LatLon){app->region.center_lat, app->region.center_lon});
        app->view_state_bridge.camera.x = (float)center.x;
        app->view_state_bridge.camera.y = (float)center.y;
    }
    app_center_camera_on_region(&app->view_state_bridge.camera, &app->region, app->width, app->height);
    debug_overlay_init(&app->ui_state_bridge.overlay);
    app->ui_state_bridge.hud_layer_debug_collapsed = false;
    memset(&app->ui_state_bridge.hud_layer_debug_panel_rect, 0, sizeof(app->ui_state_bridge.hud_layer_debug_panel_rect));
    memset(&app->ui_state_bridge.hud_layer_debug_collapse_rect, 0, sizeof(app->ui_state_bridge.hud_layer_debug_collapse_rect));
    memset(&app->ui_state_bridge.hud_layer_debug_handle_rect, 0, sizeof(app->ui_state_bridge.hud_layer_debug_handle_rect));
    app->ui_state_bridge.hud_layer_debug_layout_dirty = true;
    app->ui_state_bridge.hud_layer_debug_layout_hash = 0u;
    app->ui_state_bridge.hud_layer_debug_cached_w = 0.0f;
    app->ui_state_bridge.hud_layer_debug_cached_h = 0.0f;
    app->ui_state_bridge.hud_layer_debug_cached_line_count = 0;
    app->ui_state_bridge.hud_layer_debug_cached_max_text_w = 0;
    app->ui_state_bridge.hud_route_panel_collapsed = false;
    memset(&app->ui_state_bridge.hud_route_panel_rect, 0, sizeof(app->ui_state_bridge.hud_route_panel_rect));
    memset(&app->ui_state_bridge.hud_route_panel_collapse_rect, 0, sizeof(app->ui_state_bridge.hud_route_panel_collapse_rect));
    memset(&app->ui_state_bridge.hud_route_panel_handle_rect, 0, sizeof(app->ui_state_bridge.hud_route_panel_handle_rect));
    memset(&app->ui_state_bridge.hud_route_panel_row_rects, 0, sizeof(app->ui_state_bridge.hud_route_panel_row_rects));
    memset(&app->ui_state_bridge.hud_route_panel_toggle_rects, 0, sizeof(app->ui_state_bridge.hud_route_panel_toggle_rects));
    app->ui_state_bridge.hud_route_panel_layout_dirty = true;
    app->ui_state_bridge.hud_route_panel_layout_hash = 0u;
    app->ui_state_bridge.hud_route_panel_cached_w = 0.0f;
    app->ui_state_bridge.hud_route_panel_cached_h = 0.0f;
    app->ui_state_bridge.hud_route_panel_cached_row_count = 0;
    app->ui_state_bridge.hud_route_panel_cached_max_text_w = 0;
    memset(&app->ui_state_bridge.hud_route_panel_summary_text, 0, sizeof(app->ui_state_bridge.hud_route_panel_summary_text));
    memset(&app->ui_state_bridge.hud_route_panel_row_text, 0, sizeof(app->ui_state_bridge.hud_route_panel_row_text));
    for (uint32_t i = 0; i < ROUTE_ALTERNATIVE_MAX; ++i) {
        app->route_state_bridge.route_alt_visible[i] = true;
    }
    app->single_line = false;
    route_state_init(&app->route_state_bridge.route);
    if (!app_load_route_graph(app)) {
        log_error("Route graph unavailable for startup region '%s'; build graph with: make graph && ./build/tools/mapforge_graph --region %s --osm data/osm_sources/%s.osm --out data/regions/%s",
                  app->region.name, app->region.name, app->region.name, app->region.name);
    }
    app->route_state_bridge.dragging_start = false;
    app->route_state_bridge.dragging_goal = false;
    app->route_state_bridge.has_hover = false;
    memset(&app->route_state_bridge.hover_anchor, 0, sizeof(app->route_state_bridge.hover_anchor));
    memset(&app->route_state_bridge.start_anchor, 0, sizeof(app->route_state_bridge.start_anchor));
    memset(&app->route_state_bridge.goal_anchor, 0, sizeof(app->route_state_bridge.goal_anchor));
    app->route_state_bridge.route_edge_snap_enabled = app_env_flag_enabled("MAPFORGE_ROUTE_EDGE_SNAP");
    app->route_state_bridge.route_edge_snap_debug = app_env_flag_enabled("MAPFORGE_ROUTE_EDGE_SNAP_DEBUG");
    app->route_state_bridge.playback_playing = false;
    app->route_state_bridge.playback_time_s = 0.0f;
    app->route_state_bridge.playback_speed = 1.0f;
    app->view_state_bridge.show_landuse = false;
    app->view_state_bridge.building_zoom_bias = app_building_zoom_bias_for_region(&app->region);
    app->view_state_bridge.building_fill_enabled = true;
    app->view_state_bridge.road_zoom_bias = app_road_zoom_bias_for_region(&app->region);
    app->view_state_bridge.polygon_outline_only = false;
    memset(&app->ui_state_bridge.header_layer_row_rects, 0, sizeof(app->ui_state_bridge.header_layer_row_rects));
    memset(&app->ui_state_bridge.header_layer_label_rects, 0, sizeof(app->ui_state_bridge.header_layer_label_rects));
    memset(&app->ui_state_bridge.header_layer_toggle_rects, 0, sizeof(app->ui_state_bridge.header_layer_toggle_rects));
    memset(&app->ui_state_bridge.header_zoom_toggle_rect, 0, sizeof(app->ui_state_bridge.header_zoom_toggle_rect));
    memset(&app->ui_state_bridge.header_layer_opacity_panel_rect, 0, sizeof(app->ui_state_bridge.header_layer_opacity_panel_rect));
    memset(&app->ui_state_bridge.header_layer_opacity_track_rect, 0, sizeof(app->ui_state_bridge.header_layer_opacity_track_rect));
    memset(&app->ui_state_bridge.header_layer_fade_panel_rect, 0, sizeof(app->ui_state_bridge.header_layer_fade_panel_rect));
    memset(&app->ui_state_bridge.header_layer_fade_start_track_rect, 0, sizeof(app->ui_state_bridge.header_layer_fade_start_track_rect));
    memset(&app->ui_state_bridge.header_layer_fade_speed_track_rect, 0, sizeof(app->ui_state_bridge.header_layer_fade_speed_track_rect));
    app->ui_state_bridge.header_layer_opacity_dragging = false;
    app->ui_state_bridge.header_layer_fade_drag_target = 0;
    app->ui_state_bridge.header_layer_panel_mode = 0;
    app->ui_state_bridge.header_layer_selected_valid = false;
    app->ui_state_bridge.header_layer_selected_kind = TILE_LAYER_ROAD_ARTERY;
    app->view_state_bridge.zoom_logic_enabled = true;
    app->tile_state_bridge.presenter_invariants_enabled = !app_env_flag_enabled("MAPFORGE_DISABLE_PRESENTER_INVARIANTS");
    app->tile_state_bridge.contour_runtime_enabled = app_env_flag_enabled("MAPFORGE_ENABLE_CONTOUR");
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        memset(&app->tile_state_bridge.tile_queues[i], 0, sizeof(app->tile_state_bridge.tile_queues[i]));
        app->view_state_bridge.layer_user_enabled[i] = true;
        app->view_state_bridge.layer_opacity_milli[i] = 1000u;
        float zoom_start = app_layer_zoom_start(app, (TileLayerKind)i);
        if (zoom_start < 0.0f) {
            zoom_start = 0.0f;
        }
        if (zoom_start > 20.0f) {
            zoom_start = 20.0f;
        }
        app->view_state_bridge.layer_fade_start_milli[i] = (uint16_t)(zoom_start * 50.0f);
        app->view_state_bridge.layer_fade_speed_milli[i] = 170u;
        app->tile_state_bridge.queue_band[i] = TILE_BAND_DEFAULT;
        app->tile_state_bridge.previous_target_band[i] = TILE_BAND_DEFAULT;
        app->tile_state_bridge.stable_target_band[i] = TILE_BAND_DEFAULT;
        app->tile_state_bridge.layer_target_band[i] = TILE_BAND_DEFAULT;
        app->tile_state_bridge.layer_band_last_change_time[i] = 0.0;
    }
    app->tile_state_bridge.queue_valid = false;
    app->tile_state_bridge.visible_valid = false;
    app->tile_state_bridge.loading_expected = 0;
    app->tile_state_bridge.loading_done = 0;
    app->tile_state_bridge.loading_no_data_time = 0.0f;
    app->tile_state_bridge.loading_layer_index = 0;
    app->tile_state_bridge.draw_path_vk_count = 0u;
    app->tile_state_bridge.draw_path_fallback_count = 0u;
    app->tile_state_bridge.band_switch_deferred_count = 0u;
    app->tile_state_bridge.queue_rebuild_deferred_count = 0u;
    app->tile_state_bridge.transition_blend_draw_count = 0u;
    app->tile_state_bridge.present_hold_hits = 0u;
    app->tile_state_bridge.present_hold_misses = 0u;
    app->tile_state_bridge.present_hold_updates = 0u;
    app->tile_state_bridge.present_hold_tick = 1u;
    app->tile_state_bridge.last_queue_rebuild_time = 0.0;
    app_load_persisted_view_state(app);
    app_refresh_layer_states(app);
    app_bridge_sync_from_legacy(app);

    return true;
}

static void app_shutdown(AppState *app) {
    if (!app) {
        return;
    }

    app_bridge_sync_to_legacy(app);
    app_save_persisted_view_state(app);
    mapforge_shared_theme_save_persisted();

    if (TTF_WasInit()) {
        ui_font_shutdown(&app->renderer);
        TTF_Quit();
    }

    // Stop producers/consumers first so no background path can race tile ownership at teardown.
    app_route_worker_shutdown(app);
    app_vk_asset_worker_shutdown(app);
    app_vk_poly_prep_shutdown(app);
    tile_loader_shutdown(&app->tile_state_bridge.tile_loader);
    app_clear_tile_queue(app);

    vk_tile_cache_clear_with_renderer(&app->tile_state_bridge.vk_tile_cache, app->renderer.vk);
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        tile_manager_shutdown(&app->tile_state_bridge.tile_managers[i]);
    }
    app_route_release_snap_index(app);
    app_trace_shutdown(app);
    vk_tile_cache_shutdown(&app->tile_state_bridge.vk_tile_cache);
    route_state_shutdown(&app->route_state_bridge.route);
    renderer_shutdown(&app->renderer);

    if (app->window) {
        SDL_DestroyWindow(app->window);
        app->window = NULL;
    }

    SDL_Quit();
}

int app_run(void) {
    AppState app = {0};
    if (!app_init(&app)) {
        app_shutdown(&app);
        return 1;
    }

    double last_time = time_now_seconds();
    double perf_next_log = last_time + 1.0;
    uint32_t last_loading_done = 0;
    double last_loading_progress_time = last_time;
    RendererBackend last_backend = renderer_get_backend(&app.renderer);
    bool vk_debug_logs = app_env_flag_enabled("MAPFORGE_VK_DEBUG");

    while (!app.ui_state_bridge.input.quit) {
        double frame_begin = 0.0;
        double after_events = 0.0;
        app_runtime_begin_frame(&app, &frame_begin, &after_events);

        if (app_runtime_handle_global_controls(&app)) {
            continue;
        }

        float dt = 0.0f;
        double after_update = 0.0;
        double after_queue = 0.0;
        double after_integrate = 0.0;
        double after_route = 0.0;
        app_runtime_update_frame(&app, &last_time, &dt, &after_update, &after_queue, &after_integrate, &after_route);

        uint32_t visible_tiles = 0;
        double before_present = 0.0;
        double after_render = 0.0;
        if (app.tile_state_bridge.loading_expected > 0 && app.tile_state_bridge.loading_done == 0) {
            app.tile_state_bridge.loading_no_data_time += dt;
        } else {
            app.tile_state_bridge.loading_no_data_time = 0.0f;
        }
        app_runtime_render_frame(&app, &last_backend, &visible_tiles, &before_present, &after_render);
        RoadRenderStats road_stats = {0};
        road_renderer_stats_get(&road_stats);

        if (app.tile_state_bridge.loading_done != last_loading_done) {
            last_loading_done = app.tile_state_bridge.loading_done;
            last_loading_progress_time = after_render;
        }

        app.frame_timings.frame_ms = (after_render - frame_begin) * 1000.0;
        app.frame_timings.events_ms = (after_events - frame_begin) * 1000.0;
        app.frame_timings.update_ms = (after_update - after_events) * 1000.0;
        app.frame_timings.queue_ms = (after_queue - after_update) * 1000.0;
        app.frame_timings.integrate_ms = (after_integrate - after_queue) * 1000.0;
        app.frame_timings.route_ms = (after_route - after_integrate) * 1000.0;
        app.frame_timings.render_ms = (before_present - after_route) * 1000.0;
        app.frame_timings.present_ms = (after_render - before_present) * 1000.0;
        if (app.trace_enabled) {
            double rel_time_s = after_render - app.trace_start_time;
            app_trace_emit_frame_samples(&app, rel_time_s);
            app_trace_emit_queue_markers(&app, rel_time_s);
        }
        double frame_ms = app.frame_timings.frame_ms;
        double events_ms = app.frame_timings.events_ms;
        bool long_frame = frame_ms >= 120.0;
        bool stuck_loading = app.tile_state_bridge.loading_expected > 0 &&
            app.tile_state_bridge.loading_done < app.tile_state_bridge.loading_expected &&
            (after_render - last_loading_progress_time) >= 1.5;
        if (vk_debug_logs && (long_frame || stuck_loading || after_render >= perf_next_log)) {
            TileLoaderStats stats = {0};
            tile_loader_get_stats(&app.tile_state_bridge.tile_loader, &stats);
            if (renderer_get_backend(&app.renderer) == RENDERER_BACKEND_VULKAN) {
                VkTileCacheStats vk_asset_stats = {0};
                VkPolyPrepStats poly_prep_stats = {0};
                vk_tile_cache_get_stats(&app.tile_state_bridge.vk_tile_cache, &vk_asset_stats);
                app_vk_poly_prep_get_stats(&app, &poly_prep_stats);
                uint32_t drawn_major = app_sum_road_classes(road_stats.drawn_by_class, ROAD_CLASS_MOTORWAY, ROAD_CLASS_TERTIARY);
                uint32_t drawn_local = app_sum_road_classes(road_stats.drawn_by_class, ROAD_CLASS_RESIDENTIAL, ROAD_CLASS_SERVICE);
                uint32_t drawn_path = app_sum_road_classes(road_stats.drawn_by_class, ROAD_CLASS_FOOTWAY, ROAD_CLASS_PATH);
                uint32_t filt_major = app_sum_road_classes(road_stats.filtered_by_class, ROAD_CLASS_MOTORWAY, ROAD_CLASS_TERTIARY);
                uint32_t filt_local = app_sum_road_classes(road_stats.filtered_by_class, ROAD_CLASS_RESIDENTIAL, ROAD_CLASS_SERVICE);
                uint32_t filt_path = app_sum_road_classes(road_stats.filtered_by_class, ROAD_CLASS_FOOTWAY, ROAD_CLASS_PATH);
                log_info("perf region=%s backend=vk frame=%.1fms events=%.1f update=%.1f queue=%.1f integrate=%.1f route=%.1f render=%.1f present=%.1f zoom=%.2f vis=%u load=%u/%u active=%s "
                         "band_target(a=%s l=%s) band_vis(c=%u/%u m=%u/%u f=%u/%u d=%u/%u) band_q(c=%u m=%u f=%u d=%u) band_fallback=%u "
                         "req=%u/%u res=%u/%u enq=%llu drop=%llu evict=%llu out=%llu out_drop=%llu out_evict=%llu miss=%llu ok=%llu fail=%llu "
                         "vk_begin=%d vk_begin_fail_total=%llu vk_recreate=%u vk_geom=%u/%u vk_geom_skip=%u vk_lines=%u vk_line_skip=%u vk_line_budget=%u vk_rect=%u vk_fill=%u "
                         "vk_assets=%u/%u builds=%u evict=%u miss=%u jobs(q=%u build=%llu drop=%llu evict=%llu) poly_prep(in=%u out=%u enq=%llu done=%llu drop=%llu) resident(a=%u l=%u) fill_resident(w=%u p=%u l=%u b=%u) "
                         "mesh(v=%llu b=%llu fail=%u fill_fail=%u) vk_poly_fill(draw=%u skip=%u fail=%u idx=%u) "
                         "road_draw(m=%u l=%u p=%u) road_filter(m=%u l=%u p=%u) draw_path(vk=%u fallback=%u blend=%u) defer(band=%u queue=%u) hold(hit=%u miss=%u upd=%u)",
                         app.region.name,
                         frame_ms, events_ms, app.frame_timings.update_ms,
                         app.frame_timings.queue_ms, app.frame_timings.integrate_ms,
                         app.frame_timings.route_ms, app.frame_timings.render_ms, app.frame_timings.present_ms,
                         app.view_state_bridge.camera.zoom,
                         app.tile_state_bridge.visible_tile_count,
                         app.tile_state_bridge.loading_done, app.tile_state_bridge.loading_expected,
                         app.tile_state_bridge.active_layer_valid ? layer_policy_label(app.tile_state_bridge.active_layer_kind) : "none",
                         layer_policy_band_label(app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_ARTERY]),
                         layer_policy_band_label(app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_LOCAL]),
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_COARSE], app.tile_state_bridge.band_visible_expected[TILE_BAND_COARSE],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_MID], app.tile_state_bridge.band_visible_expected[TILE_BAND_MID],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_FINE], app.tile_state_bridge.band_visible_expected[TILE_BAND_FINE],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_DEFAULT], app.tile_state_bridge.band_visible_expected[TILE_BAND_DEFAULT],
                         app.tile_state_bridge.band_queue_depth[TILE_BAND_COARSE], app.tile_state_bridge.band_queue_depth[TILE_BAND_MID],
                         app.tile_state_bridge.band_queue_depth[TILE_BAND_FINE], app.tile_state_bridge.band_queue_depth[TILE_BAND_DEFAULT],
                         app.tile_state_bridge.vk_road_band_fallback_draws,
                         stats.req_count, stats.req_capacity,
                         stats.res_count, stats.res_capacity,
                         (unsigned long long)stats.enqueued_count,
                         (unsigned long long)stats.enqueue_drop_count,
                         (unsigned long long)stats.enqueue_evict_count,
                         (unsigned long long)stats.produced_count,
                         (unsigned long long)stats.result_drop_count,
                         (unsigned long long)stats.result_evict_count,
                         (unsigned long long)stats.missing_count,
                         (unsigned long long)stats.load_ok_count,
                         (unsigned long long)stats.load_fail_count,
                         app.renderer.vk_last_begin_result,
                         (unsigned long long)app.renderer.vk_begin_failures_total,
                         app.renderer.vk_swapchain_recreates,
                         app.renderer.vk_geom_used,
                         app.renderer.vk_geom_budget,
                         app.renderer.vk_geom_budget_skips,
                         app.renderer.vk_lines_drawn,
                         app.renderer.vk_line_budget_skips,
                         app.renderer.vk_line_budget,
                         app.renderer.vk_rects_drawn,
                         app.renderer.vk_rects_filled,
                         vk_asset_stats.count,
                         vk_asset_stats.capacity,
                         vk_asset_stats.builds,
                         vk_asset_stats.evictions,
                         app.tile_state_bridge.vk_asset_misses,
                         app.worker_state_bridge.vk_asset_job_count,
                         (unsigned long long)app.worker_state_bridge.vk_asset_job_build_count,
                         (unsigned long long)app.worker_state_bridge.vk_asset_job_drop_count,
                         (unsigned long long)app.worker_state_bridge.vk_asset_job_evict_count,
                         poly_prep_stats.in_count,
                         poly_prep_stats.out_count,
                         (unsigned long long)poly_prep_stats.enqueued_count,
                         (unsigned long long)poly_prep_stats.done_count,
                         (unsigned long long)poly_prep_stats.drop_count,
                         vk_asset_stats.resident_artery,
                         vk_asset_stats.resident_local,
                         vk_asset_stats.resident_fill_water,
                         vk_asset_stats.resident_fill_park,
                         vk_asset_stats.resident_fill_landuse,
                         vk_asset_stats.resident_fill_building,
                         (unsigned long long)vk_asset_stats.mesh_vertices,
                         (unsigned long long)vk_asset_stats.mesh_bytes,
                         vk_asset_stats.mesh_build_failures,
                         vk_asset_stats.fill_mesh_build_failures,
                         app.tile_state_bridge.vk_poly_fill_drawn,
                         app.tile_state_bridge.vk_poly_fill_skip,
                         app.tile_state_bridge.vk_poly_fill_fail,
                         app.tile_state_bridge.vk_poly_fill_indices,
                         drawn_major, drawn_local, drawn_path,
                         filt_major, filt_local, filt_path,
                         app.tile_state_bridge.draw_path_vk_count,
                         app.tile_state_bridge.draw_path_fallback_count,
                         app.tile_state_bridge.transition_blend_draw_count,
                         app.tile_state_bridge.band_switch_deferred_count,
                         app.tile_state_bridge.queue_rebuild_deferred_count,
                         app.tile_state_bridge.present_hold_hits,
                         app.tile_state_bridge.present_hold_misses,
                         app.tile_state_bridge.present_hold_updates);
            } else {
                log_info("perf region=%s backend=sdl frame=%.1fms events=%.1f update=%.1f queue=%.1f integrate=%.1f route=%.1f render=%.1f present=%.1f zoom=%.2f vis=%u load=%u/%u active=%s "
                         "band_target(a=%s l=%s) band_vis(c=%u/%u m=%u/%u f=%u/%u d=%u/%u) band_q(c=%u m=%u f=%u d=%u) band_fallback=%u "
                         "req=%u/%u res=%u/%u enq=%llu drop=%llu evict=%llu out=%llu out_drop=%llu out_evict=%llu miss=%llu ok=%llu fail=%llu "
                         "draw_path(vk=%u fallback=%u blend=%u) defer(band=%u queue=%u) hold(hit=%u miss=%u upd=%u)",
                         app.region.name,
                         frame_ms, events_ms, app.frame_timings.update_ms,
                         app.frame_timings.queue_ms, app.frame_timings.integrate_ms,
                         app.frame_timings.route_ms, app.frame_timings.render_ms, app.frame_timings.present_ms,
                         app.view_state_bridge.camera.zoom,
                         app.tile_state_bridge.visible_tile_count,
                         app.tile_state_bridge.loading_done, app.tile_state_bridge.loading_expected,
                         app.tile_state_bridge.active_layer_valid ? layer_policy_label(app.tile_state_bridge.active_layer_kind) : "none",
                         layer_policy_band_label(app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_ARTERY]),
                         layer_policy_band_label(app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_LOCAL]),
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_COARSE], app.tile_state_bridge.band_visible_expected[TILE_BAND_COARSE],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_MID], app.tile_state_bridge.band_visible_expected[TILE_BAND_MID],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_FINE], app.tile_state_bridge.band_visible_expected[TILE_BAND_FINE],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_DEFAULT], app.tile_state_bridge.band_visible_expected[TILE_BAND_DEFAULT],
                         app.tile_state_bridge.band_queue_depth[TILE_BAND_COARSE], app.tile_state_bridge.band_queue_depth[TILE_BAND_MID],
                         app.tile_state_bridge.band_queue_depth[TILE_BAND_FINE], app.tile_state_bridge.band_queue_depth[TILE_BAND_DEFAULT],
                         app.tile_state_bridge.vk_road_band_fallback_draws,
                         stats.req_count, stats.req_capacity,
                         stats.res_count, stats.res_capacity,
                         (unsigned long long)stats.enqueued_count,
                         (unsigned long long)stats.enqueue_drop_count,
                         (unsigned long long)stats.enqueue_evict_count,
                         (unsigned long long)stats.produced_count,
                         (unsigned long long)stats.result_drop_count,
                         (unsigned long long)stats.result_evict_count,
                         (unsigned long long)stats.missing_count,
                         (unsigned long long)stats.load_ok_count,
                         (unsigned long long)stats.load_fail_count,
                         app.tile_state_bridge.draw_path_vk_count,
                         app.tile_state_bridge.draw_path_fallback_count,
                         app.tile_state_bridge.transition_blend_draw_count,
                         app.tile_state_bridge.band_switch_deferred_count,
                         app.tile_state_bridge.queue_rebuild_deferred_count,
                         app.tile_state_bridge.present_hold_hits,
                         app.tile_state_bridge.present_hold_misses,
                         app.tile_state_bridge.present_hold_updates);
            }
            perf_next_log = after_render + 1.0;
        }
    }

    app_shutdown(&app);
    return 0;
}
