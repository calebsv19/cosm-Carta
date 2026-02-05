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

static float heuristic(const RouteGraph *graph, uint32_t a, uint32_t b) {
    double dx = graph->node_x[a] - graph->node_x[b];
    double dy = graph->node_y[a] - graph->node_y[b];
    return (float)sqrt(dx * dx + dy * dy);
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

bool route_astar(const RouteGraph *graph, uint32_t start, uint32_t goal, bool fastest, RouteTravelMode mode, RoutePath *out_path) {
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

    g_score[start] = 0.0f;
    f_score[start] = heuristic(graph, start, goal);

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
            float edge_cost = graph->edge_length[e];
            if (fastest) {
                float speed = graph->edge_speed[e];
                if (speed <= 0.1f) {
                    speed = 0.1f;
                }
                edge_cost = edge_cost / speed;
            }

            float tentative = g_score[current] + edge_cost;
            if (tentative < g_score[neighbor]) {
                came_from[neighbor] = (int32_t)current;
                g_score[neighbor] = tentative;
                float h = heuristic(graph, neighbor, goal);
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
