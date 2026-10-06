#include "route/cost.h"

#include "map/mft_loader.h"

#include <math.h>
#include <stdlib.h>

// Provides the coarse access fallback for legacy graphs.
bool route_class_allowed(RouteTravelMode mode, uint8_t road_class) {
    if (mode == ROUTE_MODE_WALK) {
        return road_class != ROAD_CLASS_MOTORWAY && road_class != ROAD_CLASS_TRUNK;
    }
    return road_class != ROAD_CLASS_FOOTWAY && road_class != ROAD_CLASS_PATH;
}

// Checks explicit mode permissions or legacy class policy.
bool route_edge_allowed(const RouteGraph *graph, uint32_t edge, RouteTravelMode mode) {
    if (!graph || edge >= graph->edge_count) return false;
    return graph->edge_access ? (graph->edge_access[edge] & (mode == ROUTE_MODE_WALK ? 2u : 1u)) != 0u
        : route_class_allowed(mode, graph->edge_class[edge]);
}

// Resolves traversal speed for optimization and ETA.
float route_edge_speed_mps(const RouteGraph *graph, uint32_t edge, RouteTravelMode mode) {
    if (!graph || edge >= graph->edge_count) return 0.0f;
    if (mode == ROUTE_MODE_WALK) return ROUTE_WALK_SPEED_MPS;
    float speed = graph->edge_speed[edge];
    return isfinite(speed) && speed > 0.1f ? speed : 0.1f;
}

// Computes static mode-specific traversal seconds.
float route_edge_time_s(const RouteGraph *graph, uint32_t edge, RouteTravelMode mode) {
    float speed = route_edge_speed_mps(graph, edge, mode);
    return speed > 0.0f ? graph->edge_length[edge] / speed : INFINITY;
}

// Scores one edge under the common objective contract.
float route_edge_objective_cost(const RouteGraph *graph, uint32_t edge,
                                RouteObjective objective, RouteTravelMode mode) {
    if (!graph || edge >= graph->edge_count) return INFINITY;
    float length = graph->edge_length[edge];
    float time_s = route_edge_time_s(graph, edge, mode);
    switch (objective) {
        case ROUTE_OBJECTIVE_SHORTEST_DISTANCE:
            return length;
        case ROUTE_OBJECTIVE_LOWEST_TIME:
            return time_s;
        case ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN: {
            /* Incline tags are a climb proxy until DEM-backed ascent is available. */
            float grade = graph->edge_grade ? graph->edge_grade[edge] : 0.0f;
            float gain = fmaxf(grade, 0.0f) * length;
            float penalty = graph->edge_penalty ? fmaxf(graph->edge_penalty[edge], 0.0f) : 0.0f;
            return length + gain * 8.0f + penalty * length * 0.5f;
        }
        case ROUTE_OBJECTIVE_MOST_TIME_ABOVE_SPEED_THRESHOLD: {
            if (mode == ROUTE_MODE_WALK) return time_s;
            float limit = graph->edge_speed_limit ? graph->edge_speed_limit[edge] : 0.0f;
            return time_s * (limit >= ROUTE_FAST_ROAD_THRESHOLD_MPS ? 0.6f : 1.8f);
        }
        default:
            return INFINITY;
    }
}

// Selects the cheapest allowed parallel edge for a directed endpoint segment.
bool route_find_best_edge(const RouteGraph *graph, uint32_t from, uint32_t to,
                          RouteObjective objective, RouteTravelMode mode, uint32_t *out_edge) {
    if (!graph || !out_edge || from >= graph->node_count || to >= graph->node_count) return false;
    float best_cost = INFINITY;
    uint32_t best_edge = UINT32_MAX;
    for (uint32_t edge = graph->edge_start[from]; edge < graph->edge_start[from + 1u]; ++edge) {
        if (graph->edge_to[edge] != to || !route_edge_allowed(graph, edge, mode)) continue;
        float cost = route_edge_objective_cost(graph, edge, objective, mode);
        if (cost < best_cost) { best_cost = cost; best_edge = edge; }
    }
    if (best_edge == UINT32_MAX) return false;
    *out_edge = best_edge;
    return true;
}

// Resolves the exact selected directed edge for a path step.
bool route_path_edge_index(const RouteGraph *graph, const RoutePath *path,
                           uint32_t step, uint32_t *out_edge) {
    if (!graph || !path || !out_edge || step + 1u >= path->count) return false;
    uint32_t from = path->nodes[step];
    uint32_t to = path->nodes[step + 1u];
    if (from >= graph->node_count || to >= graph->node_count) return false;
    if (path->edges) {
        uint32_t edge = path->edges[step];
        if (edge < graph->edge_start[from] || edge >= graph->edge_start[from + 1u] ||
            graph->edge_to[edge] != to) return false;
        *out_edge = edge;
        return true;
    }
    for (uint32_t edge = graph->edge_start[from]; edge < graph->edge_start[from + 1u]; ++edge) {
        if (graph->edge_to[edge] == to) {
            *out_edge = edge;
            return true;
        }
    }
    return false;
}

// Selects the recorded per-edge mode, or the caller's mode for a legacy path.
static RouteTravelMode route_path_step_mode(const RoutePath *path, uint32_t step, RouteTravelMode mode) {
    return path->edge_modes ? (RouteTravelMode)path->edge_modes[step] : mode;
}

// Rebuilds physical length and per-leg time accumulators.
bool route_path_compute_metrics(const RouteGraph *graph, RoutePath *path, RouteTravelMode mode) {
    if (!graph || !path || path->count == 0u || !path->nodes) return false;
    free(path->cumulative_length_m);
    free(path->cumulative_time_s);
    path->cumulative_length_m = calloc(path->count, sizeof(float));
    path->cumulative_time_s = calloc(path->count, sizeof(float));
    path->total_length_m = 0.0f;
    path->total_time_s = 0.0f;
    if (!path->cumulative_length_m || !path->cumulative_time_s) return false;
    for (uint32_t step = 0u; step + 1u < path->count; ++step) {
        uint32_t edge = 0u;
        if (!route_path_edge_index(graph, path, step, &edge)) return false;
        path->total_length_m += graph->edge_length[edge];
        path->total_time_s += route_edge_time_s(graph, edge, route_path_step_mode(path, step, mode));
        path->cumulative_length_m[step + 1u] = path->total_length_m;
        path->cumulative_time_s[step + 1u] = path->total_time_s;
    }
    return true;
}

// Scores selected path edges using their recorded travel modes.
float route_path_objective_cost(const RouteGraph *graph, const RoutePath *path,
                                RouteObjective objective, RouteTravelMode mode) {
    if (!graph || !path || path->count == 0u) return INFINITY;
    float cost = 0.0f;
    for (uint32_t step = 0u; step + 1u < path->count; ++step) {
        uint32_t edge = 0u;
        if (!route_path_edge_index(graph, path, step, &edge)) return INFINITY;
        cost += route_edge_objective_cost(graph, edge, objective, route_path_step_mode(path, step, mode));
    }
    return cost;
}
