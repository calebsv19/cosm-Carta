#ifndef MAPFORGE_ROUTE_COST_H
#define MAPFORGE_ROUTE_COST_H

#include "route/astar.h"

#define ROUTE_WALK_SPEED_MPS 1.4f
#define ROUTE_FAST_ROAD_THRESHOLD_MPS 13.4112f

// Resolves explicit per-mode permission with legacy class fallback.
bool route_edge_allowed(const RouteGraph *graph, uint32_t edge, RouteTravelMode mode);

// Provides the coarse access fallback for legacy graphs.
bool route_class_allowed(RouteTravelMode mode, uint8_t road_class);

// Resolves a mode-specific speed used by both optimization and ETA.
float route_edge_speed_mps(const RouteGraph *graph, uint32_t edge, RouteTravelMode mode);

// Returns static traversal time for one edge under the selected travel mode.
float route_edge_time_s(const RouteGraph *graph, uint32_t edge, RouteTravelMode mode);

// Defines the single objective cost used by routing and endpoint selection.
float route_edge_objective_cost(const RouteGraph *graph, uint32_t edge,
                                RouteObjective objective, RouteTravelMode mode);

// Selects the cheapest allowed parallel edge for a directed endpoint segment.
bool route_find_best_edge(const RouteGraph *graph, uint32_t from, uint32_t to,
                          RouteObjective objective, RouteTravelMode mode, uint32_t *out_edge);

// Resolves the selected directed edge for a path step, including legacy node-only paths.
bool route_path_edge_index(const RouteGraph *graph, const RoutePath *path,
                           uint32_t step, uint32_t *out_edge);

// Rebuilds physical lengths and mode-specific cumulative times for a path.
bool route_path_compute_metrics(const RouteGraph *graph, RoutePath *path, RouteTravelMode mode);

// Scores a complete path with the same objective and per-leg modes as the solver.
float route_path_objective_cost(const RouteGraph *graph, const RoutePath *path,
                                RouteObjective objective, RouteTravelMode mode);

#endif
