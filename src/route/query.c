#include "route/query.h"
#include "map/geodesic.h"
#include "route/cost.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Resolves a nearest legal directed road segment in ground meters.
bool route_query_snap(const RouteGraph *graph, MercatorMeters requested, RouteTravelMode mode, double limit_m,
                      RouteEndpointAnchor *out) {
    if (!graph || !out || !graph->node_count || !isfinite(requested.x) || !isfinite(requested.y))
        return false;
    memset(out, 0, sizeof(*out));
    double best = INFINITY;
    for (uint32_t from = 0; from < graph->node_count; ++from) {
        for (uint32_t edge = graph->edge_start[from]; edge < graph->edge_start[from + 1u]; ++edge) {
            if (!route_edge_allowed(graph, edge, mode))
                continue;
            uint32_t to = graph->edge_to[edge];
            double ax = graph->node_x[from], ay = graph->node_y[from], dx = graph->node_x[to] - ax,
                   dy = graph->node_y[to] - ay;
            double length = dx * dx + dy * dy;
            if (length < 0.000001)
                continue;
            double t = ((requested.x - ax) * dx + (requested.y - ay) * dy) / length;
            t = fmax(0.0, fmin(1.0, t));
            double x = ax + t * dx, y = ay + t * dy, ex = requested.x - x, ey = requested.y - y,
                   d2 = ex * ex + ey * ey;
            if (d2 >= best)
                continue;
            best = d2;
            out->valid = true;
            out->on_edge = true;
            out->edge_from = from;
            out->edge_to = to;
            out->node = t < 0.5 ? from : to;
            out->world_x = (float)x;
            out->world_y = (float)y;
            out->dist_to_from_m = graph->edge_length[edge] * (float)t;
            out->dist_to_to_m = graph->edge_length[edge] * (float)(1.0 - t);
        }
    }
    if (!out->valid)
        return false;
    out->requested_x = requested.x;
    out->requested_y = requested.y;
    out->connector_m =
        mercator_ground_distance_meters(requested, (MercatorMeters){out->world_x, out->world_y});
    if (out->connector_m > limit_m) {
        out->valid = false;
        return false;
    }
    return true;
}

// Stores a permitted partial segment toward or away from a road endpoint.
typedef struct QueryOption {
    uint32_t node, edge;
    float length, time, cost;
} QueryOption;

// Builds directed start/goal options without taking an illegal reverse leg.
static uint32_t query_options(const RouteGraph *graph, const RouteEndpointAnchor *anchor, bool start,
                              RouteTravelMode mode, RouteObjective objective, QueryOption options[2]) {
    if (!anchor->on_edge) {
        options[0] = (QueryOption){anchor->node, UINT32_MAX, 0, 0, 0};
        return 1u;
    }
    uint32_t count = 0u, edge;
    if (route_find_best_edge(graph, anchor->edge_from, anchor->edge_to, objective, mode, &edge)) {
        float length = start ? anchor->dist_to_to_m : anchor->dist_to_from_m;
        options[count++] = (QueryOption){start ? anchor->edge_to : anchor->edge_from, edge, length,
                                         length / route_edge_speed_mps(graph, edge, mode),
                                         route_edge_objective_cost(graph, edge, objective, mode) * length /
                                             graph->edge_length[edge]};
    }
    if (route_find_best_edge(graph, anchor->edge_to, anchor->edge_from, objective, mode, &edge)) {
        float length = start ? anchor->dist_to_from_m : anchor->dist_to_to_m;
        options[count++] = (QueryOption){start ? anchor->edge_from : anchor->edge_to, edge, length,
                                         length / route_edge_speed_mps(graph, edge, mode),
                                         route_edge_objective_cost(graph, edge, objective, mode) * length /
                                             graph->edge_length[edge]};
    }
    return count;
}

// Allocates an explicit geometry path that retains valid graph node identities.
static bool query_allocate(RoutePath *path, uint32_t count) {
    memset(path, 0, sizeof(*path));
    path->count = count;
    path->nodes = calloc(count, sizeof(uint32_t));
    path->edges = calloc(count ? count : 1u, sizeof(uint32_t));
    path->edge_modes = calloc(count ? count : 1u, sizeof(uint8_t));
    path->geometry_x = calloc(count, sizeof(double));
    path->geometry_y = calloc(count, sizeof(double));
    path->cumulative_length_m = calloc(count, sizeof(float));
    path->cumulative_time_s = calloc(count, sizeof(float));
    if (!path->nodes || !path->edges || !path->edge_modes || !path->geometry_x || !path->geometry_y ||
        !path->cumulative_length_m || !path->cumulative_time_s) {
        route_path_free(path);
        return false;
    }
    return true;
}
// Adds a point and its incoming measured leg to explicit query geometry.
static void query_point(RoutePath *path, uint32_t index, uint32_t node, double x, double y, uint32_t edge,
                        RouteTravelMode mode, float length, float time) {
    path->nodes[index] = node;
    path->geometry_x[index] = x;
    path->geometry_y[index] = y;
    if (index) {
        path->edges[index - 1u] = edge;
        path->edge_modes[index - 1u] = (uint8_t)mode;
    }
    path->total_length_m += length;
    path->total_time_s += time;
    path->cumulative_length_m[index] = path->total_length_m;
    path->cumulative_time_s[index] = path->total_time_s;
}
// Wraps the core graph path in road partials and explicitly approximate home connectors.
static bool query_geometry(const RouteGraph *graph, RoutePath *core, const RouteEndpointAnchor *start,
                           const RouteEndpointAnchor *goal, QueryOption first, QueryOption last,
                           RouteTravelMode mode) {
    RoutePath path;
    if (!query_allocate(&path, core->count + 4u))
        return false;
    query_point(&path, 0, first.node, start->requested_x, start->requested_y, UINT32_MAX, ROUTE_MODE_WALK, 0,
                0);
    query_point(&path, 1, first.node, start->world_x, start->world_y, UINT32_MAX, ROUTE_MODE_WALK,
                (float)start->connector_m, (float)start->connector_m / 1.4f);
    for (uint32_t i = 0u; i < core->count; ++i) {
        uint32_t node = core->nodes[i];
        float length = i ? core->cumulative_length_m[i] - core->cumulative_length_m[i - 1u] : first.length;
        float time = i ? core->cumulative_time_s[i] - core->cumulative_time_s[i - 1u] : first.time;
        query_point(&path, i + 2u, node, graph->node_x[node], graph->node_y[node],
                    i ? core->edges[i - 1u] : first.edge,
                    i ? (RouteTravelMode)core->edge_modes[i - 1u] : mode, length, time);
    }
    query_point(&path, core->count + 2u, last.node, goal->world_x, goal->world_y, last.edge, mode,
                last.length, last.time);
    query_point(&path, core->count + 3u, last.node, goal->requested_x, goal->requested_y, UINT32_MAX,
                ROUTE_MODE_WALK, (float)goal->connector_m, (float)goal->connector_m / 1.4f);
    route_path_free(core);
    *core = path;
    return true;
}
// Creates direct same-segment geometry without detouring through either intersection.
static bool query_direct(RouteState *state, const RouteEndpointAnchor *start, const RouteEndpointAnchor *goal,
                         uint32_t edge, float length) {
    RoutePath path;
    if (!query_allocate(&path, 4u))
        return false;
    query_point(&path, 0, start->node, start->requested_x, start->requested_y, UINT32_MAX, ROUTE_MODE_WALK, 0,
                0);
    query_point(&path, 1, start->node, start->world_x, start->world_y, UINT32_MAX, ROUTE_MODE_WALK,
                (float)start->connector_m, (float)start->connector_m / 1.4f);
    query_point(&path, 2, goal->node, goal->world_x, goal->world_y, edge, state->mode, length,
                length / route_edge_speed_mps(&state->graph, edge, state->mode));
    query_point(&path, 3, goal->node, goal->requested_x, goal->requested_y, UINT32_MAX, ROUTE_MODE_WALK,
                (float)goal->connector_m, (float)goal->connector_m / 1.4f);
    route_path_free(&state->path);
    state->path = path;
    for (uint32_t i = 0u; i < state->alternatives.count; ++i)
        route_path_free(&state->alternatives.paths[i]);
    state->alternatives.count = 0u;
    route_path_free(&state->drive_path);
    route_path_free(&state->walk_path);
    state->has_transfer = false;
    state->start_node = start->node;
    state->goal_node = goal->node;
    state->has_start = state->has_goal = true;
    return true;
}
// Solves every permitted endpoint pair using the same graph cost policy as native routing.
bool route_query_solve(RouteState *state, const RouteEndpointAnchor *start, const RouteEndpointAnchor *goal) {
    if (!state || !state->loaded || !start || !goal || !start->valid || !goal->valid)
        return false;
    RouteEndpointAnchor first_anchor = *start, last_anchor = *goal;
    if (first_anchor.requested_x == 0.0 && first_anchor.requested_y == 0.0) {
        first_anchor.requested_x = first_anchor.world_x;
        first_anchor.requested_y = first_anchor.world_y;
        first_anchor.connector_m = 0.0;
    }
    if (last_anchor.requested_x == 0.0 && last_anchor.requested_y == 0.0) {
        last_anchor.requested_x = last_anchor.world_x;
        last_anchor.requested_y = last_anchor.world_y;
        last_anchor.connector_m = 0.0;
    }
    if (first_anchor.on_edge &&
        !route_query_snap(&state->graph, (MercatorMeters){first_anchor.requested_x, first_anchor.requested_y},
                          state->mode, 200.0, &first_anchor))
        return false;
    if (last_anchor.on_edge &&
        !route_query_snap(&state->graph, (MercatorMeters){last_anchor.requested_x, last_anchor.requested_y},
                          state->mode, 200.0, &last_anchor))
        return false;
    start = &first_anchor;
    goal = &last_anchor;
    if (fabs(start->requested_x - goal->requested_x) < 0.000001 &&
        fabs(start->requested_y - goal->requested_y) < 0.000001) {
        RoutePath path;
        if (!query_allocate(&path, 2u))
            return false;
        route_state_clear(state);
        query_point(&path, 0, start->node, start->requested_x, start->requested_y, UINT32_MAX, state->mode, 0,
                    0);
        query_point(&path, 1, goal->node, goal->requested_x, goal->requested_y, UINT32_MAX, state->mode, 0,
                    0);
        state->path = path;
        state->start_node = start->node;
        state->goal_node = goal->node;
        state->has_start = state->has_goal = true;
        return true;
    }
    QueryOption starts[2], goals[2], best_start = {0}, best_goal = {0};
    uint32_t ns = query_options(&state->graph, start, true, state->mode, state->objective, starts),
             ng = query_options(&state->graph, goal, false, state->mode, state->objective, goals);
    float best = INFINITY;
    bool found = false;
    for (uint32_t i = 0u; i < ns; ++i)
        for (uint32_t j = 0u; j < ng; ++j) {
            if (!route_state_route(state, starts[i].node, goals[j].node))
                continue;
            float score =
                route_path_objective_cost(&state->graph, &state->path, state->objective, state->mode) +
                starts[i].cost + goals[j].cost;
            if (score < best) {
                best = score;
                best_start = starts[i];
                best_goal = goals[j];
                found = true;
            }
        }
    if (start->on_edge && goal->on_edge &&
        ((start->edge_from == goal->edge_from && start->edge_to == goal->edge_to) ||
         (start->edge_from == goal->edge_to && start->edge_to == goal->edge_from))) {
        float goal_distance = start->edge_from == goal->edge_from ? goal->dist_to_from_m : goal->dist_to_to_m;
        float delta = goal_distance - start->dist_to_from_m;
        uint32_t edge;
        uint32_t from = delta >= 0 ? start->edge_from : start->edge_to,
                 to = delta >= 0 ? start->edge_to : start->edge_from;
        if (route_find_best_edge(&state->graph, from, to, state->objective, state->mode, &edge)) {
            float length = fabsf(delta),
                  score = route_edge_objective_cost(&state->graph, edge, state->objective, state->mode) *
                          length / state->graph.edge_length[edge];
            if (score <= best)
                return query_direct(state, start, goal, edge, length);
        }
    }
    if (!found || !route_state_route(state, best_start.node, best_goal.node))
        return false;
    if (!query_geometry(&state->graph, &state->path, start, goal, best_start, best_goal, state->mode))
        return false;
    state->has_transfer = false;
    for (uint32_t i = 0u; i < state->alternatives.count; ++i)
        if (!query_geometry(&state->graph, &state->alternatives.paths[i], start, goal, best_start, best_goal,
                            state->mode))
            return false;
    return true;
}
