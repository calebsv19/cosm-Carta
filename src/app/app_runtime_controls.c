#include "app/app_internal.h"

#include "app/region_loader.h"
#include "core/log.h"
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

static int app_runtime_find_next_routable_region_index(int current_index) {
    int total = region_count();
    if (total <= 0) {
        return -1;
    }
    for (int step = 1; step <= total; ++step) {
        int candidate = (current_index + step) % total;
        const RegionInfo *info = region_get(candidate);
        if (info && region_has_graph(info)) {
            return candidate;
        }
    }
    return -1;
}

bool app_runtime_handle_global_controls(AppState *app) {
    if (!app) {
        return false;
    }

    if (app->input.toggle_debug_pressed) {
        app->overlay.enabled = !app->overlay.enabled;
    }
    if (app->input.toggle_single_line_pressed) {
        app->single_line = !app->single_line;
    }
    if (app->input.toggle_region_pressed) {
        int total_regions = region_count();
        if (total_regions <= 0) {
            log_error("No region packs found under '%s'", region_data_root());
            return true;
        }
        int next_index = app_runtime_find_next_routable_region_index(app->region_index);
        if (next_index < 0) {
            log_error("No routable regions found (missing graph/graph.bin). Build graph files to enable route placement.");
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
            for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
                tile_manager_shutdown(&app->tile_managers[i]);
                tile_manager_init(&app->tile_managers[i], 256, app->region.tiles_dir);
            }
            tile_loader_shutdown(&app->tile_loader);
            tile_loader_init(&app->tile_loader, app->region.tiles_dir);
            vk_tile_cache_clear_with_renderer(&app->vk_tile_cache, app->renderer.vk);
            app_clear_tile_queue(app);
            app->visible_valid = false;
            app->loading_expected = 0;
            app->loading_done = 0;
            app->loading_no_data_time = 0.0f;
            app->tile_request_id += 1;
            if (!app_load_route_graph(app)) {
                log_error("Route graph missing for region '%s'; skipping route interactions in this region.", app->region.name);
            }
            route_state_clear(&app->route);
            app_route_worker_clear(app);
            app_playback_reset(app);
            memset(&app->hover_anchor, 0, sizeof(app->hover_anchor));
            memset(&app->start_anchor, 0, sizeof(app->start_anchor));
            memset(&app->goal_anchor, 0, sizeof(app->goal_anchor));
            app->building_zoom_bias = app_building_zoom_bias_for_region(&app->region);
            app->road_zoom_bias = app_road_zoom_bias_for_region(&app->region);
            app_center_camera_on_region(&app->camera, &app->region, app->width, app->height);
        }
    }
    if (app->input.toggle_profile_pressed) {
        app->route.objective = app_runtime_next_route_objective(app->route.objective);
        if (app->route.has_start && app->route.has_goal) {
            app_route_schedule_recompute(app, 0.0);
        }
    }
    if (app->input.toggle_landuse_pressed) {
        app->show_landuse = !app->show_landuse;
    }
    if (app->input.toggle_building_fill_pressed) {
        app->building_fill_enabled = !app->building_fill_enabled;
    }
    if (app->input.toggle_polygon_outline_pressed) {
        app->polygon_outline_only = !app->polygon_outline_only;
    }
    if (app->input.theme_cycle_next_pressed) {
        mapforge_shared_theme_cycle_next();
        mapforge_shared_theme_save_persisted();
    }
    if (app->input.theme_cycle_prev_pressed) {
        mapforge_shared_theme_cycle_prev();
        mapforge_shared_theme_save_persisted();
    }
    if (app->input.toggle_playback_pressed && app->route.path.count >= 2) {
        app->playback_playing = !app->playback_playing;
    }
    if (app->input.playback_step_forward && app->route.path.total_time_s > 0.0f) {
        app->playback_time_s += 5.0f;
        if (app->playback_time_s > app->route.path.total_time_s) {
            app->playback_time_s = app->route.path.total_time_s;
        }
    }
    if (app->input.playback_step_back && app->route.path.total_time_s > 0.0f) {
        app->playback_time_s -= 5.0f;
        if (app->playback_time_s < 0.0f) {
            app->playback_time_s = 0.0f;
        }
    }
    if (app->input.playback_speed_up) {
        app->playback_speed = app_next_playback_speed(app->playback_speed, 1);
    }
    if (app->input.playback_speed_down) {
        app->playback_speed = app_next_playback_speed(app->playback_speed, -1);
    }

    return false;
}
