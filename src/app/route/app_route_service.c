#include "app/app_internal.h"

#include <math.h>
#include <string.h>

static bool app_route_service_anchor_changed(const RouteEndpointAnchor *current,
                                             uint32_t current_node,
                                             const RouteEndpointAnchor *next) {
    if (!current || !next) {
        return false;
    }
    return next->node != current_node ||
           fabsf(next->world_x - current->world_x) > 0.01f ||
           fabsf(next->world_y - current->world_y) > 0.01f;
}

static void app_route_service_schedule_if_ready(AppState *app,
                                                double recompute_debounce_sec) {
    if (!app || !app->route_state_bridge.route.has_start || !app->route_state_bridge.route.has_goal) {
        return;
    }
    app_route_schedule_recompute(app, recompute_debounce_sec);
}

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

void app_route_service_clear_route_selection(AppState *app) {
    if (!app) {
        return;
    }
    route_state_clear(&app->route_state_bridge.route);
    app_playback_reset(app);
    app->route_state_bridge.dragging_start = false;
    app->route_state_bridge.dragging_goal = false;
    memset(&app->route_state_bridge.start_anchor, 0, sizeof(app->route_state_bridge.start_anchor));
    memset(&app->route_state_bridge.goal_anchor, 0, sizeof(app->route_state_bridge.goal_anchor));
}

bool app_route_service_begin_endpoint_drag(AppState *app, bool set_start) {
    if (!app) {
        return false;
    }
    if (set_start) {
        app->route_state_bridge.dragging_start = true;
    } else {
        app->route_state_bridge.dragging_goal = true;
    }
    return true;
}

bool app_route_service_set_endpoint_anchor(AppState *app,
                                           bool set_start,
                                           const RouteEndpointAnchor *anchor,
                                           double recompute_debounce_sec) {
    if (!app || !anchor || !anchor->valid) {
        return false;
    }
    if (set_start) {
        app->route_state_bridge.route.start_node = anchor->node;
        app->route_state_bridge.route.has_start = true;
        app->route_state_bridge.start_anchor = *anchor;
    } else {
        app->route_state_bridge.route.goal_node = anchor->node;
        app->route_state_bridge.route.has_goal = true;
        app->route_state_bridge.goal_anchor = *anchor;
    }
    app_route_service_schedule_if_ready(app, recompute_debounce_sec);
    return true;
}

bool app_route_service_update_drag_endpoint(AppState *app,
                                            const RouteEndpointAnchor *anchor,
                                            double recompute_debounce_sec) {
    if (!app || !anchor || !anchor->valid) {
        return false;
    }

    bool changed = false;
    if (app->route_state_bridge.dragging_start &&
        app_route_service_anchor_changed(&app->route_state_bridge.start_anchor,
                                         app->route_state_bridge.route.start_node,
                                         anchor)) {
        app->route_state_bridge.route.start_node = anchor->node;
        app->route_state_bridge.route.has_start = true;
        app->route_state_bridge.start_anchor = *anchor;
        changed = true;
    }
    if (app->route_state_bridge.dragging_goal &&
        app_route_service_anchor_changed(&app->route_state_bridge.goal_anchor,
                                         app->route_state_bridge.route.goal_node,
                                         anchor)) {
        app->route_state_bridge.route.goal_node = anchor->node;
        app->route_state_bridge.route.has_goal = true;
        app->route_state_bridge.goal_anchor = *anchor;
        changed = true;
    }
    if (changed) {
        app_route_service_schedule_if_ready(app, recompute_debounce_sec);
    }
    return changed;
}

bool app_route_service_finish_endpoint_drag(AppState *app,
                                            bool set_start,
                                            double recompute_debounce_sec) {
    if (!app) {
        return false;
    }
    bool was_dragging = set_start
        ? app->route_state_bridge.dragging_start
        : app->route_state_bridge.dragging_goal;
    if (set_start) {
        app->route_state_bridge.dragging_start = false;
    } else {
        app->route_state_bridge.dragging_goal = false;
    }
    if (was_dragging) {
        app_route_service_schedule_if_ready(app, recompute_debounce_sec);
    }
    return was_dragging;
}
