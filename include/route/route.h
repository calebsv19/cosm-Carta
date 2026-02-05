#ifndef MAPFORGE_ROUTE_ROUTE_H
#define MAPFORGE_ROUTE_ROUTE_H

#include "route/astar.h"
#include "route/graph.h"

#include <stdbool.h>
#include <stdint.h>

// Stores routing state and the current route path.
typedef struct RouteState {
    RouteGraph graph;
    RoutePath path;
    bool loaded;
    bool fastest;
    RouteTravelMode mode;
    bool has_start;
    bool has_goal;
    uint32_t start_node;
    uint32_t goal_node;
} RouteState;

// Initializes routing state.
void route_state_init(RouteState *state);

// Loads the graph for a region.
bool route_state_load_graph(RouteState *state, const char *path);

// Clears the current route path.
void route_state_clear(RouteState *state);

// Sets start and goal nodes and computes a route.
bool route_state_route(RouteState *state, uint32_t start_node, uint32_t goal_node);

// Frees routing state data.
void route_state_shutdown(RouteState *state);

#endif
