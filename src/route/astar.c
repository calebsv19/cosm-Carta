#include "route/astar.h"

#include "map/mft_loader.h"

#include <float.h>
#include <math.h>
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

static bool edge_allowed(RouteTravelMode mode, uint8_t road_class);

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

static bool heap_pop(MinHeap *heap, uint32_t *out_node) {
    if (heap->count == 0) {
        return false;
    }

    if (out_node) {
        *out_node = heap->items[0].node;
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

static float objective_min_time_multiplier(RouteObjective objective) {
    if (objective == ROUTE_OBJECTIVE_MOST_TIME_ABOVE_SPEED_THRESHOLD) {
        return 0.6f;
    }
    return 1.0f;
}

static float heuristic(const RouteGraph *graph,
                       uint32_t a,
                       uint32_t b,
                       RouteObjective objective,
                       float max_speed_mps) {
    double dx = graph->node_x[a] - graph->node_x[b];
    double dy = graph->node_y[a] - graph->node_y[b];
    float distance_m = (float)sqrt(dx * dx + dy * dy);
    if (objective == ROUTE_OBJECTIVE_SHORTEST_DISTANCE) {
        return distance_m;
    }
    if (objective == ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN) {
        /* Keep heuristic admissible for elevation objective by using Dijkstra behavior. */
        return 0.0f;
    }
    if (max_speed_mps <= 0.1f) {
        return 0.0f;
    }
    return (distance_m / max_speed_mps) * objective_min_time_multiplier(objective);
}

static float max_edge_speed_for_mode(const RouteGraph *graph, RouteTravelMode mode) {
    if (!graph || graph->edge_count == 0) {
        return 0.0f;
    }

    float max_speed = 0.0f;
    for (uint32_t e = 0; e < graph->edge_count; ++e) {
        if (!edge_allowed(mode, graph->edge_class[e])) {
            continue;
        }
        float speed = graph->edge_speed[e];
        if (speed > max_speed) {
            max_speed = speed;
        }
    }

    return max_speed;
}

static float edge_cost_for_objective(const RouteGraph *graph, uint32_t edge_index, RouteObjective objective) {
    if (!graph || edge_index >= graph->edge_count) {
        return FLT_MAX;
    }

    float length_m = graph->edge_length[edge_index];
    float speed_mps = graph->edge_speed[edge_index];
    if (speed_mps <= 0.1f) {
        speed_mps = 0.1f;
    }
    float base_time_s = length_m / speed_mps;

    switch (objective) {
        case ROUTE_OBJECTIVE_SHORTEST_DISTANCE:
            return length_m;
        case ROUTE_OBJECTIVE_LOWEST_TIME:
            return base_time_s;
        case ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN: {
            float grade = graph->edge_grade ? graph->edge_grade[edge_index] : 0.0f;
            float uphill_gain_m = 0.0f;
            if (grade > 0.0f) {
                uphill_gain_m = grade * length_m;
            }
            float penalty = graph->edge_penalty ? graph->edge_penalty[edge_index] : 0.0f;
            return length_m + uphill_gain_m * 8.0f + penalty * length_m * 0.5f;
        }
        case ROUTE_OBJECTIVE_MOST_TIME_ABOVE_SPEED_THRESHOLD: {
            const float threshold_mps = 13.4112f; /* 30 mph */
            float speed_limit = graph->edge_speed_limit ? graph->edge_speed_limit[edge_index] : 0.0f;
            bool above_threshold = speed_limit >= threshold_mps;
            float time_weight = above_threshold ? 0.6f : 1.8f;
            return base_time_s * time_weight;
        }
        default:
            return base_time_s;
    }
}

void route_path_free(RoutePath *path) {
    if (!path) {
        return;
    }

    free(path->nodes);
    free(path->cumulative_length_m);
    free(path->cumulative_time_s);
    memset(path, 0, sizeof(*path));
}

static bool edge_allowed(RouteTravelMode mode, uint8_t road_class) {
    if (mode == ROUTE_MODE_WALK) {
        return road_class != ROAD_CLASS_MOTORWAY && road_class != ROAD_CLASS_TRUNK;
    }
    return road_class != ROAD_CLASS_FOOTWAY && road_class != ROAD_CLASS_PATH;
}

bool route_astar(const RouteGraph *graph,
                 uint32_t start,
                 uint32_t goal,
                 RouteObjective objective,
                 RouteTravelMode mode,
                 RoutePath *out_path) {
    if (!graph || !out_path || start >= graph->node_count || goal >= graph->node_count) {
        return false;
    }

    route_path_free(out_path);

    float *g_score = (float *)malloc(sizeof(float) * graph->node_count);
    float *f_score = (float *)malloc(sizeof(float) * graph->node_count);
    int32_t *came_from = (int32_t *)malloc(sizeof(int32_t) * graph->node_count);
    if (!g_score || !f_score || !came_from) {
        free(g_score);
        free(f_score);
        free(came_from);
        return false;
    }

    for (uint32_t i = 0; i < graph->node_count; ++i) {
        g_score[i] = FLT_MAX;
        f_score[i] = FLT_MAX;
        came_from[i] = -1;
    }

    float max_speed_mps = (objective == ROUTE_OBJECTIVE_SHORTEST_DISTANCE ||
                           objective == ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN)
        ? 0.0f
        : max_edge_speed_for_mode(graph, mode);
    g_score[start] = 0.0f;
    f_score[start] = heuristic(graph, start, goal, objective, max_speed_mps);

    MinHeap open;
    heap_init(&open);
    heap_push(&open, start, f_score[start]);

    bool found = false;

    while (open.count > 0) {
        uint32_t current = 0;
        heap_pop(&open, &current);
        if (current == goal) {
            found = true;
            break;
        }

        uint32_t start_edge = graph->edge_start[current];
        uint32_t end_edge = graph->edge_start[current + 1];
        for (uint32_t e = start_edge; e < end_edge; ++e) {
            if (!edge_allowed(mode, graph->edge_class[e])) {
                continue;
            }
            uint32_t neighbor = graph->edge_to[e];
            float edge_cost = edge_cost_for_objective(graph, e, objective);

            float tentative = g_score[current] + edge_cost;
            if (tentative < g_score[neighbor]) {
                came_from[neighbor] = (int32_t)current;
                g_score[neighbor] = tentative;
                float h = heuristic(graph, neighbor, goal, objective, max_speed_mps);
                f_score[neighbor] = tentative + h;
                heap_push(&open, neighbor, f_score[neighbor]);
            }
        }
    }

    if (!found) {
        heap_free(&open);
        free(g_score);
        free(f_score);
        free(came_from);
        return false;
    }

    uint32_t path_count = 0;
    for (int32_t at = (int32_t)goal; at != -1; at = came_from[at]) {
        path_count += 1;
    }

    out_path->nodes = (uint32_t *)malloc(sizeof(uint32_t) * path_count);
    if (!out_path->nodes) {
        heap_free(&open);
        free(g_score);
        free(f_score);
        free(came_from);
        return false;
    }

    out_path->count = path_count;
    uint32_t idx = path_count;
    for (int32_t at = (int32_t)goal; at != -1; at = came_from[at]) {
        out_path->nodes[--idx] = (uint32_t)at;
    }

    out_path->cumulative_length_m = (float *)calloc(out_path->count, sizeof(float));
    out_path->cumulative_time_s = (float *)calloc(out_path->count, sizeof(float));
    if (!out_path->cumulative_length_m || !out_path->cumulative_time_s) {
        route_path_free(out_path);
        heap_free(&open);
        free(g_score);
        free(f_score);
        free(came_from);
        return false;
    }

    out_path->total_length_m = 0.0f;
    out_path->total_time_s = 0.0f;
    for (uint32_t i = 0; i + 1 < out_path->count; ++i) {
        uint32_t node = out_path->nodes[i];
        uint32_t next = out_path->nodes[i + 1];
        uint32_t start_edge = graph->edge_start[node];
        uint32_t end_edge = graph->edge_start[node + 1];
        for (uint32_t e = start_edge; e < end_edge; ++e) {
            if (graph->edge_to[e] == next) {
                out_path->total_length_m += graph->edge_length[e];
                float speed = graph->edge_speed[e];
                if (speed <= 0.1f) {
                    speed = 0.1f;
                }
                out_path->total_time_s += graph->edge_length[e] / speed;
                out_path->cumulative_length_m[i + 1] = out_path->total_length_m;
                out_path->cumulative_time_s[i + 1] = out_path->total_time_s;
                break;
            }
        }
    }

    heap_free(&open);
    free(g_score);
    free(f_score);
    free(came_from);
    return true;
}
