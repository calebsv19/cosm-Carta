#include "app/app_internal.h"

#include "app/region_loader.h"
#include "core/log.h"
#include "ui/font.h"
#include "ui/shared_theme_font_adapter.h"

#include <string.h>

static const RouteObjective kObjectiveCycle[] = {
    ROUTE_OBJECTIVE_SHORTEST_DISTANCE,
    ROUTE_OBJECTIVE_LOWEST_TIME,
    ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN,
    ROUTE_OBJECTIVE_MOST_TIME_ABOVE_SPEED_THRESHOLD
};

static RouteObjective app_runtime_next_route_objective(RouteObjective current) {
    for (size_t i = 0; i < sizeof(kObjectiveCycle) / sizeof(kObjectiveCycle[0]); ++i) {
        if (kObjectiveCycle[i] == current) {
            size_t next = (i + 1u) % (sizeof(kObjectiveCycle) / sizeof(kObjectiveCycle[0]));
            return kObjectiveCycle[next];
        }
    }
    return ROUTE_OBJECTIVE_SHORTEST_DISTANCE;
}

static int app_runtime_find_next_region_index(int current_index) {
    int total = region_count();
    if (total <= 0) {
        return -1;
    }
    int base_index = current_index;
    if (base_index < 0) {
        base_index = 0;
    }
    for (int step = 1; step <= total; ++step) {
        int candidate = (base_index + step) % total;
        const RegionInfo *info = region_get(candidate);
        if (info) {
            return candidate;
        }
    }
    return -1;
}

void app_apply_shared_ui_font(AppState *app) {
    char shared_font_path[384] = {0};
    int shared_font_size = 0;
    if (!app) {
        return;
    }
    if (mapforge_shared_font_resolve_ui_regular(shared_font_path,
                                                sizeof(shared_font_path),
                                                &shared_font_size)) {
        ui_font_set(shared_font_path, shared_font_size);
    } else {
        ui_font_set("assets/fonts/Montserrat-Regular.ttf", 10);
    }
    app->ui_state_bridge.hud_layer_debug_layout_dirty = true;
    app->ui_state_bridge.hud_route_panel_layout_dirty = true;
}

bool app_runtime_handle_global_controls(AppState *app) {
    if (!app) {
        return false;
    }

    if (app->ui_state_bridge.input.toggle_debug_pressed) {
        app->ui_state_bridge.overlay.enabled = !app->ui_state_bridge.overlay.enabled;
    }
    if (app->ui_state_bridge.input.toggle_single_line_pressed) {
        app->single_line = !app->single_line;
    }
    if (app->ui_state_bridge.input.toggle_region_pressed) {
        int total_regions = region_count();
        if (total_regions <= 0) {
            log_error("No region packs found under '%s'", region_data_root());
            return true;
        }
        int next_index = app_runtime_find_next_region_index(app->region_index);
        if (next_index < 0) {
            log_error("No switchable regions found under '%s'", region_data_root());
            return true;
        }
        app->region_index = next_index;
        const RegionInfo *info = region_get(app->region_index);
        if (info) {
            app->region = *info;
            region_load_meta(info, &app->region);
            if (app->region.tiles_dir[0] == '\0') {
                log_error("Failed to resolve tiles directory for region: %s", app->region.name);
                return true;
            }
            // Invalidate in-flight work first, then teardown/re-init loader+managers in a safe order.
            app_worker_contract_bump_world_generation(app);
            app_worker_contract_bump_tile_generation(app);
            app_clear_tile_queue(app);
            tile_loader_shutdown(&app->tile_state_bridge.tile_loader);
            vk_tile_cache_clear_with_renderer(&app->tile_state_bridge.vk_tile_cache, app->renderer.vk);
            for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
                tile_manager_shutdown(&app->tile_state_bridge.tile_managers[i]);
                tile_manager_init(&app->tile_state_bridge.tile_managers[i], 256, app->region.tiles_dir);
            }
            tile_loader_init(&app->tile_state_bridge.tile_loader, app->region.tiles_dir);
            app->tile_state_bridge.visible_valid = false;
            app->tile_state_bridge.loading_expected = 0;
            app->tile_state_bridge.loading_done = 0;
            app->tile_state_bridge.loading_no_data_time = 0.0f;
            if (!app_load_route_graph(app)) {
                log_error("Route graph missing for region '%s'; skipping route interactions in this region.", app->region.name);
            }
            route_state_clear(&app->route_state_bridge.route);
            app_route_worker_clear(app);
            app_playback_reset(app);
            memset(&app->route_state_bridge.hover_anchor, 0, sizeof(app->route_state_bridge.hover_anchor));
            memset(&app->route_state_bridge.start_anchor, 0, sizeof(app->route_state_bridge.start_anchor));
            memset(&app->route_state_bridge.goal_anchor, 0, sizeof(app->route_state_bridge.goal_anchor));
            app->view_state_bridge.building_zoom_bias = app_building_zoom_bias_for_region(&app->region);
            app->view_state_bridge.road_zoom_bias = app_road_zoom_bias_for_region(&app->region);
            app_center_camera_on_region(&app->view_state_bridge.camera, &app->region, app->width, app->height);
        }
    }
    if (app->ui_state_bridge.input.toggle_profile_pressed) {
        app->route_state_bridge.route.objective = app_runtime_next_route_objective(app->route_state_bridge.route.objective);
        if (app->route_state_bridge.route.has_start && app->route_state_bridge.route.has_goal) {
            app_route_schedule_recompute(app, 0.0);
        }
    }
    if (app->ui_state_bridge.input.toggle_landuse_pressed) {
        app->view_state_bridge.show_landuse = !app->view_state_bridge.show_landuse;
    }
    if (app->ui_state_bridge.input.toggle_building_fill_pressed) {
        app->view_state_bridge.building_fill_enabled = !app->view_state_bridge.building_fill_enabled;
    }
    if (app->ui_state_bridge.input.toggle_polygon_outline_pressed) {
        app->view_state_bridge.polygon_outline_only = !app->view_state_bridge.polygon_outline_only;
    }
    if (app->ui_state_bridge.input.theme_cycle_next_pressed) {
        mapforge_shared_theme_cycle_next();
        mapforge_shared_theme_save_persisted();
    }
    if (app->ui_state_bridge.input.theme_cycle_prev_pressed) {
        mapforge_shared_theme_cycle_prev();
        mapforge_shared_theme_save_persisted();
    }
    bool font_zoom_changed = false;
    if (app->ui_state_bridge.input.font_zoom_in_pressed) {
        font_zoom_changed = mapforge_shared_font_step_by(1) || font_zoom_changed;
    }
    if (app->ui_state_bridge.input.font_zoom_out_pressed) {
        font_zoom_changed = mapforge_shared_font_step_by(-1) || font_zoom_changed;
    }
    if (app->ui_state_bridge.input.font_zoom_reset_pressed) {
        font_zoom_changed = mapforge_shared_font_reset_zoom_step() || font_zoom_changed;
    }
    if (font_zoom_changed) {
        app_apply_shared_ui_font(app);
    }
    if (app->ui_state_bridge.input.toggle_playback_pressed && app->route_state_bridge.route.path.count >= 2) {
        app->route_state_bridge.playback_playing = !app->route_state_bridge.playback_playing;
    }
    if (app->ui_state_bridge.input.playback_step_forward && app->route_state_bridge.route.path.total_time_s > 0.0f) {
        app->route_state_bridge.playback_time_s += 5.0f;
        if (app->route_state_bridge.playback_time_s > app->route_state_bridge.route.path.total_time_s) {
            app->route_state_bridge.playback_time_s = app->route_state_bridge.route.path.total_time_s;
        }
    }
    if (app->ui_state_bridge.input.playback_step_back && app->route_state_bridge.route.path.total_time_s > 0.0f) {
        app->route_state_bridge.playback_time_s -= 5.0f;
        if (app->route_state_bridge.playback_time_s < 0.0f) {
            app->route_state_bridge.playback_time_s = 0.0f;
        }
    }
    if (app->ui_state_bridge.input.playback_speed_up) {
        app->route_state_bridge.playback_speed = app_next_playback_speed(app->route_state_bridge.playback_speed, 1);
    }
    if (app->ui_state_bridge.input.playback_speed_down) {
        app->route_state_bridge.playback_speed = app_next_playback_speed(app->route_state_bridge.playback_speed, -1);
    }

    return false;
}
