#include "app/app_internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int g_schedule_calls = 0;
static int g_playback_reset_calls = 0;
static double g_last_debounce = -1.0;

void app_route_schedule_recompute(AppState *app, double debounce_sec) {
    (void)app;
    g_last_debounce = debounce_sec;
    g_schedule_calls += 1;
}

void app_playback_reset(AppState *app) {
    (void)app;
    g_playback_reset_calls += 1;
}

void route_path_free(RoutePath *path) {
    if (!path) {
        return;
    }
    free(path->nodes);
    free(path->cumulative_time_s);
    memset(path, 0, sizeof(*path));
}

void route_state_clear(RouteState *state) {
    if (!state) {
        return;
    }
    route_path_free(&state->path);
    route_path_free(&state->drive_path);
    route_path_free(&state->walk_path);
    memset(state, 0, sizeof(*state));
}

static void seed_route_path(RoutePath *path) {
    assert(path);
    memset(path, 0, sizeof(*path));
    path->count = 2u;
    path->nodes = (uint32_t *)malloc(sizeof(uint32_t) * 2u);
    path->cumulative_time_s = (float *)malloc(sizeof(float) * 2u);
    assert(path->nodes);
    assert(path->cumulative_time_s);
    path->nodes[0] = 1u;
    path->nodes[1] = 2u;
    path->cumulative_time_s[0] = 0.0f;
    path->cumulative_time_s[1] = 1.0f;
}

static void reset_stub_counters(void) {
    g_schedule_calls = 0;
    g_playback_reset_calls = 0;
    g_last_debounce = -1.0;
}

static RouteEndpointAnchor make_anchor(uint32_t node, float x, float y) {
    RouteEndpointAnchor anchor;
    memset(&anchor, 0, sizeof(anchor));
    anchor.valid = true;
    anchor.node = node;
    anchor.world_x = x;
    anchor.world_y = y;
    return anchor;
}

static void test_endpoint_service_set_and_drag(void) {
    AppState app;
    memset(&app, 0, sizeof(app));
    RouteEndpointAnchor start = make_anchor(10u, 100.0f, 200.0f);
    RouteEndpointAnchor goal = make_anchor(20u, 300.0f, 400.0f);
    RouteEndpointAnchor moved_goal = make_anchor(21u, 310.0f, 405.0f);

    reset_stub_counters();
    assert(app_route_service_set_endpoint_anchor(&app, true, &start, 0.0));
    assert(app.route_state_bridge.route.has_start);
    assert(!app.route_state_bridge.route.has_goal);
    assert(app.route_state_bridge.route.start_node == 10u);
    assert(app.route_state_bridge.start_anchor.world_x == 100.0f);
    assert(g_schedule_calls == 0);

    assert(app_route_service_set_endpoint_anchor(&app, false, &goal, 0.0));
    assert(app.route_state_bridge.route.has_goal);
    assert(app.route_state_bridge.route.goal_node == 20u);
    assert(g_schedule_calls == 1);
    assert(g_last_debounce == 0.0);

    assert(app_route_service_begin_endpoint_drag(&app, false));
    assert(app.route_state_bridge.dragging_goal);
    assert(app_route_service_update_drag_endpoint(&app, &moved_goal, 0.045));
    assert(app.route_state_bridge.route.goal_node == 21u);
    assert(app.route_state_bridge.goal_anchor.world_x == 310.0f);
    assert(g_schedule_calls == 2);
    assert(g_last_debounce == 0.045);

    assert(app_route_service_finish_endpoint_drag(&app, false, 0.0));
    assert(!app.route_state_bridge.dragging_goal);
    assert(g_schedule_calls == 3);
    assert(g_last_debounce == 0.0);
}

static void test_endpoint_service_clear_route_selection(void) {
    AppState app;
    memset(&app, 0, sizeof(app));
    app.route_state_bridge.route.has_start = true;
    app.route_state_bridge.route.has_goal = true;
    app.route_state_bridge.route.start_node = 1u;
    app.route_state_bridge.route.goal_node = 2u;
    app.route_state_bridge.dragging_start = true;
    app.route_state_bridge.dragging_goal = true;
    app.route_state_bridge.start_anchor = make_anchor(1u, 10.0f, 11.0f);
    app.route_state_bridge.goal_anchor = make_anchor(2u, 20.0f, 21.0f);

    reset_stub_counters();
    app_route_service_clear_route_selection(&app);
    assert(!app.route_state_bridge.route.has_start);
    assert(!app.route_state_bridge.route.has_goal);
    assert(!app.route_state_bridge.dragging_start);
    assert(!app.route_state_bridge.dragging_goal);
    assert(!app.route_state_bridge.start_anchor.valid);
    assert(!app.route_state_bridge.goal_anchor.valid);
    assert(g_playback_reset_calls == 1);
}

int main(void) {
    AppState app;
    memset(&app, 0, sizeof(app));

    app.route_state_bridge.route.alternatives.count = 2u;
    app.route_state_bridge.route.alternatives.objectives[0] = ROUTE_OBJECTIVE_SHORTEST_DISTANCE;
    app.route_state_bridge.route.alternatives.objectives[1] = ROUTE_OBJECTIVE_LOWEST_TIME;
    app.route_state_bridge.route.objective = ROUTE_OBJECTIVE_SHORTEST_DISTANCE;

    assert(!app_route_service_select_alternative(&app, 5u));
    assert(!app_route_service_toggle_alternative_visibility(&app, 5u));

    reset_stub_counters();
    seed_route_path(&app.route_state_bridge.route.drive_path);
    seed_route_path(&app.route_state_bridge.route.walk_path);
    app.route_state_bridge.route.has_start = false;
    app.route_state_bridge.route.has_goal = false;
    app.ui_state_bridge.hud_route_panel_layout_dirty = false;
    assert(app_route_service_select_alternative(&app, 1u));
    assert(app.route_state_bridge.route.objective == ROUTE_OBJECTIVE_LOWEST_TIME);
    assert(app.route_state_bridge.route_alt_visible[1]);
    assert(app.route_state_bridge.route.drive_path.count == 0u);
    assert(app.route_state_bridge.route.walk_path.count == 0u);
    assert(!app.route_state_bridge.route.has_transfer);
    assert(app.route_state_bridge.route.transfer_node == 0u);
    assert(g_playback_reset_calls == 1);
    assert(g_schedule_calls == 0);
    assert(app.ui_state_bridge.hud_route_panel_layout_dirty);

    reset_stub_counters();
    app.route_state_bridge.route.has_start = true;
    app.route_state_bridge.route.has_goal = true;
    app.ui_state_bridge.hud_route_panel_layout_dirty = false;
    assert(app_route_service_select_alternative(&app, 0u));
    assert(app.route_state_bridge.route.objective == ROUTE_OBJECTIVE_SHORTEST_DISTANCE);
    assert(g_playback_reset_calls == 1);
    assert(g_schedule_calls == 1);
    assert(app.ui_state_bridge.hud_route_panel_layout_dirty);

    app.route_state_bridge.route_alt_visible[0] = false;
    app.ui_state_bridge.hud_route_panel_layout_dirty = false;
    assert(app_route_service_toggle_alternative_visibility(&app, 0u));
    assert(app.route_state_bridge.route_alt_visible[0]);
    assert(app.ui_state_bridge.hud_route_panel_layout_dirty);
    app.ui_state_bridge.hud_route_panel_layout_dirty = false;
    assert(app_route_service_toggle_alternative_visibility(&app, 0u));
    assert(!app.route_state_bridge.route_alt_visible[0]);
    assert(app.ui_state_bridge.hud_route_panel_layout_dirty);

    test_endpoint_service_set_and_drag();
    test_endpoint_service_clear_route_selection();

    return 0;
}
