#include "app/app_internal.h"

#include "core/time.h"

#include <math.h>
#include <string.h>

static const float kRouteHoverSnapRadiusPx = 16.0f;
static const float kRoutePlacementSnapRadiusPx = 24.0f;

static uint64_t app_route_anchor_cell_key(int32_t cx, int32_t cy) {
    return ((uint64_t)(uint32_t)cx << 32u) | (uint64_t)(uint32_t)cy;
}

static int32_t app_route_anchor_find_cell(const RouteSnapIndex *index, uint64_t key) {
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
                int32_t cell_idx = app_route_anchor_find_cell(index, app_route_anchor_cell_key(cx, cy));
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
