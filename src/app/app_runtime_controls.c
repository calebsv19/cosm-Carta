#include "app/app_internal.h"
#include "app/app_map_viewport_internal.h"
#include "app/app_runtime_ingest_internal.h"

#include "../camera/camera_viewport_bridge.h"
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

static void app_runtime_rotate_heading(AppState *app, float delta_rad) {
    SDL_FRect viewport = app_map_viewport_rect(app);
    if (!app || viewport.w <= 0.0f || viewport.h <= 0.0f) {
        return;
    }

    Camera *camera = &app->view_state_bridge.camera;
    float next_heading = camera->heading_target_rad + delta_rad;
    CoreResult result = camera_viewport_bridge_rotate_target_at_anchor(camera,
                                                                       viewport.w * 0.5f,
                                                                       viewport.h * 0.5f,
                                                                       (int)viewport.w,
                                                                       (int)viewport.h,
                                                                       next_heading);
    if (result.code != CORE_OK) {
        camera_set_heading_target(camera, next_heading);
    }
}

static void app_runtime_step_zoom(AppState *app, float zoom_delta) {
    SDL_FRect viewport = app_map_viewport_rect(app);
    if (!app || viewport.w <= 0.0f || viewport.h <= 0.0f) {
        return;
    }

    Camera *camera = &app->view_state_bridge.camera;
    float next_zoom = app_clampf(camera->zoom_target + zoom_delta,
                                 (float)CAMERA_ZOOM_MIN_LEVEL,
                                 (float)CAMERA_ZOOM_MAX_LEVEL);
    (void)camera_viewport_bridge_zoom_target_at_anchor(camera,
                                                       viewport.w * 0.5f,
                                                       viewport.h * 0.5f,
                                                       (int)viewport.w,
                                                       (int)viewport.h,
                                                       next_zoom);
}

bool app_runtime_handle_global_controls(AppState *app) {
    if (!app) {
        return false;
    }

    const float kRotateStepRad = 0.0872664626f;
    const RoutePath *active_path = app_route_primary_path(app, NULL);
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
    if (app->ui_state_bridge.input.toggle_follow_preview_pressed) {
        if (app->route_state_bridge.preview_follow_enabled) {
            app->route_state_bridge.preview_follow_enabled = false;
        } else if (active_path && active_path->count >= 2u) {
            app->route_state_bridge.preview_follow_enabled = true;
        }
    }
    if (app->ui_state_bridge.input.toggle_follow_heading_mode_pressed) {
        app->route_state_bridge.preview_heading_up = !app->route_state_bridge.preview_heading_up;
        if (app->route_state_bridge.preview_follow_enabled &&
            !app->route_state_bridge.preview_heading_up) {
            camera_set_heading_target(&app->view_state_bridge.camera, 0.0f);
        }
    }
    if (app->ui_state_bridge.input.toggle_playback_pressed && active_path && active_path->count >= 2u) {
        app->route_state_bridge.playback_playing = !app->route_state_bridge.playback_playing;
    }
    if (!app->ingest_edit_mode && app->ui_state_bridge.input.zoom_step_in_pressed) {
        app_runtime_step_zoom(app, 0.25f);
    }
    if (!app->ingest_edit_mode && app->ui_state_bridge.input.zoom_step_out_pressed) {
        app_runtime_step_zoom(app, -0.25f);
    }
    if (app->ui_state_bridge.input.playback_step_forward && active_path && active_path->total_time_s > 0.0f) {
        app->route_state_bridge.playback_time_s += 5.0f;
        if (app->route_state_bridge.playback_time_s > active_path->total_time_s) {
            app->route_state_bridge.playback_time_s = active_path->total_time_s;
        }
    }
    if (app->ui_state_bridge.input.playback_step_back && active_path && active_path->total_time_s > 0.0f) {
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
    if (!app->ingest_edit_mode && app->ui_state_bridge.input.rotate_heading_left_pressed) {
        if (app->route_state_bridge.preview_follow_enabled) {
            app->route_state_bridge.preview_follow_enabled = false;
        }
        app_runtime_rotate_heading(app, -kRotateStepRad);
    }
    if (!app->ingest_edit_mode && app->ui_state_bridge.input.rotate_heading_right_pressed) {
        if (app->route_state_bridge.preview_follow_enabled) {
            app->route_state_bridge.preview_follow_enabled = false;
        }
        app_runtime_rotate_heading(app, kRotateStepRad);
    }
    if (!app->ingest_edit_mode && app->ui_state_bridge.input.rotate_heading_reset_pressed) {
        if (app->route_state_bridge.preview_follow_enabled) {
            app->route_state_bridge.preview_follow_enabled = false;
        }
        camera_set_heading_target(&app->view_state_bridge.camera, 0.0f);
    }

    return ingest_consumed;
}
