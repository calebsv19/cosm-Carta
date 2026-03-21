#include "app/app_internal.h"

#include "core/log.h"
#include "core/time.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const float kRouteSnapCellSizeM = 96.0f;
static const float kRouteHoverSnapRadiusPx = 16.0f;
static const float kRoutePlacementSnapRadiusPx = 24.0f;

static void app_route_path_move(RoutePath *dst, RoutePath *src) {
    if (!dst || !src) {
        return;
    }
    route_path_free(dst);
    *dst = *src;
    memset(src, 0, sizeof(*src));
}

static void app_route_alternatives_move(RouteAlternativeSet *dst, RouteAlternativeSet *src) {
    if (!dst || !src) {
        return;
    }
    for (uint32_t i = 0; i < ROUTE_ALTERNATIVE_MAX; ++i) {
        route_path_free(&dst->paths[i]);
    }
    *dst = *src;
    memset(src, 0, sizeof(*src));
}

static void app_route_result_clear(RouteComputeResult *result) {
    if (!result) {
        return;
    }
    route_path_free(&result->path);
    route_path_free(&result->drive_path);
    route_path_free(&result->walk_path);
    for (uint32_t i = 0; i < ROUTE_ALTERNATIVE_MAX; ++i) {
        route_path_free(&result->alternatives.paths[i]);
    }
    memset(result, 0, sizeof(*result));
}

static int app_route_objective_index(RouteObjective objective) {
    int idx = (int)objective;
    if (idx < 0 || idx >= (int)ROUTE_OBJECTIVE_COUNT) {
        return -1;
    }
    return idx;
}

static void app_route_graph_path_for_region(const AppState *app, char *out_path, size_t out_size) {
    if (!app || !out_path || out_size == 0u) {
        return;
    }
    if (!region_graph_path(&app->region, out_path, out_size)) {
        out_path[0] = '\0';
    }
}

static void app_route_snap_index_free(RouteSnapIndex *index) {
    if (!index) {
        return;
    }
    free(index->segments);
    free(index->entries);
    free(index->cells);
    free(index->segment_seen);
    memset(index, 0, sizeof(*index));
}

static uint64_t app_route_snap_cell_key(int32_t cx, int32_t cy) {
    return ((uint64_t)(uint32_t)cx << 32u) | (uint64_t)(uint32_t)cy;
}

static int app_route_snap_cell_entry_compare(const void *a, const void *b) {
    const RouteSnapCellEntry *ea = (const RouteSnapCellEntry *)a;
    const RouteSnapCellEntry *eb = (const RouteSnapCellEntry *)b;
    if (ea->key < eb->key) {
        return -1;
    }
    if (ea->key > eb->key) {
        return 1;
    }
    if (ea->segment_index < eb->segment_index) {
        return -1;
    }
    if (ea->segment_index > eb->segment_index) {
        return 1;
    }
    return 0;
}

static bool app_route_snap_entry_push(RouteSnapIndex *index, uint64_t key, uint32_t segment_index,
                                      uint32_t *entry_capacity) {
    if (!index || !entry_capacity) {
        return false;
    }
    if (index->entry_count == *entry_capacity) {
        uint32_t next = *entry_capacity == 0u ? 4096u : (*entry_capacity * 2u);
        RouteSnapCellEntry *grown = (RouteSnapCellEntry *)realloc(index->entries, sizeof(RouteSnapCellEntry) * next);
        if (!grown) {
            return false;
        }
        index->entries = grown;
        *entry_capacity = next;
    }
    RouteSnapCellEntry *entry = &index->entries[index->entry_count++];
    entry->key = key;
    entry->segment_index = segment_index;
    return true;
}

static bool app_route_snap_index_build(const RouteGraph *graph, RouteSnapIndex *index) {
    if (!graph || !index || graph->node_count == 0u || graph->edge_count == 0u) {
        return false;
    }

    app_route_snap_index_free(index);

    float min_x = (float)graph->node_x[0];
    float min_y = (float)graph->node_y[0];
    float max_x = min_x;
    float max_y = min_y;
    for (uint32_t i = 1u; i < graph->node_count; ++i) {
        float x = (float)graph->node_x[i];
        float y = (float)graph->node_y[i];
        if (x < min_x) {
            min_x = x;
        }
        if (x > max_x) {
            max_x = x;
        }
        if (y < min_y) {
            min_y = y;
        }
        if (y > max_y) {
            max_y = y;
        }
    }

    uint32_t segment_capacity = 0u;
    for (uint32_t from = 0u; from < graph->node_count; ++from) {
        uint32_t edge_start = graph->edge_start[from];
        uint32_t edge_end = graph->edge_start[from + 1u];
        for (uint32_t e = edge_start; e < edge_end; ++e) {
            uint32_t to = graph->edge_to[e];
            if (to < graph->node_count && to != from) {
                segment_capacity += 1u;
            }
        }
    }
    if (segment_capacity == 0u) {
        return false;
    }

    index->segments = (RouteSnapSegment *)malloc(sizeof(RouteSnapSegment) * segment_capacity);
    if (!index->segments) {
        app_route_snap_index_free(index);
        return false;
    }
    index->segment_count = 0u;
    index->cell_size_m = kRouteSnapCellSizeM;
    index->min_x = min_x;
    index->min_y = min_y;
    index->max_x = max_x;
    index->max_y = max_y;

    uint32_t entry_capacity = 0u;
    for (uint32_t from = 0u; from < graph->node_count; ++from) {
        uint32_t edge_start = graph->edge_start[from];
        uint32_t edge_end = graph->edge_start[from + 1u];
        for (uint32_t e = edge_start; e < edge_end; ++e) {
            uint32_t to = graph->edge_to[e];
            if (to >= graph->node_count || to == from) {
                continue;
            }

            uint32_t segment_index = index->segment_count++;
            index->segments[segment_index].from = from;
            index->segments[segment_index].to = to;

            float ax = (float)graph->node_x[from];
            float ay = (float)graph->node_y[from];
            float bx = (float)graph->node_x[to];
            float by = (float)graph->node_y[to];
            float sx0 = ax < bx ? ax : bx;
            float sy0 = ay < by ? ay : by;
            float sx1 = ax > bx ? ax : bx;
            float sy1 = ay > by ? ay : by;

            int32_t cx0 = (int32_t)floorf((sx0 - index->min_x) / index->cell_size_m);
            int32_t cy0 = (int32_t)floorf((sy0 - index->min_y) / index->cell_size_m);
            int32_t cx1 = (int32_t)floorf((sx1 - index->min_x) / index->cell_size_m);
            int32_t cy1 = (int32_t)floorf((sy1 - index->min_y) / index->cell_size_m);
            if (cx1 < cx0 || cy1 < cy0) {
                continue;
            }

            for (int32_t cy = cy0; cy <= cy1; ++cy) {
                for (int32_t cx = cx0; cx <= cx1; ++cx) {
                    if (!app_route_snap_entry_push(index, app_route_snap_cell_key(cx, cy), segment_index, &entry_capacity)) {
                        app_route_snap_index_free(index);
                        return false;
                    }
                }
            }
        }
    }

    if (index->entry_count == 0u || index->segment_count == 0u) {
        app_route_snap_index_free(index);
        return false;
    }

    qsort(index->entries, index->entry_count, sizeof(RouteSnapCellEntry), app_route_snap_cell_entry_compare);

    uint32_t unique_cells = 0u;
    for (uint32_t i = 0u; i < index->entry_count; ++i) {
        if (i == 0u || index->entries[i].key != index->entries[i - 1u].key) {
            unique_cells += 1u;
        }
    }
    index->cells = (RouteSnapCellSpan *)malloc(sizeof(RouteSnapCellSpan) * unique_cells);
    if (!index->cells) {
        app_route_snap_index_free(index);
        return false;
    }

    index->cell_count = 0u;
    for (uint32_t i = 0u; i < index->entry_count;) {
        uint64_t key = index->entries[i].key;
        uint32_t start = i;
        while (i < index->entry_count && index->entries[i].key == key) {
            i += 1u;
        }
        RouteSnapCellSpan *span = &index->cells[index->cell_count++];
        span->key = key;
        span->start = start;
        span->count = i - start;
    }

    index->segment_seen = (uint32_t *)calloc(index->segment_count, sizeof(uint32_t));
    if (!index->segment_seen) {
        app_route_snap_index_free(index);
        return false;
    }

    index->query_seq = 1u;
    index->ready = true;
    return true;
}

static int32_t app_route_snap_find_cell(const RouteSnapIndex *index, uint64_t key) {
    if (!index || !index->cells || index->cell_count == 0u) {
        return -1;
    }
    uint32_t lo = 0u;
    uint32_t hi = index->cell_count;
    while (lo < hi) {
        uint32_t mid = lo + ((hi - lo) / 2u);
        uint64_t mid_key = index->cells[mid].key;
        if (mid_key == key) {
            return (int32_t)mid;
        }
        if (mid_key < key) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return -1;
}

bool app_load_route_graph(AppState *app) {
    if (!app) {
        return false;
    }

    app_route_snap_index_free(&app->route_state_bridge.route_snap_index);

    char path[512];
    app_route_graph_path_for_region(app, path, sizeof(path));
    if (path[0] == '\0') {
        log_error("Failed to resolve graph path for region: %s", app->region.name);
        return false;
    }
    if (!route_state_load_graph(&app->route_state_bridge.route, path)) {
        log_error("Missing route graph for region: %s", app->region.name);
        return false;
    }
    if (!app_route_snap_index_build(&app->route_state_bridge.route.graph, &app->route_state_bridge.route_snap_index)) {
        log_error("Failed to build route snap index for region: %s", app->region.name);
    }
    if (app->worker_state_bridge.route_worker_enabled) {
        pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
        while (app->worker_state_bridge.route_worker_busy) {
            pthread_cond_wait(&app->worker_state_bridge.route_worker_cond, &app->worker_state_bridge.route_worker_mutex);
        }
        route_state_load_graph(&app->worker_state_bridge.route_worker_state, path);
        app->worker_state_bridge.route_job_pending = false;
        app_route_result_clear(&app->worker_state_bridge.route_result);
        app->worker_state_bridge.route_result_pending = false;
        app->route_state_bridge.route_recompute_scheduled = false;
        app->worker_state_bridge.route_latest_requested_id = 0u;
        app->worker_state_bridge.route_latest_submitted_id = 0u;
        app->worker_state_bridge.route_latest_applied_id = 0u;
        pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);
    }
    return true;
}

static bool app_find_nearest_node(const RouteGraph *graph, float world_x, float world_y, uint32_t *out_node, double *out_dist) {
    if (!graph || graph->node_count == 0 || !out_node || !out_dist) {
        return false;
    }

    uint32_t best = 0;
    double best_dist = 0.0;
    for (uint32_t i = 0; i < graph->node_count; ++i) {
        double dx = graph->node_x[i] - world_x;
        double dy = graph->node_y[i] - world_y;
        double dist = dx * dx + dy * dy;
        if (i == 0 || dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }

    *out_node = best;
    *out_dist = best_dist;
    return true;
}

static float app_route_snap_radius_m_for_pixels(const AppState *app, float pixels) {
    if (!app || pixels <= 0.0f) {
        return 0.0f;
    }
    float ppm = camera_pixels_per_meter(&app->view_state_bridge.camera);
    if (ppm <= 0.0f) {
        return 0.0f;
    }
    return pixels / ppm;
}

static float app_route_snap_radius_m(const AppState *app) {
    return app_route_snap_radius_m_for_pixels(app, kRouteHoverSnapRadiusPx);
}

static bool app_anchor_from_node(const AppState *app, uint32_t node, RouteEndpointAnchor *out_anchor) {
    if (!app || !out_anchor || !app->route_state_bridge.route.loaded || node >= app->route_state_bridge.route.graph.node_count) {
        return false;
    }

    memset(out_anchor, 0, sizeof(*out_anchor));
    out_anchor->valid = true;
    out_anchor->on_edge = false;
    out_anchor->node = node;
    out_anchor->edge_from = node;
    out_anchor->edge_to = node;
    out_anchor->world_x = (float)app->route_state_bridge.route.graph.node_x[node];
    out_anchor->world_y = (float)app->route_state_bridge.route.graph.node_y[node];
    return true;
}

static bool app_find_nearest_edge_anchor(AppState *app, float world_x, float world_y, float snap_radius_m, RouteEndpointAnchor *out_anchor) {
    if (!app || !out_anchor || !app->route_state_bridge.route.loaded || app->route_state_bridge.route.graph.node_count == 0 || app->route_state_bridge.route.graph.edge_count == 0) {
        return false;
    }
    if (snap_radius_m <= 0.0f) {
        return false;
    }

    const RouteGraph *graph = &app->route_state_bridge.route.graph;
    float snap_radius_sq = snap_radius_m * snap_radius_m;
    const RouteSnapIndex *index = &app->route_state_bridge.route_snap_index;
    double query_begin = time_now_seconds();
    uint32_t cells_visited = 0u;
    uint32_t segments_tested = 0u;
    uint32_t segment_hits = 0u;

    bool found = false;
    float best_dist_sq = 0.0f;
    RouteEndpointAnchor best = {0};

    if (index->ready && index->cells && index->segment_seen && index->cell_size_m > 0.0f) {
        int32_t cx0 = (int32_t)floorf((world_x - snap_radius_m - index->min_x) / index->cell_size_m);
        int32_t cy0 = (int32_t)floorf((world_y - snap_radius_m - index->min_y) / index->cell_size_m);
        int32_t cx1 = (int32_t)floorf((world_x + snap_radius_m - index->min_x) / index->cell_size_m);
        int32_t cy1 = (int32_t)floorf((world_y + snap_radius_m - index->min_y) / index->cell_size_m);

        app->route_state_bridge.route_snap_index.query_seq += 1u;
        if (app->route_state_bridge.route_snap_index.query_seq == 0u) {
            memset(app->route_state_bridge.route_snap_index.segment_seen, 0, sizeof(uint32_t) * app->route_state_bridge.route_snap_index.segment_count);
            app->route_state_bridge.route_snap_index.query_seq = 1u;
        }
        uint32_t query_seq = app->route_state_bridge.route_snap_index.query_seq;

        for (int32_t cy = cy0; cy <= cy1; ++cy) {
            for (int32_t cx = cx0; cx <= cx1; ++cx) {
                int32_t cell_idx = app_route_snap_find_cell(index, app_route_snap_cell_key(cx, cy));
                if (cell_idx < 0) {
                    continue;
                }
                cells_visited += 1u;
                const RouteSnapCellSpan *cell = &index->cells[cell_idx];
                uint32_t start = cell->start;
                uint32_t end = cell->start + cell->count;
                for (uint32_t i = start; i < end; ++i) {
                    uint32_t segment_index = index->entries[i].segment_index;
                    if (segment_index >= index->segment_count) {
                        continue;
                    }
                    if (app->route_state_bridge.route_snap_index.segment_seen[segment_index] == query_seq) {
                        continue;
                    }
                    app->route_state_bridge.route_snap_index.segment_seen[segment_index] = query_seq;

                    uint32_t from = index->segments[segment_index].from;
                    uint32_t to = index->segments[segment_index].to;
                    if (from >= graph->node_count || to >= graph->node_count || from == to) {
                        continue;
                    }
                    segments_tested += 1u;

                    float ax = (float)graph->node_x[from];
                    float ay = (float)graph->node_y[from];
                    float bx = (float)graph->node_x[to];
                    float by = (float)graph->node_y[to];
                    float vx = bx - ax;
                    float vy = by - ay;
                    float vv = vx * vx + vy * vy;
                    if (vv <= 0.0001f) {
                        continue;
                    }

                    float wx = world_x - ax;
                    float wy = world_y - ay;
                    float t = (wx * vx + wy * vy) / vv;
                    if (t < 0.0f) {
                        t = 0.0f;
                    } else if (t > 1.0f) {
                        t = 1.0f;
                    }

                    float px = ax + vx * t;
                    float py = ay + vy * t;
                    float dx = world_x - px;
                    float dy = world_y - py;
                    float dist_sq = dx * dx + dy * dy;
                    if (dist_sq > snap_radius_sq) {
                        continue;
                    }
                    segment_hits += 1u;
                    if (found && dist_sq >= best_dist_sq) {
                        continue;
                    }

                    float seg_len = sqrtf(vv);
                    float dist_to_from = seg_len * t;
                    float dist_to_to = seg_len * (1.0f - t);
                    uint32_t snap_node = dist_to_from <= dist_to_to ? from : to;

                    memset(&best, 0, sizeof(best));
                    best.valid = true;
                    best.on_edge = true;
                    best.node = snap_node;
                    best.edge_from = from;
                    best.edge_to = to;
                    best.world_x = px;
                    best.world_y = py;
                    best.dist_to_from_m = dist_to_from;
                    best.dist_to_to_m = dist_to_to;
                    best_dist_sq = dist_sq;
                    found = true;
                }
            }
        }
    } else {
        for (uint32_t from = 0; from < graph->node_count; ++from) {
            uint32_t edge_start = graph->edge_start[from];
            uint32_t edge_end = graph->edge_start[from + 1];
            for (uint32_t e = edge_start; e < edge_end; ++e) {
                uint32_t to = graph->edge_to[e];
                if (to >= graph->node_count || from == to) {
                    continue;
                }
                segments_tested += 1u;

                float ax = (float)graph->node_x[from];
                float ay = (float)graph->node_y[from];
                float bx = (float)graph->node_x[to];
                float by = (float)graph->node_y[to];
                float vx = bx - ax;
                float vy = by - ay;
                float vv = vx * vx + vy * vy;
                if (vv <= 0.0001f) {
                    continue;
                }

                float wx = world_x - ax;
                float wy = world_y - ay;
                float t = (wx * vx + wy * vy) / vv;
                if (t < 0.0f) {
                    t = 0.0f;
                } else if (t > 1.0f) {
                    t = 1.0f;
                }

                float px = ax + vx * t;
                float py = ay + vy * t;
                float dx = world_x - px;
                float dy = world_y - py;
                float dist_sq = dx * dx + dy * dy;
                if (dist_sq > snap_radius_sq) {
                    continue;
                }
                segment_hits += 1u;
                if (found && dist_sq >= best_dist_sq) {
                    continue;
                }

                float seg_len = sqrtf(vv);
                float dist_to_from = seg_len * t;
                float dist_to_to = seg_len * (1.0f - t);
                uint32_t snap_node = dist_to_from <= dist_to_to ? from : to;

                memset(&best, 0, sizeof(best));
                best.valid = true;
                best.on_edge = true;
                best.node = snap_node;
                best.edge_from = from;
                best.edge_to = to;
                best.world_x = px;
                best.world_y = py;
                best.dist_to_from_m = dist_to_from;
                best.dist_to_to_m = dist_to_to;
                best_dist_sq = dist_sq;
                found = true;
            }
        }
    }

    app->route_state_bridge.route_snap_debug_cells = cells_visited;
    app->route_state_bridge.route_snap_debug_segments = segments_tested;
    app->route_state_bridge.route_snap_debug_hits = segment_hits;
    app->route_state_bridge.route_snap_debug_query_ms = (float)((time_now_seconds() - query_begin) * 1000.0);

    if (!found) {
        return false;
    }

    *out_anchor = best;
    return true;
}

static bool app_is_near_node_with_radius(const AppState *app, float world_x, float world_y, float snap_radius_m, uint32_t *out_node) {
    if (!app || !app->route_state_bridge.route.loaded || app->route_state_bridge.route.graph.node_count == 0 || !out_node || snap_radius_m <= 0.0f) {
        return false;
    }

    uint32_t node = 0;
    double dist = 0.0;
    if (!app_find_nearest_node(&app->route_state_bridge.route.graph, world_x, world_y, &node, &dist)) {
        return false;
    }

    if (dist > (double)(snap_radius_m * snap_radius_m)) {
        return false;
    }

    *out_node = node;
    return true;
}

bool app_is_near_node(const AppState *app, float world_x, float world_y, uint32_t *out_node) {
    return app_is_near_node_with_radius(app, world_x, world_y, app_route_snap_radius_m(app), out_node);
}

bool app_pick_route_anchor(AppState *app, float world_x, float world_y, RouteEndpointAnchor *out_anchor) {
    if (!app || !out_anchor || !app->route_state_bridge.route.loaded) {
        return false;
    }

    float snap_radius_m = app_route_snap_radius_m_for_pixels(app, kRouteHoverSnapRadiusPx);
    if (app_find_nearest_edge_anchor(app, world_x, world_y, snap_radius_m, out_anchor)) {
        return true;
    }

    uint32_t node = 0;
    if (!app_is_near_node_with_radius(app, world_x, world_y, snap_radius_m, &node)) {
        return false;
    }
    return app_anchor_from_node(app, node, out_anchor);
}

bool app_pick_route_anchor_unbounded(AppState *app, float world_x, float world_y, RouteEndpointAnchor *out_anchor) {
    if (!app || !out_anchor || !app->route_state_bridge.route.loaded) {
        return false;
    }

    float snap_radius_m = app_route_snap_radius_m_for_pixels(app, kRoutePlacementSnapRadiusPx);
    if (app_find_nearest_edge_anchor(app, world_x, world_y, snap_radius_m, out_anchor)) {
        return true;
    }

    uint32_t node = 0;
    if (!app_is_near_node_with_radius(app, world_x, world_y, snap_radius_m, &node)) {
        return false;
    }
    return app_anchor_from_node(app, node, out_anchor);
}

void app_route_release_snap_index(AppState *app) {
    if (!app) {
        return;
    }
    app_route_snap_index_free(&app->route_state_bridge.route_snap_index);
}

void app_update_hover(AppState *app) {
    if (!app || !app->route_state_bridge.route.loaded) {
        if (app) {
            app->route_state_bridge.has_hover = false;
        }
        return;
    }

    float world_x = 0.0f;
    float world_y = 0.0f;
    camera_screen_to_world(&app->view_state_bridge.camera, (float)app->ui_state_bridge.input.mouse_x, (float)app->ui_state_bridge.input.mouse_y, app->width, app->height, &world_x, &world_y);

    RouteEndpointAnchor hover = {0};
    if (app_pick_route_anchor(app, world_x, world_y, &hover)) {
        app->route_state_bridge.hover_node = hover.node;
        app->route_state_bridge.hover_anchor = hover;
        app->route_state_bridge.has_hover = true;
    } else {
        memset(&app->route_state_bridge.hover_anchor, 0, sizeof(app->route_state_bridge.hover_anchor));
        app->route_state_bridge.has_hover = false;
    }
}

bool app_mouse_over_node(const AppState *app, uint32_t node, float radius) {
    if (!app || !app->route_state_bridge.route.loaded || node >= app->route_state_bridge.route.graph.node_count) {
        return false;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->view_state_bridge.camera,
                           (float)app->route_state_bridge.route.graph.node_x[node],
                           (float)app->route_state_bridge.route.graph.node_y[node],
                           app->width, app->height, &sx, &sy);
    float dx = (float)app->ui_state_bridge.input.mouse_x - sx;
    float dy = (float)app->ui_state_bridge.input.mouse_y - sy;
    return (dx * dx + dy * dy) <= radius * radius;
}

bool app_mouse_over_anchor(const AppState *app, const RouteEndpointAnchor *anchor, float radius) {
    if (!app || !anchor || !anchor->valid || radius <= 0.0f) {
        return false;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->view_state_bridge.camera, anchor->world_x, anchor->world_y, app->width, app->height, &sx, &sy);
    float dx = (float)app->ui_state_bridge.input.mouse_x - sx;
    float dy = (float)app->ui_state_bridge.input.mouse_y - sy;
    return (dx * dx + dy * dy) <= radius * radius;
}

void app_draw_hover_marker(AppState *app) {
    if (!app || !app->route_state_bridge.has_hover || !app->route_state_bridge.route.loaded || !app->route_state_bridge.hover_anchor.valid) {
        return;
    }

    if ((app->route_state_bridge.start_anchor.valid && app_mouse_over_anchor(app, &app->route_state_bridge.start_anchor, 4.5f)) ||
        (app->route_state_bridge.goal_anchor.valid && app_mouse_over_anchor(app, &app->route_state_bridge.goal_anchor, 4.5f))) {
        return;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->view_state_bridge.camera, app->route_state_bridge.hover_anchor.world_x, app->route_state_bridge.hover_anchor.world_y, app->width, app->height, &sx, &sy);
    renderer_set_draw_color(&app->renderer, 80, 200, 255, 220);
    SDL_FRect rect = {sx - 5.0f, sy - 5.0f, 10.0f, 10.0f};
    renderer_draw_rect(&app->renderer, &rect);

    if (app->route_state_bridge.route_edge_snap_debug && app->route_state_bridge.hover_anchor.on_edge &&
        app->route_state_bridge.hover_anchor.edge_from < app->route_state_bridge.route.graph.node_count &&
        app->route_state_bridge.hover_anchor.edge_to < app->route_state_bridge.route.graph.node_count) {
        float ax = 0.0f;
        float ay = 0.0f;
        float bx = 0.0f;
        float by = 0.0f;
        camera_world_to_screen(&app->view_state_bridge.camera,
                               (float)app->route_state_bridge.route.graph.node_x[app->route_state_bridge.hover_anchor.edge_from],
                               (float)app->route_state_bridge.route.graph.node_y[app->route_state_bridge.hover_anchor.edge_from],
                               app->width, app->height, &ax, &ay);
        camera_world_to_screen(&app->view_state_bridge.camera,
                               (float)app->route_state_bridge.route.graph.node_x[app->route_state_bridge.hover_anchor.edge_to],
                               (float)app->route_state_bridge.route.graph.node_y[app->route_state_bridge.hover_anchor.edge_to],
                               app->width, app->height, &bx, &by);
        renderer_set_draw_color(&app->renderer, 255, 210, 70, 220);
        renderer_draw_line(&app->renderer, ax, ay, bx, by);
    }
}

const RoutePath *app_route_primary_path(const AppState *app, uint32_t *out_alt_index) {
    if (out_alt_index) {
        *out_alt_index = UINT32_MAX;
    }
    if (!app) {
        return NULL;
    }

    bool has_any_alternatives = app->route_state_bridge.route.alternatives.count > 0;
    for (uint32_t i = 0; i < app->route_state_bridge.route.alternatives.count && i < ROUTE_ALTERNATIVE_MAX; ++i) {
        if (app->route_state_bridge.route.alternatives.objectives[i] != app->route_state_bridge.route.objective) {
            continue;
        }
        if (!app->route_state_bridge.route_alt_visible[i]) {
            continue;
        }
        if (app->route_state_bridge.route.alternatives.paths[i].count < 2) {
            continue;
        }
        if (out_alt_index) {
            *out_alt_index = i;
        }
        return &app->route_state_bridge.route.alternatives.paths[i];
    }

    for (uint32_t i = 0; i < app->route_state_bridge.route.alternatives.count && i < ROUTE_ALTERNATIVE_MAX; ++i) {
        if (!app->route_state_bridge.route_alt_visible[i]) {
            continue;
        }
        if (app->route_state_bridge.route.alternatives.paths[i].count < 2) {
            continue;
        }
        if (out_alt_index) {
            *out_alt_index = i;
        }
        return &app->route_state_bridge.route.alternatives.paths[i];
    }

    if (has_any_alternatives) {
        return NULL;
    }

    if (app->route_state_bridge.route.path.count >= 2) {
        return &app->route_state_bridge.route.path;
    }
    return NULL;
}

bool app_recompute_route(AppState *app) {
    if (!app || !app->route_state_bridge.route.has_start || !app->route_state_bridge.route.has_goal) {
        return false;
    }

    app_route_schedule_recompute(app, 0.0);
    return true;
}

typedef struct RouteEndpointOption {
    bool valid;
    uint32_t node;
    float extra_length_m;
    float extra_time_s;
    float extra_cost;
} RouteEndpointOption;

static bool app_route_find_edge_index(const RouteGraph *graph, uint32_t from, uint32_t to, uint32_t *out_edge) {
    if (!graph || from >= graph->node_count || to >= graph->node_count) {
        return false;
    }
    uint32_t begin = graph->edge_start[from];
    uint32_t end = graph->edge_start[from + 1u];
    for (uint32_t e = begin; e < end; ++e) {
        if (graph->edge_to[e] == to) {
            if (out_edge) {
                *out_edge = e;
            }
            return true;
        }
    }
    return false;
}

static float app_route_edge_cost_for_objective(const RouteGraph *graph, uint32_t edge_index, RouteObjective objective) {
    if (!graph || edge_index >= graph->edge_count) {
        return INFINITY;
    }
    float length_m = graph->edge_length[edge_index];
    float speed_mps = graph->edge_speed[edge_index];
    if (speed_mps <= 0.1f) {
        speed_mps = 0.1f;
    }

    switch (objective) {
        case ROUTE_OBJECTIVE_SHORTEST_DISTANCE:
            return length_m;
        case ROUTE_OBJECTIVE_LOWEST_TIME:
            return length_m / speed_mps;
        case ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN: {
            float grade = graph->edge_grade ? graph->edge_grade[edge_index] : 0.0f;
            float gain = grade > 0.0f ? grade * length_m : 0.0f;
            return gain + length_m * 0.01f;
        }
        case ROUTE_OBJECTIVE_MOST_TIME_ABOVE_SPEED_THRESHOLD: {
            float speed_limit = graph->edge_speed_limit ? graph->edge_speed_limit[edge_index] : 0.0f;
            float time_s = length_m / speed_mps;
            float bonus = speed_limit >= 17.88f ? time_s : 0.0f;
            return time_s - bonus * 0.5f + length_m * 0.02f;
        }
        default:
            return length_m;
    }
}

static float app_route_path_objective_cost(const RouteGraph *graph, const RoutePath *path, RouteObjective objective) {
    if (!graph || !path || path->count < 2u) {
        return INFINITY;
    }
    float cost = 0.0f;
    for (uint32_t i = 0u; i + 1u < path->count; ++i) {
        uint32_t from = path->nodes[i];
        uint32_t to = path->nodes[i + 1u];
        uint32_t edge = 0u;
        if (!app_route_find_edge_index(graph, from, to, &edge)) {
            return INFINITY;
        }
        cost += app_route_edge_cost_for_objective(graph, edge, objective);
    }
    return cost;
}

static float app_route_partial_time_for_mode(const RouteGraph *graph, uint32_t edge_index, float partial_length_m,
                                             RouteTravelMode mode) {
    if (!graph || edge_index >= graph->edge_count || partial_length_m <= 0.0f) {
        return 0.0f;
    }
    if (mode == ROUTE_MODE_WALK) {
        const float walk_speed_mps = 1.4f;
        return partial_length_m / walk_speed_mps;
    }
    float speed = graph->edge_speed[edge_index];
    if (speed <= 0.1f) {
        speed = 0.1f;
    }
    return partial_length_m / speed;
}

static RouteEndpointOption app_route_build_option(const RouteGraph *graph, uint32_t node, uint32_t edge_index,
                                                  float partial_length_m, RouteObjective objective, RouteTravelMode mode) {
    RouteEndpointOption opt = {0};
    if (!graph || edge_index >= graph->edge_count || node >= graph->node_count || partial_length_m < 0.0f) {
        return opt;
    }
    float edge_length = graph->edge_length[edge_index];
    if (edge_length <= 0.001f) {
        return opt;
    }
    float frac = partial_length_m / edge_length;
    if (frac < 0.0f) {
        frac = 0.0f;
    } else if (frac > 1.0f) {
        frac = 1.0f;
    }
    opt.valid = true;
    opt.node = node;
    opt.extra_length_m = partial_length_m;
    opt.extra_time_s = app_route_partial_time_for_mode(graph, edge_index, partial_length_m, mode);
    opt.extra_cost = app_route_edge_cost_for_objective(graph, edge_index, objective) * frac;
    return opt;
}

static uint32_t app_route_endpoint_options(const RouteGraph *graph,
                                           const RouteEndpointAnchor *anchor,
                                           bool is_start,
                                           RouteObjective objective,
                                           RouteTravelMode mode,
                                           uint32_t fallback_node,
                                           RouteEndpointOption *out_options,
                                           uint32_t max_options) {
    if (!out_options || max_options == 0u || !graph || graph->node_count == 0u) {
        return 0u;
    }
    memset(out_options, 0, sizeof(RouteEndpointOption) * max_options);

    if (!anchor || !anchor->valid || !anchor->on_edge ||
        anchor->edge_from >= graph->node_count || anchor->edge_to >= graph->node_count || anchor->edge_from == anchor->edge_to) {
        if (fallback_node >= graph->node_count) {
            return 0u;
        }
        out_options[0].valid = true;
        out_options[0].node = fallback_node;
        return 1u;
    }

    uint32_t count = 0u;
    uint32_t edge_fwd = 0u;
    bool has_fwd = app_route_find_edge_index(graph, anchor->edge_from, anchor->edge_to, &edge_fwd);
    uint32_t edge_rev = 0u;
    bool has_rev = app_route_find_edge_index(graph, anchor->edge_to, anchor->edge_from, &edge_rev);

    if (is_start) {
        if (has_fwd && count < max_options) {
            out_options[count++] = app_route_build_option(graph, anchor->edge_to, edge_fwd, anchor->dist_to_to_m, objective, mode);
        }
        if (has_rev && count < max_options) {
            out_options[count++] = app_route_build_option(graph, anchor->edge_from, edge_rev, anchor->dist_to_from_m, objective, mode);
        }
    } else {
        if (has_fwd && count < max_options) {
            out_options[count++] = app_route_build_option(graph, anchor->edge_from, edge_fwd, anchor->dist_to_from_m, objective, mode);
        }
        if (has_rev && count < max_options) {
            out_options[count++] = app_route_build_option(graph, anchor->edge_to, edge_rev, anchor->dist_to_to_m, objective, mode);
        }
    }

    if (count == 0u && fallback_node < graph->node_count) {
        out_options[0].valid = true;
        out_options[0].node = fallback_node;
        count = 1u;
    }
    return count;
}

static bool app_route_run_phase2_endpoint_solve(RouteState *state,
                                                const RouteEndpointAnchor *start_anchor,
                                                const RouteEndpointAnchor *goal_anchor,
                                                uint32_t fallback_start_node,
                                                uint32_t fallback_goal_node,
                                                RouteObjective objective,
                                                RouteTravelMode mode,
                                                uint32_t *out_start_node,
                                                uint32_t *out_goal_node) {
    if (!state || !state->loaded) {
        return false;
    }

    RouteEndpointOption start_opts[2];
    RouteEndpointOption goal_opts[2];
    uint32_t start_count = app_route_endpoint_options(&state->graph, start_anchor, true, objective, mode, fallback_start_node, start_opts, 2u);
    uint32_t goal_count = app_route_endpoint_options(&state->graph, goal_anchor, false, objective, mode, fallback_goal_node, goal_opts, 2u);
    if (start_count == 0u || goal_count == 0u) {
        return false;
    }

    bool have_best = false;
    float best_score = 0.0f;
    uint32_t best_start = fallback_start_node;
    uint32_t best_goal = fallback_goal_node;
    float best_extra_len = 0.0f;
    float best_extra_time = 0.0f;

    for (uint32_t si = 0u; si < start_count; ++si) {
        if (!start_opts[si].valid) {
            continue;
        }
        for (uint32_t gi = 0u; gi < goal_count; ++gi) {
            if (!goal_opts[gi].valid) {
                continue;
            }
            if (!route_state_route(state, start_opts[si].node, goal_opts[gi].node)) {
                continue;
            }
            float core_cost = app_route_path_objective_cost(&state->graph, &state->path, objective);
            if (!isfinite(core_cost)) {
                continue;
            }
            float score = core_cost + start_opts[si].extra_cost + goal_opts[gi].extra_cost;
            if (have_best && score >= best_score) {
                continue;
            }
            best_score = score;
            best_start = start_opts[si].node;
            best_goal = goal_opts[gi].node;
            best_extra_len = start_opts[si].extra_length_m + goal_opts[gi].extra_length_m;
            best_extra_time = start_opts[si].extra_time_s + goal_opts[gi].extra_time_s;
            have_best = true;
        }
    }

    if (!have_best) {
        return false;
    }

    if (!route_state_route(state, best_start, best_goal)) {
        return false;
    }
    state->path.total_length_m += best_extra_len;
    state->path.total_time_s += best_extra_time;
    if (out_start_node) {
        *out_start_node = best_start;
    }
    if (out_goal_node) {
        *out_goal_node = best_goal;
    }
    return true;
}

static void *app_route_worker_thread_main(void *userdata) {
    AppState *app = (AppState *)userdata;
    if (!app) {
        return NULL;
    }

    for (;;) {
        RouteComputeJob job = {0};
        pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
        while (app->worker_state_bridge.route_worker_running && !app->worker_state_bridge.route_job_pending) {
            pthread_cond_wait(&app->worker_state_bridge.route_worker_cond, &app->worker_state_bridge.route_worker_mutex);
        }
        if (!app->worker_state_bridge.route_worker_running) {
            pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);
            break;
        }
        job = app->worker_state_bridge.route_job;
        app->worker_state_bridge.route_job_pending = false;
        app->worker_state_bridge.route_worker_busy = true;
        pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);

        RouteComputeResult result = {0};
        result.request_id = job.request_id;
        result.start_node = job.start_node;
        result.goal_node = job.goal_node;
        result.objective = job.objective;
        result.mode = job.mode;

        app->worker_state_bridge.route_worker_state.objective = job.objective;
        app->worker_state_bridge.route_worker_state.mode = job.mode;
        bool ok = app_route_run_phase2_endpoint_solve(&app->worker_state_bridge.route_worker_state,
                                                      &job.start_anchor,
                                                      &job.goal_anchor,
                                                      job.start_node,
                                                      job.goal_node,
                                                      job.objective,
                                                      job.mode,
                                                      &result.start_node,
                                                      &result.goal_node);
        result.ok = ok;
        if (ok) {
            result.has_transfer = app->worker_state_bridge.route_worker_state.has_transfer;
            result.transfer_node = app->worker_state_bridge.route_worker_state.transfer_node;
            result.path = app->worker_state_bridge.route_worker_state.path;
            result.drive_path = app->worker_state_bridge.route_worker_state.drive_path;
            result.walk_path = app->worker_state_bridge.route_worker_state.walk_path;
            result.alternatives = app->worker_state_bridge.route_worker_state.alternatives;
            memset(&app->worker_state_bridge.route_worker_state.path, 0, sizeof(app->worker_state_bridge.route_worker_state.path));
            memset(&app->worker_state_bridge.route_worker_state.drive_path, 0, sizeof(app->worker_state_bridge.route_worker_state.drive_path));
            memset(&app->worker_state_bridge.route_worker_state.walk_path, 0, sizeof(app->worker_state_bridge.route_worker_state.walk_path));
            memset(&app->worker_state_bridge.route_worker_state.alternatives, 0, sizeof(app->worker_state_bridge.route_worker_state.alternatives));
        }

        pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
        app_route_result_clear(&app->worker_state_bridge.route_result);
        app->worker_state_bridge.route_result = result;
        app->worker_state_bridge.route_result_pending = true;
        app->worker_state_bridge.route_worker_busy = false;
        pthread_cond_broadcast(&app->worker_state_bridge.route_worker_cond);
        pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);
    }

    return NULL;
}

bool app_route_worker_init(AppState *app) {
    if (!app) {
        return false;
    }
    app->worker_state_bridge.route_worker_enabled = false;
    app->worker_state_bridge.route_worker_running = false;
    app->worker_state_bridge.route_worker_busy = false;
    app->worker_state_bridge.route_job_pending = false;
    app->worker_state_bridge.route_result_pending = false;
    app->worker_state_bridge.route_latest_requested_id = 0u;
    app->worker_state_bridge.route_latest_submitted_id = 0u;
    app->worker_state_bridge.route_latest_applied_id = 0u;
    app->route_state_bridge.route_recompute_scheduled = false;
    app->route_state_bridge.route_recompute_due_time = 0.0;
    memset(&app->worker_state_bridge.route_job, 0, sizeof(app->worker_state_bridge.route_job));
    memset(&app->worker_state_bridge.route_result, 0, sizeof(app->worker_state_bridge.route_result));
    route_state_init(&app->worker_state_bridge.route_worker_state);
    if (pthread_mutex_init(&app->worker_state_bridge.route_worker_mutex, NULL) != 0) {
        return false;
    }
    if (pthread_cond_init(&app->worker_state_bridge.route_worker_cond, NULL) != 0) {
        pthread_mutex_destroy(&app->worker_state_bridge.route_worker_mutex);
        return false;
    }
    app->worker_state_bridge.route_worker_running = true;
    if (pthread_create(&app->worker_state_bridge.route_worker_thread, NULL, app_route_worker_thread_main, app) != 0) {
        app->worker_state_bridge.route_worker_running = false;
        pthread_cond_destroy(&app->worker_state_bridge.route_worker_cond);
        pthread_mutex_destroy(&app->worker_state_bridge.route_worker_mutex);
        route_state_shutdown(&app->worker_state_bridge.route_worker_state);
        return false;
    }
    app->worker_state_bridge.route_worker_enabled = true;
    return true;
}

void app_route_worker_shutdown(AppState *app) {
    if (!app || !app->worker_state_bridge.route_worker_enabled) {
        return;
    }
    pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
    app->worker_state_bridge.route_worker_running = false;
    pthread_cond_broadcast(&app->worker_state_bridge.route_worker_cond);
    pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);
    pthread_join(app->worker_state_bridge.route_worker_thread, NULL);
    pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
    app_route_result_clear(&app->worker_state_bridge.route_result);
    app->worker_state_bridge.route_result_pending = false;
    app->worker_state_bridge.route_job_pending = false;
    pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);
    pthread_cond_destroy(&app->worker_state_bridge.route_worker_cond);
    pthread_mutex_destroy(&app->worker_state_bridge.route_worker_mutex);
    route_state_shutdown(&app->worker_state_bridge.route_worker_state);
    app->worker_state_bridge.route_worker_enabled = false;
}

void app_route_worker_clear(AppState *app) {
    if (!app || !app->worker_state_bridge.route_worker_enabled) {
        return;
    }
    pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
    while (app->worker_state_bridge.route_worker_busy) {
        pthread_cond_wait(&app->worker_state_bridge.route_worker_cond, &app->worker_state_bridge.route_worker_mutex);
    }
    app->worker_state_bridge.route_job_pending = false;
    app_route_result_clear(&app->worker_state_bridge.route_result);
    app->worker_state_bridge.route_result_pending = false;
    app->route_state_bridge.route_recompute_scheduled = false;
    app->worker_state_bridge.route_latest_requested_id = 0u;
    app->worker_state_bridge.route_latest_submitted_id = 0u;
    app->worker_state_bridge.route_latest_applied_id = 0u;
    route_state_clear(&app->worker_state_bridge.route_worker_state);
    pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);
}

void app_route_schedule_recompute(AppState *app, double debounce_sec) {
    if (!app || !app->worker_state_bridge.route_worker_enabled || !app->route_state_bridge.route.has_start || !app->route_state_bridge.route.has_goal) {
        return;
    }
    if (debounce_sec < 0.0) {
        debounce_sec = 0.0;
    }

    double now = time_now_seconds();
    pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
    app->route_state_bridge.route_recompute_scheduled = true;
    app->route_state_bridge.route_recompute_due_time = now + debounce_sec;
    app->worker_state_bridge.route_latest_requested_id += 1u;
    pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);
}

void app_route_poll_result(AppState *app) {
    if (!app || !app->worker_state_bridge.route_worker_enabled) {
        return;
    }

    RouteComputeResult result = {0};
    bool have_result = false;
    RouteComputeJob submit_job = {0};
    bool should_submit = false;
    double now = time_now_seconds();

    pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
    if (app->worker_state_bridge.route_result_pending) {
        result = app->worker_state_bridge.route_result;
        memset(&app->worker_state_bridge.route_result, 0, sizeof(app->worker_state_bridge.route_result));
        app->worker_state_bridge.route_result_pending = false;
        have_result = true;
    }
    if (app->route_state_bridge.route_recompute_scheduled && now >= app->route_state_bridge.route_recompute_due_time &&
        app->route_state_bridge.route.loaded && app->route_state_bridge.route.has_start && app->route_state_bridge.route.has_goal) {
        submit_job.request_id = app->worker_state_bridge.route_latest_requested_id;
        submit_job.start_node = app->route_state_bridge.route.start_node;
        submit_job.goal_node = app->route_state_bridge.route.goal_node;
        submit_job.start_anchor = app->route_state_bridge.start_anchor;
        submit_job.goal_anchor = app->route_state_bridge.goal_anchor;
        submit_job.objective = app->route_state_bridge.route.objective;
        submit_job.mode = app->route_state_bridge.route.mode;
        app->worker_state_bridge.route_job = submit_job;
        app->worker_state_bridge.route_job_pending = true;
        app->route_state_bridge.route_recompute_scheduled = false;
        app->worker_state_bridge.route_latest_submitted_id = submit_job.request_id;
        should_submit = true;
    }
    if (should_submit) {
        pthread_cond_signal(&app->worker_state_bridge.route_worker_cond);
    }
    pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);

    if (!have_result) {
        return;
    }
    if (result.request_id != app->worker_state_bridge.route_latest_submitted_id || result.request_id < app->worker_state_bridge.route_latest_applied_id) {
        app_route_result_clear(&result);
        return;
    }
    if (!result.ok) {
        app_route_result_clear(&result);
        return;
    }

    bool visible_by_objective[ROUTE_OBJECTIVE_COUNT];
    for (uint32_t i = 0; i < ROUTE_OBJECTIVE_COUNT; ++i) {
        visible_by_objective[i] = true;
    }
    for (uint32_t i = 0; i < app->route_state_bridge.route.alternatives.count && i < ROUTE_ALTERNATIVE_MAX; ++i) {
        int objective_idx = app_route_objective_index(app->route_state_bridge.route.alternatives.objectives[i]);
        if (objective_idx >= 0) {
            visible_by_objective[objective_idx] = app->route_state_bridge.route_alt_visible[i];
        }
    }

    app_route_path_move(&app->route_state_bridge.route.path, &result.path);
    app_route_path_move(&app->route_state_bridge.route.drive_path, &result.drive_path);
    app_route_path_move(&app->route_state_bridge.route.walk_path, &result.walk_path);
    app_route_alternatives_move(&app->route_state_bridge.route.alternatives, &result.alternatives);
    app->ui_state_bridge.hud_route_panel_layout_dirty = true;
    for (uint32_t i = 0; i < ROUTE_ALTERNATIVE_MAX; ++i) {
        if (i < app->route_state_bridge.route.alternatives.count) {
            int objective_idx = app_route_objective_index(app->route_state_bridge.route.alternatives.objectives[i]);
            app->route_state_bridge.route_alt_visible[i] = objective_idx >= 0 ? visible_by_objective[objective_idx] : true;
        } else {
            app->route_state_bridge.route_alt_visible[i] = false;
        }
    }
    app->route_state_bridge.route.start_node = result.start_node;
    app->route_state_bridge.route.goal_node = result.goal_node;
    app->route_state_bridge.route.has_start = true;
    app->route_state_bridge.route.has_goal = true;
    app->route_state_bridge.route.objective = result.objective;
    app->route_state_bridge.route.mode = result.mode;
    app->route_state_bridge.route.has_transfer = result.has_transfer;
    app->route_state_bridge.route.transfer_node = result.transfer_node;
    app->worker_state_bridge.route_latest_applied_id = result.request_id;
    app_playback_reset(app);
}
