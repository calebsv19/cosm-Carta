#include "route/route.h"

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

static bool edge_allowed_drive(uint8_t road_class) {
    return road_class != ROAD_CLASS_FOOTWAY && road_class != ROAD_CLASS_PATH;
}

static bool edge_allowed_walk(uint8_t road_class) {
    return road_class != ROAD_CLASS_MOTORWAY && road_class != ROAD_CLASS_TRUNK;
}

static bool node_has_drive_edge(const RouteGraph *graph, uint32_t node) {
    if (!graph || node >= graph->node_count) {
        return false;
    }

    uint32_t start_edge = graph->edge_start[node];
    uint32_t end_edge = graph->edge_start[node + 1];
    for (uint32_t e = start_edge; e < end_edge; ++e) {
        if (edge_allowed_drive(graph->edge_class[e])) {
            return true;
        }
    }
    return false;
}

static bool route_path_compute_totals(const RouteGraph *graph, RoutePath *path) {
    if (!graph || !path || path->count < 2) {
        return false;
    }

    path->cumulative_length_m = (float *)calloc(path->count, sizeof(float));
    path->cumulative_time_s = (float *)calloc(path->count, sizeof(float));
    if (!path->cumulative_length_m || !path->cumulative_time_s) {
        return false;
    }

    path->total_length_m = 0.0f;
    path->total_time_s = 0.0f;
    for (uint32_t i = 0; i + 1 < path->count; ++i) {
        uint32_t node = path->nodes[i];
        uint32_t next = path->nodes[i + 1];
        uint32_t start_edge = graph->edge_start[node];
        uint32_t end_edge = graph->edge_start[node + 1];
        bool found = false;
        for (uint32_t e = start_edge; e < end_edge; ++e) {
            if (graph->edge_to[e] == next) {
                path->total_length_m += graph->edge_length[e];
                float speed = graph->edge_speed[e];
                if (speed <= 0.1f) {
                    speed = 0.1f;
                }
                path->total_time_s += graph->edge_length[e] / speed;
                path->cumulative_length_m[i + 1] = path->total_length_m;
                path->cumulative_time_s[i + 1] = path->total_time_s;
                found = true;
                break;
            }
        }
        if (!found) {
            free(path->cumulative_length_m);
            free(path->cumulative_time_s);
            path->cumulative_length_m = NULL;
            path->cumulative_time_s = NULL;
            path->total_length_m = 0.0f;
            path->total_time_s = 0.0f;
            return false;
        }
    }

    return true;
}

static bool route_path_apply_walk_speed(const RouteGraph *graph, RoutePath *path, float walk_speed_mps) {
    if (!graph || !path || path->count < 2 || walk_speed_mps <= 0.01f) {
        return false;
    }

    if (!path->cumulative_time_s) {
        path->cumulative_time_s = (float *)calloc(path->count, sizeof(float));
        if (!path->cumulative_time_s) {
            return false;
        }
    } else {
        memset(path->cumulative_time_s, 0, sizeof(float) * path->count);
    }

    path->total_time_s = 0.0f;
    for (uint32_t i = 0; i + 1 < path->count; ++i) {
        uint32_t node = path->nodes[i];
        uint32_t next = path->nodes[i + 1];
        uint32_t start_edge = graph->edge_start[node];
        uint32_t end_edge = graph->edge_start[node + 1];
        bool found = false;
        for (uint32_t e = start_edge; e < end_edge; ++e) {
            if (graph->edge_to[e] == next) {
                float length = graph->edge_length[e];
                path->total_time_s += length / walk_speed_mps;
                path->cumulative_time_s[i + 1] = path->total_time_s;
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }

    return true;
}

static bool route_find_walk_to_drive(const RouteGraph *graph, uint32_t goal, bool fastest, RoutePath *out_walk_path, uint32_t *out_drive_node) {
    if (!graph || !out_walk_path || goal >= graph->node_count) {
        return false;
    }

    float *dist = (float *)malloc(sizeof(float) * graph->node_count);
    int32_t *came_from = (int32_t *)malloc(sizeof(int32_t) * graph->node_count);
    bool *visited = (bool *)calloc(graph->node_count, sizeof(bool));
    if (!dist || !came_from || !visited) {
        free(dist);
        free(came_from);
        free(visited);
        return false;
    }

    for (uint32_t i = 0; i < graph->node_count; ++i) {
        dist[i] = FLT_MAX;
        came_from[i] = -1;
    }

    MinHeap open;
    heap_init(&open);
    dist[goal] = 0.0f;
    heap_push(&open, goal, 0.0f);

    bool found = false;
    uint32_t drive_node = goal;
    while (open.count > 0) {
        uint32_t current = 0;
        float current_priority = 0.0f;
        heap_pop(&open, &current, &current_priority);
        if (visited[current]) {
            continue;
        }
        visited[current] = true;

        if (node_has_drive_edge(graph, current)) {
            drive_node = current;
            found = true;
            break;
        }

        uint32_t start_edge = graph->edge_start[current];
        uint32_t end_edge = graph->edge_start[current + 1];
        for (uint32_t e = start_edge; e < end_edge; ++e) {
            if (!edge_allowed_walk(graph->edge_class[e])) {
                continue;
            }
            uint32_t neighbor = graph->edge_to[e];
            float edge_cost = graph->edge_length[e];
            if (fastest) {
                float speed = graph->edge_speed[e];
                if (speed <= 0.1f) {
                    speed = 0.1f;
                }
                edge_cost = edge_cost / speed;
            }
            float next = dist[current] + edge_cost;
            if (next < dist[neighbor]) {
                dist[neighbor] = next;
                came_from[neighbor] = (int32_t)current;
                heap_push(&open, neighbor, next);
            }
        }
    }

    if (!found) {
        heap_free(&open);
        free(dist);
        free(came_from);
        free(visited);
        return false;
    }

    uint32_t count = 0;
    for (int32_t at = (int32_t)drive_node; at != -1; at = came_from[at]) {
        count += 1;
    }

    out_walk_path->nodes = (uint32_t *)malloc(sizeof(uint32_t) * count);
    if (!out_walk_path->nodes) {
        heap_free(&open);
        free(dist);
        free(came_from);
        free(visited);
        return false;
    }

    out_walk_path->count = count;
    uint32_t idx = count;
    for (int32_t at = (int32_t)drive_node; at != -1; at = came_from[at]) {
        out_walk_path->nodes[--idx] = (uint32_t)at;
    }

    if (!route_path_compute_totals(graph, out_walk_path)) {
        route_path_free(out_walk_path);
        heap_free(&open);
        free(dist);
        free(came_from);
        free(visited);
        return false;
    }

    if (out_drive_node) {
        *out_drive_node = drive_node;
    }

    heap_free(&open);
    free(dist);
    free(came_from);
    free(visited);
    return true;
}

static bool route_path_copy(const RouteGraph *graph, const RoutePath *src, RoutePath *dst) {
    if (!graph || !src || !dst || src->count < 2) {
        return false;
    }

    dst->nodes = (uint32_t *)malloc(sizeof(uint32_t) * src->count);
    if (!dst->nodes) {
        return false;
    }

    memcpy(dst->nodes, src->nodes, sizeof(uint32_t) * src->count);
    dst->count = src->count;
    if (!route_path_compute_totals(graph, dst)) {
        route_path_free(dst);
        return false;
    }

    return true;
}

static bool route_path_build_combined(const RouteGraph *graph, const RoutePath *drive_path, const RoutePath *walk_path, RoutePath *out_path) {
    if (!graph || !out_path) {
        return false;
    }

    if (drive_path && drive_path->count >= 2 && (!walk_path || walk_path->count < 2)) {
        return route_path_copy(graph, drive_path, out_path);
    }
    if (walk_path && walk_path->count >= 2 && (!drive_path || drive_path->count < 2)) {
        return route_path_copy(graph, walk_path, out_path);
    }
    if (!drive_path || !walk_path || drive_path->count < 2 || walk_path->count < 2) {
        return false;
    }

    uint32_t combined_count = drive_path->count + walk_path->count - 1;
    out_path->nodes = (uint32_t *)malloc(sizeof(uint32_t) * combined_count);
    if (!out_path->nodes) {
        return false;
    }

    memcpy(out_path->nodes, drive_path->nodes, sizeof(uint32_t) * drive_path->count);
    memcpy(out_path->nodes + drive_path->count, walk_path->nodes + 1, sizeof(uint32_t) * (walk_path->count - 1));
    out_path->count = combined_count;

    if (!route_path_compute_totals(graph, out_path)) {
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
}

bool route_state_load_graph(RouteState *state, const char *path) {
    if (!state || !path) {
        return false;
    }

    route_graph_free(&state->graph);
    route_path_free(&state->path);
    route_path_free(&state->drive_path);
    route_path_free(&state->walk_path);
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
    state->has_transfer = false;

    state->start_node = start_node;
    state->goal_node = goal_node;
    state->has_start = true;
    state->has_goal = true;

    const float walk_speed_mps = 1.4f;

    if (state->mode == ROUTE_MODE_CAR && !node_has_drive_edge(&state->graph, goal_node)) {
        uint32_t transfer_node = goal_node;
        if (!route_find_walk_to_drive(&state->graph, goal_node, state->fastest, &state->walk_path, &transfer_node)) {
            return false;
        }
        route_path_apply_walk_speed(&state->graph, &state->walk_path, walk_speed_mps);
        if (!route_astar(&state->graph, start_node, transfer_node, state->fastest, ROUTE_MODE_CAR, &state->drive_path)) {
            route_path_free(&state->walk_path);
            return false;
        }
        if (!route_path_build_combined(&state->graph, &state->drive_path, &state->walk_path, &state->path)) {
            route_path_free(&state->drive_path);
            route_path_free(&state->walk_path);
            return false;
        }
        state->has_transfer = true;
        state->transfer_node = transfer_node;
        return true;
    }

    if (!route_astar(&state->graph, start_node, goal_node, state->fastest, state->mode, &state->path)) {
        return false;
    }
    if (state->mode == ROUTE_MODE_WALK) {
        route_path_apply_walk_speed(&state->graph, &state->path, walk_speed_mps);
    }
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
    memset(state, 0, sizeof(*state));
}
