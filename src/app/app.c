#include "app/app.h"
#include "app/app_internal.h"
#include "app/app_persist_state.h"
#include "app/app_trace_runtime.h"
#include "map_forge/map_forge_app_main.h"

#include "core/log.h"
#include "core/time.h"
#include "app/region_loader.h"
#include "map/road_renderer.h"
#include "route/route_render.h"
#include "ui/font.h"
#include "ui/shared_theme_font_adapter.h"
#include "kit_runtime_diag.h"

#include <SDL.h>
#include <SDL2/SDL_ttf.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

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

static double app_env_double_clamped(const char *name, double fallback, double min_value, double max_value) {
    if (!name) {
        return fallback;
    }
    const char *value = getenv(name);
    if (!value || value[0] == '\0') {
        return fallback;
    }
    char *end = NULL;
    double parsed = strtod(value, &end);
    if (end == value) {
        return fallback;
    }
    if (parsed < min_value) {
        parsed = min_value;
    }
    if (parsed > max_value) {
        parsed = max_value;
    }
    return parsed;
}

static void app_default_input_root(char *out_path, size_t out_cap) {
    const char *home = getenv("HOME");
    if (!out_path || out_cap == 0u) {
        return;
    }
    if (home && home[0] != '\0') {
        snprintf(out_path, out_cap, "%s/Desktop", home);
        return;
    }
    snprintf(out_path, out_cap, ".");
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

static int app_find_region_index_by_name(const char *name) {
    if (!name || name[0] == '\0') {
        return -1;
    }
    int total = region_count();
    for (int i = 0; i < total; ++i) {
        const RegionInfo *info = region_get(i);
        if (info && info->name && strcmp(info->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static bool app_init(AppState *app) {
    if (!app) {
        return false;
    }

    memset(&app->lifetime, 0, sizeof(app->lifetime));
    app_worker_contract_init(app);

    mapforge_shared_theme_load_persisted();
    app->lifetime.theme_loaded = true;
    app->width = 1280;
    app->height = 720;
    renderer_set_backend(&app->renderer, RENDERER_BACKEND_SDL);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        log_error("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    app->lifetime.sdl_initialized = true;

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
        if (renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN) {
            log_error("SDL_CreateWindow vulkan path failed, retrying SDL fallback: %s", SDL_GetError());
            renderer_set_backend(&app->renderer, RENDERER_BACKEND_SDL);
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
        } else {
            log_error("SDL_CreateWindow failed: %s", SDL_GetError());
            return false;
        }
    }
    app->lifetime.window_created = true;

    if (!renderer_init(&app->renderer, app->window, app->width, app->height)) {
        if (renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN) {
            log_error("renderer_init vulkan path failed, retrying SDL fallback: %s", SDL_GetError());
            renderer_shutdown(&app->renderer);
            SDL_DestroyWindow(app->window);
            app->window = NULL;
            app->lifetime.window_created = false;
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
            app->lifetime.window_created = true;
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
    app->lifetime.renderer_initialized = true;
    log_info("Render backend: %s (vulkan_available=%s)",
             renderer_backend_name(renderer_get_backend(&app->renderer)),
             app->renderer.vulkan_available ? "yes" : "no");

    app->tile_state_bridge.vk_assets_enabled = renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN;
    if (!vk_tile_cache_init(&app->tile_state_bridge.vk_tile_cache, 2048)) {
        log_error("vk_tile_cache_init failed");
        return false;
    }
    app->lifetime.vk_tile_cache_initialized = true;
    if (!app_vk_poly_prep_init(app)) {
        log_error("app_vk_poly_prep_init failed");
        return false;
    }
    app->lifetime.vk_poly_prep_initialized = true;
    if (!app_vk_asset_worker_init(app)) {
        log_error("app_vk_asset_worker_init failed");
        return false;
    }
    app->lifetime.vk_asset_worker_initialized = true;
    if (!app_route_worker_init(app)) {
        log_error("app_route_worker_init failed");
        return false;
    }
    app->lifetime.route_worker_initialized = true;
    if (TTF_Init() != 0) {
        log_error("TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    app->lifetime.ttf_initialized = true;
    app_apply_shared_ui_font(app);
    SDL_StartTextInput();
    app_default_input_root(app->input_root, sizeof(app->input_root));
    snprintf(app->input_root_edit, sizeof(app->input_root_edit), "%s", app->input_root);
    app->ingest_status[0] = '\0';
    app->ingest_package_status[0] = '\0';
    app->latest_imported_region[0] = '\0';
    app->ingest_panel_open = true;
    app->ingest_show_active_tab = false;
    app->ingest_edit_mode = false;
    app->ingest_osm_count = 0;
    app->ingest_selected_osm = 0;
    app->ingest_active_count = 0;
    app->ingest_selected_active = 0;
    app->ingest_last_active_click_tick = 0u;
    app->ingest_last_active_click_index = -1;
    app->ingest_import_running = false;
    app->ingest_import_pid = 0;
    app->ingest_import_all = false;
    app->ingest_import_expected_count = 0;
    app->ingest_import_open_region[0] = '\0';
    app->ingest_import_total_steps = 0;
    app->ingest_import_completed_steps = 0;
    app->ingest_import_progress_path[0] = '\0';
    app_load_persisted_view_state(app);
    snprintf(app->input_root_edit, sizeof(app->input_root_edit), "%s", app->input_root);
    const char *forced_region = getenv("MAPFORGE_START_REGION");
    if (forced_region && forced_region[0] != '\0') {
        snprintf(app->latest_imported_region, sizeof(app->latest_imported_region), "%s", forced_region);
    }
    app->viewport_scenario_active = false;
    app->viewport_scenario_completed = false;
    app->viewport_scenario_mode = APP_VIEWPORT_SCENARIO_NONE;
    app->viewport_scenario_start_time = 0.0;
    app->viewport_scenario_duration_sec = 0.0;
    app->viewport_scenario_origin_x = 0.0f;
    app->viewport_scenario_origin_y = 0.0f;
    app->viewport_scenario_origin_zoom = 0.0f;
    const char *viewport_scenario = getenv("MAPFORGE_VIEWPORT_SCENARIO");
    const char *viewport_scenario_label = NULL;
    if (viewport_scenario && strcmp(viewport_scenario, "phase_a") == 0) {
        app->viewport_scenario_mode = APP_VIEWPORT_SCENARIO_PHASE_A;
        viewport_scenario_label = "phase_a";
    } else if (viewport_scenario && strcmp(viewport_scenario, "phase_b") == 0) {
        app->viewport_scenario_mode = APP_VIEWPORT_SCENARIO_PHASE_B;
        viewport_scenario_label = "phase_b";
    }
    if (app->viewport_scenario_mode != APP_VIEWPORT_SCENARIO_NONE) {
        app->viewport_scenario_active = true;
        app->viewport_scenario_duration_sec =
            app_env_double_clamped("MAPFORGE_VIEWPORT_SCENARIO_DURATION_SEC", 45.0, 10.0, 300.0);
        log_info("Viewport scenario active: %s duration=%.1fs region_pref=%s",
                 viewport_scenario_label ? viewport_scenario_label : "unknown",
                 app->viewport_scenario_duration_sec,
                 forced_region && forced_region[0] != '\0' ? forced_region : "(none)");
    }

    int total_regions = region_count();
    app->region_index = -1;
    const RegionInfo *info = NULL;
    if (total_regions > 0) {
        if (app->latest_imported_region[0] != '\0') {
            app->region_index = app_find_region_index_by_name(app->latest_imported_region);
            if (app->region_index < 0) {
                log_error("Requested startup region '%s' was not found under %s",
                          app->latest_imported_region,
                          region_data_root());
            }
        }
        if (app->region_index < 0) {
            app->region_index = app_find_first_routable_region_index();
        }
        if (app->region_index < 0) {
            app->region_index = 0;
            log_error("No routable region found (missing graph/graph.bin in all regions); route placement is disabled until graph build completes");
        }
        info = region_get(app->region_index);
        if (info) {
            RegionPackageValidationResult validation = {0};
            if (!region_validate_package(info, &validation)) {
                log_error("Startup region '%s' failed package validation: %s",
                          info->name ? info->name : "unknown",
                          validation.summary);
                snprintf(app->ingest_package_status,
                         sizeof(app->ingest_package_status),
                         "region=%s invalid=%s",
                         info->name ? info->name : "unknown",
                         validation.summary);
                info = NULL;
                app->region_index = -1;
                snprintf(app->ingest_status,
                         sizeof(app->ingest_status),
                         "Region package invalid (%s). Open ingest panel (O) to repair/import.",
                         validation.summary);
            } else {
                log_info("Startup region '%s' runtime source policy=%s storage=%s archive=%s",
                         info->name ? info->name : "unknown",
                         tile_source_policy_mode_label(validation.runtime_policy_mode),
                         validation.archive_storage ? "archive_indexed" : "filesystem_tree",
                         validation.has_archive_path ? "yes" : "no");
                if (validation.archive_storage && validation.archive_fallback_tree) {
                    log_info("Startup region '%s' runtime source degraded: tree fallback enabled (archive_reader_supported=%s)",
                             info->name ? info->name : "unknown",
                             validation.archive_reader_supported ? "yes" : "no");
                }
                app_runtime_format_region_package_status(info->name,
                                                         &validation,
                                                         app->ingest_package_status,
                                                         sizeof(app->ingest_package_status));
                snprintf(app->ingest_status,
                         sizeof(app->ingest_status),
                         "Startup package ready: %s",
                         app->ingest_package_status);
            }
        }
    }
    if (info) {
        app->region = *info;
        if (!region_load_meta(info, &app->region)) {
            log_error("Startup region '%s' metadata load failed after validation", info->name ? info->name : "unknown");
            return false;
        }
        region_log_archive_rollup_summary(&app->region, "startup");
    } else {
        memset(&app->region, 0, sizeof(app->region));
        app->region.name = "no-region";
        snprintf(app->region.region_dir, sizeof(app->region.region_dir), "%s", region_data_root());
        snprintf(app->region.tiles_dir, sizeof(app->region.tiles_dir), "%s", region_data_root());
        app->region.tile_archive_path[0] = '\0';
        tile_source_config_set_filesystem(&app->region.tile_source, app->region.tiles_dir);
        app->region.has_tile_archive = false;
        snprintf(app->ingest_package_status,
                 sizeof(app->ingest_package_status),
                 "region=none storage=none policy=none degraded=none contract=none");
        if (!app_ensure_dir_recursive(region_data_root())) {
            log_error("Failed to ensure regions root exists: %s", region_data_root());
            return false;
        }
        snprintf(app->ingest_status, sizeof(app->ingest_status), "No regions loaded. Open ingest panel (O) to import OSM source files.");
    }
    if (app->region.tiles_dir[0] == '\0') {
        log_error("Failed to resolve tiles directory for region: %s", app->region.name ? app->region.name : "unknown");
        return false;
    }
    tile_source_runtime_stats_reset();
    log_info("Region data root: %s", region_data_root());

    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        if (!tile_manager_init_with_source(&app->tile_state_bridge.tile_managers[i], 256, &app->region.tile_source)) {
            log_error("tile_manager_init failed");
            return false;
        }
        app->lifetime.tile_managers_initialized += 1u;
    }
    if (!tile_loader_init_with_source(&app->tile_state_bridge.tile_loader, &app->region.tile_source)) {
        log_error("tile_loader_init failed");
        return false;
    }
    app->lifetime.tile_loader_initialized = true;
    app_trace_session_start(app);

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
    app->lifetime.route_state_initialized = true;
    if (!app_load_route_graph(app)) {
        log_error("Route graph load kickoff failed for startup region '%s'; build graph with: make graph && ./build/tools/mapforge_graph --region %s --osm data/osm_sources/<region>.osm --out data/regions/%s",
                  app->region.name ? app->region.name : "no-region",
                  app->region.name ? app->region.name : "no-region",
                  app->region.name ? app->region.name : "no-region");
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
    memset(&app->ui_state_bridge.header_layer_strip_rect, 0, sizeof(app->ui_state_bridge.header_layer_strip_rect));
    app->ui_state_bridge.header_layer_strip_scroll_px = 0.0f;
    app->ui_state_bridge.header_layer_strip_content_w = 0.0f;
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
    app_runtime_budget_policy_init(app);
    app_runtime_budget_reset_frame(app);
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
    app->tile_state_bridge.visible_ideal_count = 0u;
    app->tile_state_bridge.visible_renderable_count = 0u;
    app->tile_state_bridge.visible_missing_count = 0u;
    app->tile_state_bridge.visible_coverage_ratio = 1.0f;
    memset(app->tile_state_bridge.cache_target, 0, sizeof(app->tile_state_bridge.cache_target));
    memset(app->tile_state_bridge.cache_resident, 0, sizeof(app->tile_state_bridge.cache_resident));
    memset(app->tile_state_bridge.cache_evicted_frame, 0, sizeof(app->tile_state_bridge.cache_evicted_frame));
    memset(app->tile_state_bridge.cache_evicted_total_by_layer, 0, sizeof(app->tile_state_bridge.cache_evicted_total_by_layer));
    app->tile_state_bridge.cache_evicted_frame_total = 0u;
    app->tile_state_bridge.cache_evicted_total = 0u;
    memset(app->tile_state_bridge.layer_coverage_ratio, 0, sizeof(app->tile_state_bridge.layer_coverage_ratio));
    memset(app->tile_state_bridge.coverage_gate_pending, 0, sizeof(app->tile_state_bridge.coverage_gate_pending));
    memset(app->tile_state_bridge.coverage_gate_target_band, 0, sizeof(app->tile_state_bridge.coverage_gate_target_band));
    memset(app->tile_state_bridge.coverage_gate_pending_since, 0, sizeof(app->tile_state_bridge.coverage_gate_pending_since));
    app->tile_state_bridge.coverage_gate_deferred_count = 0u;
    app->tile_state_bridge.coverage_gate_timeout_count = 0u;
    memset(app->tile_state_bridge.lane_queue_depth, 0, sizeof(app->tile_state_bridge.lane_queue_depth));
    memset(app->tile_state_bridge.lane_service_count, 0, sizeof(app->tile_state_bridge.lane_service_count));
    app->tile_state_bridge.lane_l0_pending = 0u;
    app->tile_state_bridge.lane_l0_pending_active = false;
    app->tile_state_bridge.lane_l0_pending_since = 0.0;
    app->tile_state_bridge.lane_l0_latency_ms = 0.0f;
    app->tile_state_bridge.lane_l0_saturation_total = 0u;
    app->tile_state_bridge.lane_l0_dropped_visible_requests = 0u;
    app->tile_state_bridge.lane_l0_retry_visible_requests = 0u;
    app->tile_state_bridge.lifecycle_frame_index = 0u;
    app->tile_state_bridge.lifecycle_transition_count = 0u;
    app->tile_state_bridge.lifecycle_invalid_transition_count = 0u;
    app->tile_state_bridge.lifecycle_invalid_transition_total = 0u;
    memset(app->tile_state_bridge.lifecycle_transition_to_state, 0, sizeof(app->tile_state_bridge.lifecycle_transition_to_state));
    app->tile_state_bridge.lifecycle_renderable_ideal_count = 0u;
    app->tile_state_bridge.lifecycle_renderable_fallback_count = 0u;
    memset(app->tile_state_bridge.lifecycle_entries, 0, sizeof(app->tile_state_bridge.lifecycle_entries));
    app->tile_state_bridge.draw_path_vk_count = 0u;
    app->tile_state_bridge.draw_path_fallback_count = 0u;
    app->tile_state_bridge.band_commit_frame_count = 0u;
    app->tile_state_bridge.queue_rebuild_frame_count = 0u;
    app->tile_state_bridge.band_commit_total = 0u;
    app->tile_state_bridge.queue_rebuild_total = 0u;
    app->tile_state_bridge.band_switch_deferred_count = 0u;
    app->tile_state_bridge.queue_rebuild_deferred_count = 0u;
    app->tile_state_bridge.transition_blend_draw_count = 0u;
    app->tile_state_bridge.present_hold_hits = 0u;
    app->tile_state_bridge.present_hold_misses = 0u;
    app->tile_state_bridge.present_hold_updates = 0u;
    app->tile_state_bridge.present_hold_tick = 1u;
    app->tile_state_bridge.last_queue_rebuild_time = 0.0;
    app_ingest_rescan_sources(app);
    app_ingest_rescan_active_regions(app);
    app_refresh_layer_states(app);
    app_bridge_sync_from_legacy(app);
    app->lifetime.persisted_state_ready = true;

    return true;
}

static void app_shutdown(AppState *app) {
    if (!app) {
        return;
    }
    if (app->lifetime.shutdown_completed) {
        return;
    }
    app->lifetime.shutdown_completed = true;

    app_runtime_ingest_shutdown(app);

    if (app->lifetime.persisted_state_ready) {
        app_bridge_sync_to_legacy(app);
        app_save_persisted_view_state(app);
        if (app->lifetime.theme_loaded) {
            mapforge_shared_theme_save_persisted();
        }
        app->lifetime.persisted_state_ready = false;
    }

    if (app->lifetime.ttf_initialized) {
        SDL_StopTextInput();
        if (app->lifetime.renderer_initialized) {
            ui_font_shutdown(&app->renderer);
        }
        if (TTF_WasInit()) {
            TTF_Quit();
        }
        app->lifetime.ttf_initialized = false;
    }

    if (app->lifetime.route_worker_initialized) {
        app_route_worker_shutdown(app);
        app->lifetime.route_worker_initialized = false;
    }
    if (app->lifetime.vk_asset_worker_initialized) {
        app_vk_asset_worker_shutdown(app);
        app->lifetime.vk_asset_worker_initialized = false;
    }
    if (app->lifetime.vk_poly_prep_initialized) {
        app_vk_poly_prep_shutdown(app);
        app->lifetime.vk_poly_prep_initialized = false;
    }
    if (app->lifetime.tile_loader_initialized) {
        tile_loader_shutdown(&app->tile_state_bridge.tile_loader);
        app->lifetime.tile_loader_initialized = false;
    }
    app_clear_tile_queue(app);

    if (app->lifetime.vk_tile_cache_initialized) {
        vk_tile_cache_clear_with_renderer(&app->tile_state_bridge.vk_tile_cache,
                                          app->lifetime.renderer_initialized ? app->renderer.vk : NULL);
    }
    while (app->lifetime.tile_managers_initialized > 0u) {
        app->lifetime.tile_managers_initialized -= 1u;
        TileManager *manager = &app->tile_state_bridge.tile_managers[app->lifetime.tile_managers_initialized];
        /*
         * Exit-path hardening: release manager entry slabs without deep tile payload frees.
         * Runtime/reinit paths still use tile_manager_shutdown(); this avoids close-time aborts
         * when stale/duplicated tile ownership appears late in process lifetime.
         */
        free(manager->entries);
        memset(manager, 0, sizeof(*manager));
    }

    if (app->lifetime.route_state_initialized) {
        app_route_release_snap_index(app);
        route_state_shutdown(&app->route_state_bridge.route);
        app->lifetime.route_state_initialized = false;
    }
    if (app->lifetime.trace_session_initialized) {
        app_trace_shutdown(app);
        app->lifetime.trace_session_initialized = false;
    }
    if (app->lifetime.vk_tile_cache_initialized) {
        vk_tile_cache_shutdown(&app->tile_state_bridge.vk_tile_cache);
        app->lifetime.vk_tile_cache_initialized = false;
    }
    if (app->lifetime.renderer_initialized) {
        renderer_shutdown(&app->renderer);
        app->lifetime.renderer_initialized = false;
    }

    if (app->lifetime.window_created && app->window) {
        SDL_DestroyWindow(app->window);
        app->window = NULL;
        app->lifetime.window_created = false;
    }
    if (app->lifetime.sdl_initialized) {
        SDL_Quit();
        app->lifetime.sdl_initialized = false;
    }
}

int app_run_legacy(void) {
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
    KitRuntimeDiagInputTotals input_totals = {0};

    while (!app.ui_state_bridge.input.quit) {
        AppRuntimeDispatchFrame frame = {0};
        app_runtime_dispatch_frame(&app, &last_time, &last_backend, &frame);
        if (frame.skipped_for_global_controls) {
            continue;
        }

        RoadRenderStats road_stats = {0};
        road_renderer_stats_get(&road_stats);

        if (frame.loading_done != last_loading_done) {
            last_loading_done = frame.loading_done;
            last_loading_progress_time = frame.after_render;
        }

        KitRuntimeDiagStageMarks stage_marks = {
            .frame_begin = frame.frame_begin,
            .after_events = frame.after_events,
            .after_update = frame.after_update,
            .after_queue = frame.after_queue,
            .after_integrate = frame.after_integrate,
            .after_route = frame.after_route,
            .after_render_derive = frame.after_render_derive,
            .before_present = frame.before_present,
            .after_render = frame.after_render,
        };
        KitRuntimeDiagTimings diag_timings = {0};
        kit_runtime_diag_compute_timings(&stage_marks, &diag_timings);

        app.frame_timings.frame_ms = diag_timings.frame_ms;
        app.frame_timings.events_ms = diag_timings.events_ms;
        app.frame_timings.update_ms = diag_timings.update_ms;
        app.frame_timings.queue_ms = diag_timings.queue_ms;
        app.frame_timings.integrate_ms = diag_timings.integrate_ms;
        app.frame_timings.route_ms = diag_timings.route_ms;
        app.frame_timings.render_ms = diag_timings.render_ms;
        app.frame_timings.present_ms = diag_timings.present_ms;
        if (app.trace_enabled) {
            double rel_time_s = frame.after_render - app.trace_start_time;
            app_trace_emit_frame_samples(&app, rel_time_s);
            app_trace_emit_queue_markers(&app, rel_time_s);
        }
        double frame_ms = app.frame_timings.frame_ms;
        double events_ms = app.frame_timings.events_ms;
        double render_derive_ms = diag_timings.render_derive_ms;
        double render_submit_ms = diag_timings.render_submit_ms;
        bool long_frame = frame_ms >= 120.0;
        bool stuck_loading = frame.loading_expected > 0 &&
            frame.loading_done < frame.loading_expected &&
            (frame.after_render - last_loading_progress_time) >= 1.5;
        KitRuntimeDiagInputFrame input_frame = {
            .raw_event_count = frame.input.raw.sdl_event_count,
            .action_count = frame.input.normalized.action_count,
            .text_entry_gate_active = frame.input.normalized.text_entry_gate_active,
            .ignored_count = frame.input.normalized.ignored_count,
            .routed_global_count = frame.input.route.routed_global_count,
            .routed_pane_count = frame.input.route.routed_pane_count,
            .routed_fallback_count = frame.input.route.routed_fallback_count,
            .target_invalidation_count = frame.input.invalidation.target_invalidation_count,
            .full_invalidation_count = frame.input.invalidation.full_invalidation_count,
            .invalidation_reason_bits = frame.input.invalidation.invalidation_reason_bits,
        };
        kit_runtime_diag_input_totals_accumulate(&input_totals, &input_frame);
        if (vk_debug_logs && (long_frame || stuck_loading || frame.after_render >= perf_next_log)) {
            TileLoaderStats stats = {0};
            TileSourceRuntimeStats source_stats = {0};
            tile_loader_get_stats(&app.tile_state_bridge.tile_loader, &stats);
            tile_source_runtime_stats_get(&source_stats);
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
                log_info("perf region=%s backend=vk frame=%.1fms events=%.1f update=%.1f queue=%.1f integrate=%.1f route=%.1f render=%.1f present=%.1f rderive=%.1f rsubmit=%.1f draw_pass=%u zoom=%.2f vis=%u viewset(i=%u r=%u m=%u) load=%u/%u active=%s "
                         "input(frame_raw=%u frame_actions=%u gate=%u route_g=%u route_p=%u route_f=%u inval_t=%u inval_f=%u inval_bits=0x%x) "
                         "input(total_raw=%llu total_actions=%llu total_gated=%llu total_route(g=%llu p=%llu f=%llu) total_inval(t=%llu f=%llu)) "
                         "band_target(a=%s l=%s) band_vis(c=%u/%u m=%u/%u f=%u/%u d=%u/%u) band_q(c=%u m=%u f=%u d=%u) band_fallback=%u "
                         "life(frame=%llu tx=%u bad=%u req=%u cpu=%u gpu=%u ren=%u stale=%u ideal=%u fallback=%u bad_total=%llu) "
                         "req=%u/%u res=%u/%u enq=%llu drop=%llu evict=%llu out=%llu out_drop=%llu out_evict=%llu miss=%llu ok=%llu fail=%llu "
                         "src=%s arch(req=%llu hit=%llu ext=%llu fail=%llu tree=%llu) "
                         "vk_begin=%d vk_begin_fail_total=%llu vk_recreate=%u vk_geom=%u/%u vk_geom_skip=%u vk_lines=%u vk_line_skip=%u vk_line_budget=%u vk_rect=%u vk_fill=%u "
                         "vk_assets=%u/%u builds=%u evict=%u miss=%u jobs(q=%u build=%llu drop=%llu evict=%llu) "
                         "poly_prep(in=%u out=%u enq=%llu done=%llu drop=%llu qj=%llu qp=%llu qb=%llu qm=%llu qd=%llu wind=%llu) "
                         "resident(a=%u l=%u) fill_resident(w=%u p=%u l=%u b=%u) "
                         "mesh(v=%llu b=%llu fail=%u fill_fail=%u) vk_poly_fill(draw=%u skip=%u fail=%u idx=%u) "
                         "road_draw(m=%u l=%u p=%u) road_filter(m=%u l=%u p=%u) draw_path(vk=%u fallback=%u blend=%u) defer(band=%u queue=%u) hold(hit=%u miss=%u upd=%u)",
                         app.region.name,
                         frame_ms, events_ms, app.frame_timings.update_ms,
                         app.frame_timings.queue_ms, app.frame_timings.integrate_ms,
                         app.frame_timings.route_ms, app.frame_timings.render_ms, app.frame_timings.present_ms,
                         render_derive_ms, render_submit_ms, frame.render_draw_pass_count,
                         app.view_state_bridge.camera.zoom,
                         app.tile_state_bridge.visible_tile_count,
                         app.tile_state_bridge.visible_ideal_count,
                         app.tile_state_bridge.visible_renderable_count,
                         app.tile_state_bridge.visible_missing_count,
                         app.tile_state_bridge.loading_done, app.tile_state_bridge.loading_expected,
                         app.tile_state_bridge.active_layer_valid ? layer_policy_label(app.tile_state_bridge.active_layer_kind) : "none",
                         frame.input.raw.sdl_event_count,
                         frame.input.normalized.action_count,
                         frame.input.normalized.text_entry_gate_active ? 1u : 0u,
                         frame.input.route.routed_global_count,
                         frame.input.route.routed_pane_count,
                         frame.input.route.routed_fallback_count,
                         frame.input.invalidation.target_invalidation_count,
                         frame.input.invalidation.full_invalidation_count,
                         frame.input.invalidation.invalidation_reason_bits,
                         (unsigned long long)input_totals.raw_event_count,
                         (unsigned long long)input_totals.action_count,
                         (unsigned long long)input_totals.shortcut_gated_count,
                         (unsigned long long)input_totals.routed_global_count,
                         (unsigned long long)input_totals.routed_pane_count,
                         (unsigned long long)input_totals.routed_fallback_count,
                         (unsigned long long)input_totals.target_invalidation_count,
                         (unsigned long long)input_totals.full_invalidation_count,
                         layer_policy_band_label(app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_ARTERY]),
                         layer_policy_band_label(app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_LOCAL]),
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_COARSE], app.tile_state_bridge.band_visible_expected[TILE_BAND_COARSE],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_MID], app.tile_state_bridge.band_visible_expected[TILE_BAND_MID],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_FINE], app.tile_state_bridge.band_visible_expected[TILE_BAND_FINE],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_DEFAULT], app.tile_state_bridge.band_visible_expected[TILE_BAND_DEFAULT],
                         app.tile_state_bridge.band_queue_depth[TILE_BAND_COARSE], app.tile_state_bridge.band_queue_depth[TILE_BAND_MID],
                         app.tile_state_bridge.band_queue_depth[TILE_BAND_FINE], app.tile_state_bridge.band_queue_depth[TILE_BAND_DEFAULT],
                         app.tile_state_bridge.vk_road_band_fallback_draws,
                         (unsigned long long)app.tile_state_bridge.lifecycle_frame_index,
                         app.tile_state_bridge.lifecycle_transition_count,
                         app.tile_state_bridge.lifecycle_invalid_transition_count,
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_REQUESTED],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_DECODED_CPU],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_UPLOADED_GPU],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_RENDERABLE],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_STALE],
                         app.tile_state_bridge.lifecycle_renderable_ideal_count,
                         app.tile_state_bridge.lifecycle_renderable_fallback_count,
                         (unsigned long long)app.tile_state_bridge.lifecycle_invalid_transition_total,
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
                         tile_storage_kind_label(app.region.tile_source.storage_kind),
                         (unsigned long long)source_stats.archive_request_count,
                         (unsigned long long)source_stats.archive_hit_count,
                         (unsigned long long)source_stats.archive_extract_count,
                         (unsigned long long)source_stats.archive_extract_fail_count,
                         (unsigned long long)source_stats.archive_fallback_tree_count,
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
                         (unsigned long long)poly_prep_stats.quarantine_job_count,
                         (unsigned long long)poly_prep_stats.quarantine_polygon_count,
                         (unsigned long long)poly_prep_stats.quarantine_ring_bounds_count,
                         (unsigned long long)poly_prep_stats.quarantine_ring_min_points_count,
                         (unsigned long long)poly_prep_stats.quarantine_ring_degenerate_count,
                         (unsigned long long)poly_prep_stats.winding_normalized_count,
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
                log_info("perf_phase_a cov(global=%.3f layer(a=%.3f l=%.3f c=%.3f w=%.3f p=%.3f lu=%.3f b=%.3f)) "
                         "l0(lat_ms=%.2f pending=%u sat=%llu drop=%llu retry=%llu) "
                         "gate(defer=%u timeout=%u) cache(evict=%u total=%llu a=%u/%u l=%u/%u b=%u/%u) "
                         "churn(frame_band=%u frame_rebuild=%u total_band=%llu total_rebuild=%llu) "
                         "poly_layer(job w=%llu p=%llu lu=%llu b=%llu ring w=%llu p=%llu lu=%llu b=%llu) "
                         "budget(load req=%u app=%u clamp=%u ex=%u lane_hit=%u/%u/%u/%u integ req=%u app=%u clamp=%u ex=%u vk_asset=%u/%u sat=%u vk_poly_asset=%u/%u hit=%u)",
                         app.tile_state_bridge.visible_coverage_ratio,
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_ROAD_ARTERY],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_ROAD_LOCAL],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_CONTOUR],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_WATER],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_PARK],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_LANDUSE],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.lane_l0_latency_ms,
                         app.tile_state_bridge.lane_l0_pending,
                         (unsigned long long)app.tile_state_bridge.lane_l0_saturation_total,
                         (unsigned long long)app.tile_state_bridge.lane_l0_dropped_visible_requests,
                         (unsigned long long)app.tile_state_bridge.lane_l0_retry_visible_requests,
                         app.tile_state_bridge.coverage_gate_deferred_count,
                         app.tile_state_bridge.coverage_gate_timeout_count,
                         app.tile_state_bridge.cache_evicted_frame_total,
                         (unsigned long long)app.tile_state_bridge.cache_evicted_total,
                         app.tile_state_bridge.cache_resident[TILE_LAYER_ROAD_ARTERY],
                         app.tile_state_bridge.cache_target[TILE_LAYER_ROAD_ARTERY],
                         app.tile_state_bridge.cache_resident[TILE_LAYER_ROAD_LOCAL],
                         app.tile_state_bridge.cache_target[TILE_LAYER_ROAD_LOCAL],
                         app.tile_state_bridge.cache_resident[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.cache_target[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.band_commit_frame_count,
                         app.tile_state_bridge.queue_rebuild_frame_count,
                         (unsigned long long)app.tile_state_bridge.band_commit_total,
                         (unsigned long long)app.tile_state_bridge.queue_rebuild_total,
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_WATER],
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_PARK],
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_LANDUSE],
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_BUILDING],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_WATER],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_PARK],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_LANDUSE],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.budget_frame.load_budget_requested_total,
                         app.tile_state_bridge.budget_frame.load_budget_applied_total,
                         app.tile_state_bridge.budget_frame.load_budget_clamped_count,
                         app.tile_state_bridge.budget_frame.load_budget_exhausted_count,
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L0_VISIBLE_MISSING],
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L1_VISIBLE_REFINE],
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L2_NEAR_PREFETCH],
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L3_FAR_PREFETCH],
                         app.tile_state_bridge.budget_frame.integrate_budget_requested,
                         app.tile_state_bridge.budget_frame.integrate_budget_applied,
                         app.tile_state_bridge.budget_frame.integrate_budget_clamped_count,
                         app.tile_state_bridge.budget_frame.integrate_budget_exhausted_count,
                         app.tile_state_bridge.budget_frame.vk_asset_jobs_built,
                         app.tile_state_bridge.budget_frame.vk_asset_jobs_budget,
                         app.tile_state_bridge.budget_frame.vk_asset_budget_saturated_count,
                         app.tile_state_bridge.budget_frame.vk_poly_asset_budget_used,
                         app.tile_state_bridge.budget_frame.vk_poly_asset_budget_cap,
                         app.tile_state_bridge.budget_frame.vk_poly_asset_budget_hit_count);
            } else {
                VkPolyPrepStats poly_prep_stats = {0};
                app_vk_poly_prep_get_stats(&app, &poly_prep_stats);
                log_info("perf region=%s backend=sdl frame=%.1fms events=%.1f update=%.1f queue=%.1f integrate=%.1f route=%.1f render=%.1f present=%.1f rderive=%.1f rsubmit=%.1f draw_pass=%u zoom=%.2f vis=%u viewset(i=%u r=%u m=%u) load=%u/%u active=%s "
                         "input(frame_raw=%u frame_actions=%u gate=%u route_g=%u route_p=%u route_f=%u inval_t=%u inval_f=%u inval_bits=0x%x) "
                         "input(total_raw=%llu total_actions=%llu total_gated=%llu total_route(g=%llu p=%llu f=%llu) total_inval(t=%llu f=%llu)) "
                         "band_target(a=%s l=%s) band_vis(c=%u/%u m=%u/%u f=%u/%u d=%u/%u) band_q(c=%u m=%u f=%u d=%u) band_fallback=%u "
                         "life(frame=%llu tx=%u bad=%u req=%u cpu=%u gpu=%u ren=%u stale=%u ideal=%u fallback=%u bad_total=%llu) "
                         "req=%u/%u res=%u/%u enq=%llu drop=%llu evict=%llu out=%llu out_drop=%llu out_evict=%llu miss=%llu ok=%llu fail=%llu "
                         "src=%s arch(req=%llu hit=%llu ext=%llu fail=%llu tree=%llu) "
                         "draw_path(vk=%u fallback=%u blend=%u) defer(band=%u queue=%u) hold(hit=%u miss=%u upd=%u)",
                         app.region.name,
                         frame_ms, events_ms, app.frame_timings.update_ms,
                         app.frame_timings.queue_ms, app.frame_timings.integrate_ms,
                         app.frame_timings.route_ms, app.frame_timings.render_ms, app.frame_timings.present_ms,
                         render_derive_ms, render_submit_ms, frame.render_draw_pass_count,
                         app.view_state_bridge.camera.zoom,
                         app.tile_state_bridge.visible_tile_count,
                         app.tile_state_bridge.visible_ideal_count,
                         app.tile_state_bridge.visible_renderable_count,
                         app.tile_state_bridge.visible_missing_count,
                         app.tile_state_bridge.loading_done, app.tile_state_bridge.loading_expected,
                         app.tile_state_bridge.active_layer_valid ? layer_policy_label(app.tile_state_bridge.active_layer_kind) : "none",
                         frame.input.raw.sdl_event_count,
                         frame.input.normalized.action_count,
                         frame.input.normalized.text_entry_gate_active ? 1u : 0u,
                         frame.input.route.routed_global_count,
                         frame.input.route.routed_pane_count,
                         frame.input.route.routed_fallback_count,
                         frame.input.invalidation.target_invalidation_count,
                         frame.input.invalidation.full_invalidation_count,
                         frame.input.invalidation.invalidation_reason_bits,
                         (unsigned long long)input_totals.raw_event_count,
                         (unsigned long long)input_totals.action_count,
                         (unsigned long long)input_totals.shortcut_gated_count,
                         (unsigned long long)input_totals.routed_global_count,
                         (unsigned long long)input_totals.routed_pane_count,
                         (unsigned long long)input_totals.routed_fallback_count,
                         (unsigned long long)input_totals.target_invalidation_count,
                         (unsigned long long)input_totals.full_invalidation_count,
                         layer_policy_band_label(app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_ARTERY]),
                         layer_policy_band_label(app.tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_LOCAL]),
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_COARSE], app.tile_state_bridge.band_visible_expected[TILE_BAND_COARSE],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_MID], app.tile_state_bridge.band_visible_expected[TILE_BAND_MID],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_FINE], app.tile_state_bridge.band_visible_expected[TILE_BAND_FINE],
                         app.tile_state_bridge.band_visible_loaded[TILE_BAND_DEFAULT], app.tile_state_bridge.band_visible_expected[TILE_BAND_DEFAULT],
                         app.tile_state_bridge.band_queue_depth[TILE_BAND_COARSE], app.tile_state_bridge.band_queue_depth[TILE_BAND_MID],
                         app.tile_state_bridge.band_queue_depth[TILE_BAND_FINE], app.tile_state_bridge.band_queue_depth[TILE_BAND_DEFAULT],
                         app.tile_state_bridge.vk_road_band_fallback_draws,
                         (unsigned long long)app.tile_state_bridge.lifecycle_frame_index,
                         app.tile_state_bridge.lifecycle_transition_count,
                         app.tile_state_bridge.lifecycle_invalid_transition_count,
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_REQUESTED],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_DECODED_CPU],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_UPLOADED_GPU],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_RENDERABLE],
                         app.tile_state_bridge.lifecycle_transition_to_state[APP_TILE_LIFECYCLE_STALE],
                         app.tile_state_bridge.lifecycle_renderable_ideal_count,
                         app.tile_state_bridge.lifecycle_renderable_fallback_count,
                         (unsigned long long)app.tile_state_bridge.lifecycle_invalid_transition_total,
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
                         tile_storage_kind_label(app.region.tile_source.storage_kind),
                         (unsigned long long)source_stats.archive_request_count,
                         (unsigned long long)source_stats.archive_hit_count,
                         (unsigned long long)source_stats.archive_extract_count,
                         (unsigned long long)source_stats.archive_extract_fail_count,
                         (unsigned long long)source_stats.archive_fallback_tree_count,
                         app.tile_state_bridge.draw_path_vk_count,
                         app.tile_state_bridge.draw_path_fallback_count,
                         app.tile_state_bridge.transition_blend_draw_count,
                         app.tile_state_bridge.band_switch_deferred_count,
                         app.tile_state_bridge.queue_rebuild_deferred_count,
                         app.tile_state_bridge.present_hold_hits,
                         app.tile_state_bridge.present_hold_misses,
                         app.tile_state_bridge.present_hold_updates);
                log_info("perf_phase_a cov(global=%.3f layer(a=%.3f l=%.3f c=%.3f w=%.3f p=%.3f lu=%.3f b=%.3f)) "
                         "l0(lat_ms=%.2f pending=%u sat=%llu drop=%llu retry=%llu) "
                         "gate(defer=%u timeout=%u) cache(evict=%u total=%llu a=%u/%u l=%u/%u b=%u/%u) "
                         "churn(frame_band=%u frame_rebuild=%u total_band=%llu total_rebuild=%llu) "
                         "poly_layer(job w=%llu p=%llu lu=%llu b=%llu ring w=%llu p=%llu lu=%llu b=%llu) "
                         "budget(load req=%u app=%u clamp=%u ex=%u lane_hit=%u/%u/%u/%u integ req=%u app=%u clamp=%u ex=%u vk_asset=%u/%u sat=%u vk_poly_asset=%u/%u hit=%u)",
                         app.tile_state_bridge.visible_coverage_ratio,
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_ROAD_ARTERY],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_ROAD_LOCAL],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_CONTOUR],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_WATER],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_PARK],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_LANDUSE],
                         app.tile_state_bridge.layer_coverage_ratio[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.lane_l0_latency_ms,
                         app.tile_state_bridge.lane_l0_pending,
                         (unsigned long long)app.tile_state_bridge.lane_l0_saturation_total,
                         (unsigned long long)app.tile_state_bridge.lane_l0_dropped_visible_requests,
                         (unsigned long long)app.tile_state_bridge.lane_l0_retry_visible_requests,
                         app.tile_state_bridge.coverage_gate_deferred_count,
                         app.tile_state_bridge.coverage_gate_timeout_count,
                         app.tile_state_bridge.cache_evicted_frame_total,
                         (unsigned long long)app.tile_state_bridge.cache_evicted_total,
                         app.tile_state_bridge.cache_resident[TILE_LAYER_ROAD_ARTERY],
                         app.tile_state_bridge.cache_target[TILE_LAYER_ROAD_ARTERY],
                         app.tile_state_bridge.cache_resident[TILE_LAYER_ROAD_LOCAL],
                         app.tile_state_bridge.cache_target[TILE_LAYER_ROAD_LOCAL],
                         app.tile_state_bridge.cache_resident[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.cache_target[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.band_commit_frame_count,
                         app.tile_state_bridge.queue_rebuild_frame_count,
                         (unsigned long long)app.tile_state_bridge.band_commit_total,
                         (unsigned long long)app.tile_state_bridge.queue_rebuild_total,
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_WATER],
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_PARK],
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_LANDUSE],
                         (unsigned long long)poly_prep_stats.quarantine_jobs_by_layer[TILE_LAYER_POLY_BUILDING],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_WATER],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_PARK],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_LANDUSE],
                         (unsigned long long)poly_prep_stats.quarantine_rings_by_layer[TILE_LAYER_POLY_BUILDING],
                         app.tile_state_bridge.budget_frame.load_budget_requested_total,
                         app.tile_state_bridge.budget_frame.load_budget_applied_total,
                         app.tile_state_bridge.budget_frame.load_budget_clamped_count,
                         app.tile_state_bridge.budget_frame.load_budget_exhausted_count,
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L0_VISIBLE_MISSING],
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L1_VISIBLE_REFINE],
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L2_NEAR_PREFETCH],
                         app.tile_state_bridge.budget_frame.lane_cap_hits[TILE_QUEUE_LANE_L3_FAR_PREFETCH],
                         app.tile_state_bridge.budget_frame.integrate_budget_requested,
                         app.tile_state_bridge.budget_frame.integrate_budget_applied,
                         app.tile_state_bridge.budget_frame.integrate_budget_clamped_count,
                         app.tile_state_bridge.budget_frame.integrate_budget_exhausted_count,
                         app.tile_state_bridge.budget_frame.vk_asset_jobs_built,
                         app.tile_state_bridge.budget_frame.vk_asset_jobs_budget,
                         app.tile_state_bridge.budget_frame.vk_asset_budget_saturated_count,
                         app.tile_state_bridge.budget_frame.vk_poly_asset_budget_used,
                         app.tile_state_bridge.budget_frame.vk_poly_asset_budget_cap,
                         app.tile_state_bridge.budget_frame.vk_poly_asset_budget_hit_count);
            }
            perf_next_log = frame.after_render + 1.0;
        }
    }

    app_shutdown(&app);
    return 0;
}

int app_run(void) {
    return map_forge_app_main();
}
