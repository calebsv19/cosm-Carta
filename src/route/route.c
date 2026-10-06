#include "route/route.h"
#include "route/cost.h"

#include "core/log.h"
#include "map/mft_loader.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

typedef struct HeapItem {
    uint32_t node;
    float priority;
} HeapItem;

typedef struct MinHeap {
    HeapItem *items;
    uint32_t count;
    uint32_t capacity;
} MinHeap;

static void heap_init(MinHeap *heap) {
    heap->items = NULL;
    heap->count = 0;
    heap->capacity = 0;
}

static void heap_free(MinHeap *heap) {
    free(heap->items);
    heap->items = NULL;
    heap->count = 0;
    heap->capacity = 0;
}

static bool heap_push(MinHeap *heap, uint32_t node, float priority) {
    if (heap->count == heap->capacity) {
        uint32_t next = heap->capacity == 0 ? 256 : heap->capacity * 2;
        HeapItem *items = (HeapItem *)realloc(heap->items, next * sizeof(HeapItem));
        if (!items) {
            return false;
        }
        heap->items = items;
        heap->capacity = next;
    }

    uint32_t idx = heap->count++;
    heap->items[idx].node = node;
    heap->items[idx].priority = priority;

    while (idx > 0) {
        uint32_t parent = (idx - 1) / 2;
        if (heap->items[parent].priority <= heap->items[idx].priority) {
            break;
        }
        HeapItem temp = heap->items[parent];
        heap->items[parent] = heap->items[idx];
        heap->items[idx] = temp;
        idx = parent;
    }

    return true;
}

static bool heap_pop(MinHeap *heap, uint32_t *out_node, float *out_priority) {
    if (heap->count == 0) {
        return false;
    }

    if (out_node) {
        *out_node = heap->items[0].node;
    }
    if (out_priority) {
        *out_priority = heap->items[0].priority;
    }

    heap->count -= 1;
    heap->items[0] = heap->items[heap->count];

    uint32_t idx = 0;
    while (true) {
        uint32_t left = idx * 2 + 1;
        uint32_t right = idx * 2 + 2;
        uint32_t smallest = idx;

        if (left < heap->count && heap->items[left].priority < heap->items[smallest].priority) {
            smallest = left;
        }
        if (right < heap->count && heap->items[right].priority < heap->items[smallest].priority) {
            smallest = right;
        }

        if (smallest == idx) {
            break;
        }

        HeapItem temp = heap->items[idx];
        heap->items[idx] = heap->items[smallest];
        heap->items[smallest] = temp;
        idx = smallest;
    }

    return true;
}

static bool node_has_drive_edge(const RouteGraph *graph, uint32_t node) {
    if (!graph || node >= graph->node_count) {
        return false;
    }

    uint32_t start_edge = graph->edge_start[node];
    uint32_t end_edge = graph->edge_start[node + 1];
    for (uint32_t e = start_edge; e < end_edge; ++e) {
        if (route_edge_allowed(graph, e, ROUTE_MODE_CAR)) {
            return true;
        }
    }
    for (uint32_t e = 0u; e < graph->edge_count; ++e) {
        if (graph->edge_to[e] == node && route_edge_allowed(graph, e, ROUTE_MODE_CAR)) return true;
    }
    return false;
}

static const RouteObjective kAlternativeObjectiveOrder[] = {
    ROUTE_OBJECTIVE_SHORTEST_DISTANCE,
    ROUTE_OBJECTIVE_LOWEST_TIME,
    ROUTE_OBJECTIVE_MOST_TIME_ABOVE_SPEED_THRESHOLD,
    ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN
};

static bool route_path_copy(const RouteGraph *graph, const RoutePath *src, RoutePath *dst);

static void route_alternatives_clear(RouteAlternativeSet *set) {
    if (!set) {
        return;
    }
    for (uint32_t i = 0; i < ROUTE_ALTERNATIVE_MAX; ++i) {
        route_path_free(&set->paths[i]);
    }
    memset(set, 0, sizeof(*set));
}

// Recomputes totals while preserving modes for combined walking/driving legs.
static bool route_path_compute_totals(const RouteGraph *graph, RoutePath *path) {
    return route_path_compute_metrics(graph, path, ROUTE_MODE_CAR);
}

// Applies the same walking model used during optimization.
static bool route_path_apply_walk_speed(const RouteGraph *graph, RoutePath *path, float walk_speed_mps) {
    (void)walk_speed_mps;
    return route_path_compute_metrics(graph, path, ROUTE_MODE_WALK);
}

// Searches outbound from a start or incoming toward a goal for a legal walking transfer.
static bool route_find_walk_to_drive(const RouteGraph *graph, uint32_t endpoint,
                                     RouteObjective objective, bool incoming,
                                     RoutePath *out_path, uint32_t *out_drive_node) {
    if (!graph || !out_path || endpoint >= graph->node_count) return false;
    float *dist = malloc(sizeof(float) * graph->node_count);
    int32_t *parent = malloc(sizeof(int32_t) * graph->node_count);
    uint32_t *parent_edge = calloc(graph->node_count, sizeof(uint32_t));
    bool *visited = calloc(graph->node_count, sizeof(bool));
    uint32_t *head = incoming ? malloc(sizeof(uint32_t) * graph->node_count) : NULL;
    uint32_t *next = incoming ? malloc(sizeof(uint32_t) * graph->edge_count) : NULL;
    uint32_t *source = incoming ? malloc(sizeof(uint32_t) * graph->edge_count) : NULL;
    MinHeap open;
    heap_init(&open);
    bool success = false;
    if (!dist || !parent || !parent_edge || !visited || (incoming && (!head || !next || !source))) goto done;
    for (uint32_t n = 0u; n < graph->node_count; ++n) {
        dist[n] = FLT_MAX;
        parent[n] = -1;
        if (incoming) head[n] = UINT32_MAX;
    }
    if (incoming) {
        for (uint32_t n = 0u; n < graph->node_count; ++n) {
            for (uint32_t e = graph->edge_start[n]; e < graph->edge_start[n + 1u]; ++e) {
                source[e] = n;
                next[e] = head[graph->edge_to[e]];
                head[graph->edge_to[e]] = e;
            }
        }
    }
    dist[endpoint] = 0.0f;
    if (!heap_push(&open, endpoint, 0.0f)) goto done;
    uint32_t drive_node = endpoint;
    bool found = false;
    while (open.count > 0u) {
        uint32_t current = 0u;
        heap_pop(&open, &current, NULL);
        if (visited[current]) continue;
        visited[current] = true;
        if (node_has_drive_edge(graph, current)) {
            drive_node = current;
            found = true;
            break;
        }
        uint32_t e = incoming ? head[current] : graph->edge_start[current];
        while (incoming ? e != UINT32_MAX : e < graph->edge_start[current + 1u]) {
            uint32_t neighbor = incoming ? source[e] : graph->edge_to[e];
            if (route_edge_allowed(graph, e, ROUTE_MODE_WALK)) {
                float candidate = dist[current] + route_edge_objective_cost(graph, e, objective, ROUTE_MODE_WALK);
                if (candidate < dist[neighbor]) {
                    dist[neighbor] = candidate;
                    parent[neighbor] = (int32_t)current;
                    parent_edge[neighbor] = e;
                    if (!heap_push(&open, neighbor, candidate)) goto done;
                }
            }
            e = incoming ? next[e] : e + 1u;
        }
    }
    if (!found) goto done;
    uint32_t count = 0u;
    for (int32_t at = (int32_t)drive_node; at != -1; at = parent[at]) ++count;
    out_path->nodes = malloc(sizeof(uint32_t) * count);
    out_path->edges = calloc(count, sizeof(uint32_t));
    out_path->edge_modes = calloc(count, sizeof(uint8_t));
    if (!out_path->nodes || !out_path->edges || !out_path->edge_modes) goto done;
    out_path->count = count;
    uint32_t index = incoming ? 0u : count;
    for (int32_t at = (int32_t)drive_node; at != -1; at = parent[at]) {
        uint32_t slot = incoming ? index++ : --index;
        out_path->nodes[slot] = (uint32_t)at;
        if (parent[at] != -1) {
            uint32_t step = incoming ? slot : slot - 1u;
            out_path->edges[step] = parent_edge[at];
            out_path->edge_modes[step] = ROUTE_MODE_WALK;
        }
    }
    success = route_path_compute_metrics(graph, out_path, ROUTE_MODE_WALK);
    if (success && out_drive_node) *out_drive_node = drive_node;
done:
    if (!success) route_path_free(out_path);
    heap_free(&open);
    free(dist); free(parent); free(parent_edge); free(visited);
    free(head); free(next); free(source);
    return success;
}

static void route_state_build_alternatives(RouteState *state, uint32_t start_node, uint32_t goal_node) {
    if (!state || !state->loaded || state->path.count < 2) {
        return;
    }

    route_alternatives_clear(&state->alternatives);

    for (size_t i = 0; i < sizeof(kAlternativeObjectiveOrder) / sizeof(kAlternativeObjectiveOrder[0]); ++i) {
        if (state->alternatives.count >= ROUTE_ALTERNATIVE_MAX) {
            break;
        }
        RouteObjective candidate_objective = kAlternativeObjectiveOrder[i];
        RoutePath candidate = {0};

        if (candidate_objective == state->objective) {
            if (!route_path_copy(&state->graph, &state->path, &candidate)) {
                continue;
            }
        } else {
            if (!route_astar(&state->graph, start_node, goal_node, candidate_objective, state->mode, &candidate)) {
                continue;
            }
            if (state->mode == ROUTE_MODE_WALK) {
                route_path_apply_walk_speed(&state->graph, &candidate, 1.4f);
            }
        }
        uint32_t slot = state->alternatives.count;
        state->alternatives.paths[slot] = candidate;
        state->alternatives.objectives[slot] = candidate_objective;
        state->alternatives.count += 1u;
    }
}

// Copies exact selected edges, modes and displayed/cumulative totals.
static bool route_path_copy(const RouteGraph *graph, const RoutePath *src, RoutePath *dst) {
    if (!graph || !src || !dst || src->count == 0u) return false;
    dst->count = src->count;
    dst->nodes = malloc(sizeof(uint32_t) * src->count);
    dst->edges = calloc(src->count, sizeof(uint32_t));
    dst->edge_modes = calloc(src->count, sizeof(uint8_t));
    if (!dst->nodes || !dst->edges || !dst->edge_modes) { route_path_free(dst); return false; }
    memcpy(dst->nodes, src->nodes, sizeof(uint32_t) * src->count);
    for (uint32_t i = 0u; i + 1u < src->count; ++i) {
        if (!route_path_edge_index(graph, src, i, &dst->edges[i])) { route_path_free(dst); return false; }
        dst->edge_modes[i] = src->edge_modes ? src->edge_modes[i] : ROUTE_MODE_CAR;
    }
    if (!route_path_compute_totals(graph, dst)) { route_path_free(dst); return false; }
    dst->total_length_m = src->total_length_m;
    dst->total_time_s = src->total_time_s;
    if (src->cumulative_length_m) memcpy(dst->cumulative_length_m, src->cumulative_length_m, sizeof(float) * src->count);
    if (src->cumulative_time_s) memcpy(dst->cumulative_time_s, src->cumulative_time_s, sizeof(float) * src->count);
    return true;
}

static bool route_path_build_combined(const RouteGraph *graph,
                                      const RoutePath *start_walk_path,
                                      const RoutePath *drive_path,
                                      const RoutePath *goal_walk_path,
                                      RoutePath *out_path) {
    if (!graph || !out_path) {
        return false;
    }

    const RoutePath *segments[3] = {start_walk_path, drive_path, goal_walk_path};
    uint32_t combined_count = 0;
    uint32_t last_node = 0;
    bool have_last = false;

    for (size_t s = 0; s < 3; ++s) {
        const RoutePath *segment = segments[s];
        if (!segment || segment->count == 0 || !segment->nodes) {
            continue;
        }
        uint32_t copy_start = 0;
        if (have_last) {
            if (segment->nodes[0] != last_node) return false;
            copy_start = 1;
        }
        if (segment->count <= copy_start) {
            continue;
        }
        combined_count += segment->count - copy_start;
        last_node = segment->nodes[segment->count - 1u];
        have_last = true;
    }

    if (combined_count < 2) {
        return false;
    }

    out_path->nodes = (uint32_t *)malloc(sizeof(uint32_t) * combined_count);
    if (!out_path->nodes) {
        return false;
    }
    out_path->count = combined_count;
    out_path->edges = calloc(combined_count, sizeof(uint32_t));
    out_path->edge_modes = calloc(combined_count, sizeof(uint8_t));
    if (!out_path->edges || !out_path->edge_modes) { route_path_free(out_path); return false; }

    uint32_t dst = 0;
    have_last = false;
    last_node = 0;
    for (size_t s = 0; s < 3; ++s) {
        const RoutePath *segment = segments[s];
        if (!segment || segment->count == 0 || !segment->nodes) {
            continue;
        }
        uint32_t copy_start = 0;
        if (have_last && segment->nodes[0] == last_node) {
            copy_start = 1;
        }
        for (uint32_t i = copy_start; i < segment->count; ++i) {
            out_path->nodes[dst++] = segment->nodes[i];
            if (i + 1u < segment->count) {
                if (!route_path_edge_index(graph, segment, i, &out_path->edges[dst - 1u])) {
                    route_path_free(out_path); return false;
                }
                out_path->edge_modes[dst - 1u] = s == 1u ? ROUTE_MODE_CAR : ROUTE_MODE_WALK;
            }
            if (copy_start == 1u && i == 1u) {
                if (!route_path_edge_index(graph, segment, 0u, &out_path->edges[dst - 2u])) {
                    route_path_free(out_path); return false;
                }
                out_path->edge_modes[dst - 2u] = s == 1u ? ROUTE_MODE_CAR : ROUTE_MODE_WALK;
            }
        }
        last_node = segment->nodes[segment->count - 1u];
        have_last = true;
    }

    if (dst != combined_count || !route_path_compute_totals(graph, out_path)) {
        route_path_free(out_path);
        return false;
    }

    return true;
}

void route_state_init(RouteState *state) {
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->mode = ROUTE_MODE_CAR;
    state->objective = ROUTE_OBJECTIVE_SHORTEST_DISTANCE;
}

bool route_state_load_graph(RouteState *state, const char *path) {
    if (!state || !path) {
        return false;
    }

    route_graph_free(&state->graph);
    route_path_free(&state->path);
    route_path_free(&state->drive_path);
    route_path_free(&state->walk_path);
    route_alternatives_clear(&state->alternatives);
    state->loaded = route_graph_load(path, &state->graph);
    state->has_start = false;
    state->has_goal = false;
    state->has_transfer = false;
    return state->loaded;
}

void route_state_clear(RouteState *state) {
    if (!state) {
        return;
    }

    route_path_free(&state->path);
    route_path_free(&state->drive_path);
    route_path_free(&state->walk_path);
    route_alternatives_clear(&state->alternatives);
    state->has_start = false;
    state->has_goal = false;
    state->has_transfer = false;
    state->transfer_node = 0;
}

bool route_state_route(RouteState *state, uint32_t start_node, uint32_t goal_node) {
    if (!state || !state->loaded) {
        return false;
    }

    route_path_free(&state->path);
    route_path_free(&state->drive_path);
    route_path_free(&state->walk_path);
    route_alternatives_clear(&state->alternatives);
    state->has_transfer = false;

    state->start_node = start_node;
    state->goal_node = goal_node;
    state->has_start = true;
    state->has_goal = true;

    const float walk_speed_mps = ROUTE_WALK_SPEED_MPS;
    if (start_node == goal_node) {
        return route_astar(&state->graph, start_node, goal_node, state->objective, state->mode, &state->path);
    }

    if (state->mode == ROUTE_MODE_CAR) {
        RoutePath start_walk = {0};
        RoutePath goal_walk = {0};
        RoutePath drive_leg = {0};

        uint32_t drive_start = start_node;
        uint32_t drive_goal = goal_node;
        bool has_start_walk = false;
        bool has_goal_walk = false;
        bool has_drive_leg = false;

        if (!node_has_drive_edge(&state->graph, start_node)) {
            if (!route_find_walk_to_drive(&state->graph, start_node, state->objective, false, &start_walk, &drive_start)) {
                return false;
            }
            has_start_walk = start_walk.count >= 2u;
        }

        if (!node_has_drive_edge(&state->graph, goal_node)) {
            if (!route_find_walk_to_drive(&state->graph, goal_node, state->objective, true, &goal_walk, &drive_goal)) {
                route_path_free(&start_walk);
                return false;
            }
            if (goal_walk.count >= 2) {
                route_path_apply_walk_speed(&state->graph, &goal_walk, walk_speed_mps);
                has_goal_walk = true;
            } else {
                route_path_free(&goal_walk);
            }
        }

        if (drive_start != drive_goal || (!has_start_walk && !has_goal_walk)) {
            if (!route_astar(&state->graph, drive_start, drive_goal, state->objective, ROUTE_MODE_CAR, &drive_leg)) {
                route_path_free(&start_walk);
                route_path_free(&goal_walk);
                return false;
            }
            if (drive_leg.count >= 2) {
                has_drive_leg = true;
            } else {
                route_path_free(&drive_leg);
            }
        }

        if (!route_path_build_combined(&state->graph,
                                       has_start_walk ? &start_walk : NULL,
                                       has_drive_leg ? &drive_leg : NULL,
                                       has_goal_walk ? &goal_walk : NULL,
                                       &state->path)) {
            route_path_free(&start_walk);
            route_path_free(&goal_walk);
            route_path_free(&drive_leg);
            return false;
        }

        if (!has_start_walk && has_goal_walk && has_drive_leg) {
            state->drive_path = drive_leg;
            memset(&drive_leg, 0, sizeof(drive_leg));
            state->walk_path = goal_walk;
            memset(&goal_walk, 0, sizeof(goal_walk));
            state->has_transfer = true;
            state->transfer_node = drive_goal;
        } else if (has_start_walk && !has_goal_walk && has_drive_leg) {
            state->drive_path = drive_leg;
            memset(&drive_leg, 0, sizeof(drive_leg));
            state->walk_path = start_walk;
            memset(&start_walk, 0, sizeof(start_walk));
            state->has_transfer = true;
            state->transfer_node = drive_start;
        } else if (!has_start_walk && !has_goal_walk && has_drive_leg) {
            state->drive_path = drive_leg;
            memset(&drive_leg, 0, sizeof(drive_leg));
        }

        route_path_free(&start_walk);
        route_path_free(&goal_walk);
        route_path_free(&drive_leg);
        route_state_build_alternatives(state, start_node, goal_node);
        return true;
    }

    if (!route_astar(&state->graph, start_node, goal_node, state->objective, ROUTE_MODE_WALK, &state->path)) {
        return false;
    }
    route_path_apply_walk_speed(&state->graph, &state->path, walk_speed_mps);
    route_state_build_alternatives(state, start_node, goal_node);
    return true;
}

void route_state_shutdown(RouteState *state) {
    if (!state) {
        return;
    }

    route_graph_free(&state->graph);
    route_path_free(&state->path);
    route_path_free(&state->drive_path);
    route_path_free(&state->walk_path);
    route_alternatives_clear(&state->alternatives);
    memset(state, 0, sizeof(*state));
}

const char *route_objective_label(RouteObjective objective) {
    switch (objective) {
        case ROUTE_OBJECTIVE_SHORTEST_DISTANCE:
            return "shortest";
        case ROUTE_OBJECTIVE_LOWEST_TIME:
            return "lowest_time";
        case ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN:
            return "low_climb_proxy";
        case ROUTE_OBJECTIVE_MOST_TIME_ABOVE_SPEED_THRESHOLD:
            return "prefer_fast_roads";
        default:
            return "unknown";
    }
}
