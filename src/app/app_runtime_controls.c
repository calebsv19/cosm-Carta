#include "app/app_internal.h"

#include "ui/shared_theme_font_adapter.h"

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

bool app_runtime_handle_global_controls(AppState *app) {
    if (!app) {
        return false;
    }

    bool ingest_consumed = app_runtime_ingest_tick(app);

    if (app->ui_state_bridge.input.toggle_debug_pressed) {
        app->ui_state_bridge.overlay.enabled = !app->ui_state_bridge.overlay.enabled;
    }
    if (app->ui_state_bridge.input.toggle_single_line_pressed) {
        app->single_line = !app->single_line;
    }
    if (app->ui_state_bridge.input.toggle_region_pressed) {
        (void)app_runtime_cycle_next_region(app);
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

    return ingest_consumed;
}
