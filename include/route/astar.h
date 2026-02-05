#ifndef MAPFORGE_ROUTE_ASTAR_H
#define MAPFORGE_ROUTE_ASTAR_H

#include "route/graph.h"

#include <stdbool.h>
#include <stdint.h>

// Stores a computed route path.
typedef struct RoutePath {
    uint32_t *nodes;
    uint32_t count;
    float total_length_m;
    float total_time_s;
    float *cumulative_length_m;
    float *cumulative_time_s;
} RoutePath;

// Enumerates access modes for routing.
typedef enum RouteTravelMode {
    ROUTE_MODE_CAR = 0,
    ROUTE_MODE_WALK = 1
} RouteTravelMode;

// Frees memory owned by a route path.
void route_path_free(RoutePath *path);

// Computes a shortest or fastest route between two nodes.
bool route_astar(const RouteGraph *graph, uint32_t start, uint32_t goal, bool fastest, RouteTravelMode mode, RoutePath *out_path);

#endif
