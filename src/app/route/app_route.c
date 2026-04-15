#include "app/app_internal.h"
#include "app/app_route_internal.h"

#include "core/log.h"
#include "core/time.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const float kRouteSnapCellSizeM = 96.0f;

void app_route_result_clear(RouteComputeResult *result) {
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

static void app_route_graph_path_for_region(const AppState *app, char *out_path, size_t out_size) {
    if (!app || !out_path || out_size == 0u) {
        return;
    }
    if (!region_graph_path(&app->region, out_path, out_size)) {
        out_path[0] = '\0';
    }
}

static uint32_t app_route_next_request_id(uint32_t current) {
    uint32_t next = current + 1u;
    if (next == 0u) {
        next = 1u;
    }
    return next;
}

void app_route_snap_index_free(RouteSnapIndex *index) {
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

bool app_route_snap_index_build(const RouteGraph *graph, RouteSnapIndex *index) {
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

bool app_load_route_graph(AppState *app) {
    if (!app) {
        return false;
    }

    app_route_snap_index_free(&app->route_state_bridge.route_snap_index);
    route_state_clear(&app->route_state_bridge.route);
    route_graph_free(&app->route_state_bridge.route.graph);
    app->route_state_bridge.route.loaded = false;
    app->route_state_bridge.has_hover = false;
    app->route_state_bridge.route_graph_loading = false;
    app->route_state_bridge.hover_node = 0u;
    memset(&app->route_state_bridge.hover_anchor, 0, sizeof(app->route_state_bridge.hover_anchor));
    memset(&app->route_state_bridge.start_anchor, 0, sizeof(app->route_state_bridge.start_anchor));
    memset(&app->route_state_bridge.goal_anchor, 0, sizeof(app->route_state_bridge.goal_anchor));

    char path[512];
    app_route_graph_path_for_region(app, path, sizeof(path));
    if (path[0] == '\0') {
        log_error("Failed to resolve graph path for region: %s", app->region.name);
        return false;
    }

    if (app->worker_state_bridge.route_worker_enabled) {
        pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
        app->worker_state_bridge.route_job_pending = false;
        app_route_result_clear(&app->worker_state_bridge.route_result);
        app->worker_state_bridge.route_result_pending = false;
        app->worker_state_bridge.route_graph_job_pending = false;
        if (app->worker_state_bridge.route_graph_result_pending) {
            route_state_shutdown(&app->worker_state_bridge.route_graph_result_state);
            route_state_init(&app->worker_state_bridge.route_graph_result_state);
            app_route_snap_index_free(&app->worker_state_bridge.route_graph_result_snap_index);
            app->worker_state_bridge.route_graph_result_pending = false;
        }
        app->route_state_bridge.route_recompute_scheduled = false;
        app_worker_contract_reset_route_pipeline(app);

        app->route_state_bridge.route_graph_load_request_id =
            app_route_next_request_id(app->route_state_bridge.route_graph_load_request_id);
        app->worker_state_bridge.route_graph_job_request_id =
            app->route_state_bridge.route_graph_load_request_id;
        snprintf(app->worker_state_bridge.route_graph_job_path,
                 sizeof(app->worker_state_bridge.route_graph_job_path),
                 "%s",
                 path);
        app->worker_state_bridge.route_graph_job_pending = true;
        app->route_state_bridge.route_graph_loading = true;
        pthread_cond_signal(&app->worker_state_bridge.route_worker_cond);
        pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);
        return true;
    }

    if (!route_state_load_graph(&app->route_state_bridge.route, path)) {
        log_error("Missing route graph for region: %s", app->region.name);
        return false;
    }
    if (!app_route_snap_index_build(&app->route_state_bridge.route.graph, &app->route_state_bridge.route_snap_index)) {
        log_error("Failed to build route snap index for region: %s", app->region.name);
    }
    return true;
}

void app_route_release_snap_index(AppState *app) {
    if (!app) {
        return;
    }
    app_route_snap_index_free(&app->route_state_bridge.route_snap_index);
}

void app_route_poll_result(AppState *app) {
    if (!app || !app->worker_state_bridge.route_worker_enabled) {
        return;
    }

    bool have_graph_result = false;
    bool graph_result_ok = false;
    uint32_t graph_result_request_id = 0u;
    RouteState graph_result_state;
    RouteSnapIndex graph_result_snap_index = {0};
    RouteComputeResult result = {0};
    bool have_result = false;
    RouteComputeJob submit_job = {0};
    bool should_submit = false;
    double now = time_now_seconds();

    route_state_init(&graph_result_state);
    pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
    if (app->worker_state_bridge.route_graph_result_pending) {
        have_graph_result = true;
        graph_result_ok = app->worker_state_bridge.route_graph_result_ok;
        graph_result_request_id = app->worker_state_bridge.route_graph_result_request_id;
        app->worker_state_bridge.route_graph_result_pending = false;
        if (graph_result_ok) {
            graph_result_state = app->worker_state_bridge.route_graph_result_state;
            memset(&app->worker_state_bridge.route_graph_result_state, 0, sizeof(app->worker_state_bridge.route_graph_result_state));
            graph_result_snap_index = app->worker_state_bridge.route_graph_result_snap_index;
            memset(&app->worker_state_bridge.route_graph_result_snap_index, 0, sizeof(app->worker_state_bridge.route_graph_result_snap_index));
        } else {
            route_state_shutdown(&app->worker_state_bridge.route_graph_result_state);
            route_state_init(&app->worker_state_bridge.route_graph_result_state);
            app_route_snap_index_free(&app->worker_state_bridge.route_graph_result_snap_index);
        }
    }
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
        app_worker_contract_note_route_submitted(app, submit_job.request_id);
        should_submit = true;
    }
    if (should_submit) {
        pthread_cond_signal(&app->worker_state_bridge.route_worker_cond);
    }
    pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);

    app_route_apply_graph_result(app,
                                 have_graph_result,
                                 graph_result_ok,
                                 graph_result_request_id,
                                 &graph_result_state,
                                 &graph_result_snap_index);

    if (!have_result) {
        return;
    }
    app_route_apply_worker_result(app, &result);
}
