#include "app/app_internal.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int g_schedule_calls = 0;
static int g_playback_reset_calls = 0;

void app_route_schedule_recompute(AppState *app, double debounce_sec) {
    (void)app;
    (void)debounce_sec;
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

    return 0;
}
