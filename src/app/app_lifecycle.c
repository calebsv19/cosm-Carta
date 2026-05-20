#include "app/app_internal.h"
#include "app/app_persist_state.h"
#include "app/app_trace_runtime.h"

#include "core/log.h"
#include "app/region_loader.h"
#include "ui/font.h"
#include "ui/shared_theme_font_adapter.h"

#include <SDL.h>
#include <SDL2/SDL_ttf.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

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

bool app_init(AppState *app) {
    if (!app) {
        return false;
    }

    memset(&app->lifetime, 0, sizeof(app->lifetime));
    app_worker_contract_init(app);

    mapforge_shared_theme_load_persisted();
    app->lifetime.theme_loaded = true;
    map_forge_workspace_authoring_host_reset(&app->ui_state_bridge.workspace_authoring);
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

    uint32_t window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN) {
        window_flags |= SDL_WINDOW_VULKAN | SDL_WINDOW_ALLOW_HIGHDPI;
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
                SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
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
                SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
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
        if (!tile_manager_init_with_source_for_layer(&app->tile_state_bridge.tile_managers[i],
                                                     256,
                                                     &app->region.tile_source,
                                                     (TileLayerKind)i)) {
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
    app->route_state_bridge.preview_follow_enabled = false;
    app->route_state_bridge.preview_heading_up = true;
    app->route_state_bridge.preview_heading_memory_valid = false;
    app->route_state_bridge.preview_heading_memory_rad = 0.0f;
    app->route_state_bridge.preview_heading_memory_sample_time_s = 0.0f;
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

void app_shutdown(AppState *app) {
    if (!app) {
        return;
    }
    if (app->lifetime.shutdown_completed) {
        return;
    }
    app->lifetime.shutdown_completed = true;

    app_runtime_ingest_shutdown(app);

    if (app->lifetime.persisted_state_ready) {
        if (map_forge_workspace_authoring_host_active(&app->ui_state_bridge.workspace_authoring)) {
            (void)map_forge_workspace_authoring_host_cancel(&app->ui_state_bridge.workspace_authoring);
            if (map_forge_workspace_authoring_host_take_font_dirty(
                    &app->ui_state_bridge.workspace_authoring)) {
                app_apply_shared_ui_font(app);
            }
        }
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
