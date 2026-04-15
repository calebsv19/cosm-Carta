#include "app/app_internal.h"

const RoutePath *app_route_primary_path(const AppState *app, uint32_t *out_alt_index) {
    if (out_alt_index) {
        *out_alt_index = UINT32_MAX;
    }
    if (!app) {
        return NULL;
    }

    bool has_any_alternatives = app->route_state_bridge.route.alternatives.count > 0;
    for (uint32_t i = 0; i < app->route_state_bridge.route.alternatives.count && i < ROUTE_ALTERNATIVE_MAX; ++i) {
        if (app->route_state_bridge.route.alternatives.objectives[i] != app->route_state_bridge.route.objective) {
            continue;
        }
        if (!app->route_state_bridge.route_alt_visible[i]) {
            continue;
        }
        if (app->route_state_bridge.route.alternatives.paths[i].count < 2) {
            continue;
        }
        if (out_alt_index) {
            *out_alt_index = i;
        }
        return &app->route_state_bridge.route.alternatives.paths[i];
    }

    for (uint32_t i = 0; i < app->route_state_bridge.route.alternatives.count && i < ROUTE_ALTERNATIVE_MAX; ++i) {
        if (!app->route_state_bridge.route_alt_visible[i]) {
            continue;
        }
        if (app->route_state_bridge.route.alternatives.paths[i].count < 2) {
            continue;
        }
        if (out_alt_index) {
            *out_alt_index = i;
        }
        return &app->route_state_bridge.route.alternatives.paths[i];
    }

    if (has_any_alternatives) {
        return NULL;
    }

    if (app->route_state_bridge.route.path.count >= 2) {
        return &app->route_state_bridge.route.path;
    }
    return NULL;
}

bool app_recompute_route(AppState *app) {
    if (!app || !app->route_state_bridge.route.has_start || !app->route_state_bridge.route.has_goal) {
        return false;
    }

    app_route_schedule_recompute(app, 0.0);
    return true;
}
