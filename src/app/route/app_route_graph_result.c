#include "app/app_internal.h"
#include "app/app_route_internal.h"

#include "core/log.h"

#include <string.h>

void app_route_apply_graph_result(AppState *app,
                                  bool have_graph_result,
                                  bool graph_result_ok,
                                  uint32_t graph_result_request_id,
                                  RouteState *graph_result_state,
                                  RouteSnapIndex *graph_result_snap_index) {
    if (!app || !have_graph_result || !graph_result_state || !graph_result_snap_index) {
        return;
    }

    if (graph_result_request_id == app->route_state_bridge.route_graph_load_request_id) {
        if (graph_result_ok) {
            route_state_shutdown(&app->route_state_bridge.route);
            app->route_state_bridge.route = *graph_result_state;
            memset(graph_result_state, 0, sizeof(*graph_result_state));
            app_route_snap_index_free(&app->route_state_bridge.route_snap_index);
            app->route_state_bridge.route_snap_index = *graph_result_snap_index;
            memset(graph_result_snap_index, 0, sizeof(*graph_result_snap_index));
        } else {
            log_error("Async route graph load failed for region: %s", app->region.name ? app->region.name : "unknown");
        }
        app->route_state_bridge.route_graph_loading = false;
    }

    route_state_shutdown(graph_result_state);
    app_route_snap_index_free(graph_result_snap_index);
}
