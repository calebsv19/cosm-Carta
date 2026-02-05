#include "route/route.h"

#include "core/log.h"

#include <string.h>

void route_state_init(RouteState *state) {
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->mode = ROUTE_MODE_CAR;
}

bool route_state_load_graph(RouteState *state, const char *path) {
    if (!state || !path) {
        return false;
    }

    route_graph_free(&state->graph);
    route_path_free(&state->path);
    state->loaded = route_graph_load(path, &state->graph);
    state->has_start = false;
    state->has_goal = false;
    return state->loaded;
}

void route_state_clear(RouteState *state) {
    if (!state) {
        return;
    }

    route_path_free(&state->path);
    state->has_start = false;
    state->has_goal = false;
}

bool route_state_route(RouteState *state, uint32_t start_node, uint32_t goal_node) {
    if (!state || !state->loaded) {
        return false;
    }

    state->start_node = start_node;
    state->goal_node = goal_node;
    state->has_start = true;
    state->has_goal = true;

    return route_astar(&state->graph, start_node, goal_node, state->fastest, state->mode, &state->path);
}

void route_state_shutdown(RouteState *state) {
    if (!state) {
        return;
    }

    route_graph_free(&state->graph);
    route_path_free(&state->path);
    memset(state, 0, sizeof(*state));
}
