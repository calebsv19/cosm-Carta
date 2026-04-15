#include "app/app_internal.h"
#include "app/app_route_internal.h"

#include <string.h>

static void app_route_path_move(RoutePath *dst, RoutePath *src) {
    if (!dst || !src) {
        return;
    }
    route_path_free(dst);
    *dst = *src;
    memset(src, 0, sizeof(*src));
}

static void app_route_alternatives_move(RouteAlternativeSet *dst, RouteAlternativeSet *src) {
    if (!dst || !src) {
        return;
    }
    for (uint32_t i = 0; i < ROUTE_ALTERNATIVE_MAX; ++i) {
        route_path_free(&dst->paths[i]);
    }
    *dst = *src;
    memset(src, 0, sizeof(*src));
}

static int app_route_objective_index(RouteObjective objective) {
    int idx = (int)objective;
    if (idx < 0 || idx >= (int)ROUTE_OBJECTIVE_COUNT) {
        return -1;
    }
    return idx;
}

void app_route_apply_worker_result(AppState *app, RouteComputeResult *result) {
    if (!app || !result) {
        return;
    }
    if (!app_worker_contract_route_result_is_current(app, result->request_id)) {
        app_route_result_clear(result);
        return;
    }
    if (!result->ok) {
        app_route_result_clear(result);
        return;
    }

    bool visible_by_objective[ROUTE_OBJECTIVE_COUNT];
    for (uint32_t i = 0; i < ROUTE_OBJECTIVE_COUNT; ++i) {
        visible_by_objective[i] = true;
    }
    for (uint32_t i = 0; i < app->route_state_bridge.route.alternatives.count && i < ROUTE_ALTERNATIVE_MAX; ++i) {
        int objective_idx = app_route_objective_index(app->route_state_bridge.route.alternatives.objectives[i]);
        if (objective_idx >= 0) {
            visible_by_objective[objective_idx] = app->route_state_bridge.route_alt_visible[i];
        }
    }

    app_route_path_move(&app->route_state_bridge.route.path, &result->path);
    app_route_path_move(&app->route_state_bridge.route.drive_path, &result->drive_path);
    app_route_path_move(&app->route_state_bridge.route.walk_path, &result->walk_path);
    app_route_alternatives_move(&app->route_state_bridge.route.alternatives, &result->alternatives);
    app->ui_state_bridge.hud_route_panel_layout_dirty = true;
    for (uint32_t i = 0; i < ROUTE_ALTERNATIVE_MAX; ++i) {
        if (i < app->route_state_bridge.route.alternatives.count) {
            int objective_idx = app_route_objective_index(app->route_state_bridge.route.alternatives.objectives[i]);
            app->route_state_bridge.route_alt_visible[i] = objective_idx >= 0 ? visible_by_objective[objective_idx] : true;
        } else {
            app->route_state_bridge.route_alt_visible[i] = false;
        }
    }
    app->route_state_bridge.route.start_node = result->start_node;
    app->route_state_bridge.route.goal_node = result->goal_node;
    app->route_state_bridge.route.has_start = true;
    app->route_state_bridge.route.has_goal = true;
    app->route_state_bridge.route.objective = result->objective;
    app->route_state_bridge.route.mode = result->mode;
    app->route_state_bridge.route.has_transfer = result->has_transfer;
    app->route_state_bridge.route.transfer_node = result->transfer_node;
    app_worker_contract_note_route_applied(app, result->request_id);
    app_playback_reset(app);
}
