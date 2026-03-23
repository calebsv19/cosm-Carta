#include "app/app_internal.h"

bool app_route_service_select_alternative(AppState *app, uint32_t alt_index) {
    if (!app || alt_index >= app->route_state_bridge.route.alternatives.count || alt_index >= ROUTE_ALTERNATIVE_MAX) {
        return false;
    }

    RouteObjective next_objective = app->route_state_bridge.route.alternatives.objectives[alt_index];
    bool objective_changed = app->route_state_bridge.route.objective != next_objective;
    app->route_state_bridge.route.objective = next_objective;
    app->route_state_bridge.route_alt_visible[alt_index] = true;
    if (objective_changed) {
        route_path_free(&app->route_state_bridge.route.drive_path);
        route_path_free(&app->route_state_bridge.route.walk_path);
        app->route_state_bridge.route.has_transfer = false;
        app->route_state_bridge.route.transfer_node = 0;
    }
    if (app->route_state_bridge.route.has_start && app->route_state_bridge.route.has_goal) {
        app_route_schedule_recompute(app, 0.0);
    }
    app_playback_reset(app);
    app->ui_state_bridge.hud_route_panel_layout_dirty = true;
    return true;
}

bool app_route_service_toggle_alternative_visibility(AppState *app, uint32_t alt_index) {
    if (!app || alt_index >= ROUTE_ALTERNATIVE_MAX) {
        return false;
    }
    if (alt_index >= app->route_state_bridge.route.alternatives.count) {
        return false;
    }

    app->route_state_bridge.route_alt_visible[alt_index] = !app->route_state_bridge.route_alt_visible[alt_index];
    app->ui_state_bridge.hud_route_panel_layout_dirty = true;
    return true;
}
