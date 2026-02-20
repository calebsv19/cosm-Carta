#include "app/app_internal.h"

#include "core/log.h"
#include "core/time.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

static void app_route_path_move(RoutePath *dst, RoutePath *src) {
    if (!dst || !src) {
        return;
    }
    route_path_free(dst);
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
    memset(result, 0, sizeof(*result));
}

static void app_route_graph_path_for_region(const AppState *app, char *out_path, size_t out_size) {
    if (!app || !out_path || out_size == 0u) {
        return;
    }
    snprintf(out_path, out_size, "data/regions/%s/graph/graph.bin", app->region.name);
}

bool app_load_route_graph(AppState *app) {
    if (!app) {
        return false;
    }

    char path[512];
    app_route_graph_path_for_region(app, path, sizeof(path));
    if (!route_state_load_graph(&app->route, path)) {
        log_error("Missing route graph for region: %s", app->region.name);
        return false;
    }
    if (app->route_worker_enabled) {
        pthread_mutex_lock(&app->route_worker_mutex);
        while (app->route_worker_busy) {
            pthread_cond_wait(&app->route_worker_cond, &app->route_worker_mutex);
        }
        route_state_load_graph(&app->route_worker_state, path);
        app->route_job_pending = false;
        app_route_result_clear(&app->route_result);
        app->route_result_pending = false;
        app->route_recompute_scheduled = false;
        app->route_latest_requested_id = 0u;
        app->route_latest_submitted_id = 0u;
        app->route_latest_applied_id = 0u;
        pthread_mutex_unlock(&app->route_worker_mutex);
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

bool app_is_near_node(const AppState *app, float world_x, float world_y, uint32_t *out_node) {
    if (!app || !app->route.loaded || app->route.graph.node_count == 0 || !out_node) {
        return false;
    }

    uint32_t node = 0;
    double dist = 0.0;
    if (!app_find_nearest_node(&app->route.graph, world_x, world_y, &node, &dist)) {
        return false;
    }

    float ppm = camera_pixels_per_meter(&app->camera);
    if (ppm <= 0.0f) {
        return false;
    }

    float snap_radius_m = 12.0f / ppm;
    if (dist > (double)(snap_radius_m * snap_radius_m)) {
        return false;
    }

    *out_node = node;
    return true;
}

void app_update_hover(AppState *app) {
    if (!app || !app->route.loaded) {
        if (app) {
            app->has_hover = false;
        }
        return;
    }

    float world_x = 0.0f;
    float world_y = 0.0f;
    camera_screen_to_world(&app->camera, (float)app->input.mouse_x, (float)app->input.mouse_y, app->width, app->height, &world_x, &world_y);

    uint32_t node = 0;
    if (app_is_near_node(app, world_x, world_y, &node)) {
        app->hover_node = node;
        app->has_hover = true;
    } else {
        app->has_hover = false;
    }
}

bool app_mouse_over_node(const AppState *app, uint32_t node, float radius) {
    if (!app || !app->route.loaded || node >= app->route.graph.node_count) {
        return false;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->camera,
                           (float)app->route.graph.node_x[node],
                           (float)app->route.graph.node_y[node],
                           app->width, app->height, &sx, &sy);
    float dx = (float)app->input.mouse_x - sx;
    float dy = (float)app->input.mouse_y - sy;
    return (dx * dx + dy * dy) <= radius * radius;
}

void app_draw_hover_marker(AppState *app) {
    if (!app || !app->has_hover || !app->route.loaded || app->hover_node >= app->route.graph.node_count) {
        return;
    }

    if ((app->route.has_start && app->hover_node == app->route.start_node) ||
        (app->route.has_goal && app->hover_node == app->route.goal_node)) {
        return;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->camera,
                           (float)app->route.graph.node_x[app->hover_node],
                           (float)app->route.graph.node_y[app->hover_node],
                           app->width, app->height, &sx, &sy);
    renderer_set_draw_color(&app->renderer, 80, 200, 255, 220);
    SDL_FRect rect = {sx - 5.0f, sy - 5.0f, 10.0f, 10.0f};
    renderer_draw_rect(&app->renderer, &rect);
}

bool app_recompute_route(AppState *app) {
    if (!app || !app->route.has_start || !app->route.has_goal) {
        return false;
    }

    app_route_schedule_recompute(app, 0.0);
    return true;
}

static void *app_route_worker_thread_main(void *userdata) {
    AppState *app = (AppState *)userdata;
    if (!app) {
        return NULL;
    }

    for (;;) {
        RouteComputeJob job = {0};
        pthread_mutex_lock(&app->route_worker_mutex);
        while (app->route_worker_running && !app->route_job_pending) {
            pthread_cond_wait(&app->route_worker_cond, &app->route_worker_mutex);
        }
        if (!app->route_worker_running) {
            pthread_mutex_unlock(&app->route_worker_mutex);
            break;
        }
        job = app->route_job;
        app->route_job_pending = false;
        app->route_worker_busy = true;
        pthread_mutex_unlock(&app->route_worker_mutex);

        RouteComputeResult result = {0};
        result.request_id = job.request_id;
        result.start_node = job.start_node;
        result.goal_node = job.goal_node;
        result.fastest = job.fastest;
        result.mode = job.mode;

        app->route_worker_state.fastest = job.fastest;
        app->route_worker_state.mode = job.mode;
        bool ok = route_state_route(&app->route_worker_state, job.start_node, job.goal_node);
        result.ok = ok;
        if (ok) {
            result.has_transfer = app->route_worker_state.has_transfer;
            result.transfer_node = app->route_worker_state.transfer_node;
            result.path = app->route_worker_state.path;
            result.drive_path = app->route_worker_state.drive_path;
            result.walk_path = app->route_worker_state.walk_path;
            memset(&app->route_worker_state.path, 0, sizeof(app->route_worker_state.path));
            memset(&app->route_worker_state.drive_path, 0, sizeof(app->route_worker_state.drive_path));
            memset(&app->route_worker_state.walk_path, 0, sizeof(app->route_worker_state.walk_path));
        }

        pthread_mutex_lock(&app->route_worker_mutex);
        app_route_result_clear(&app->route_result);
        app->route_result = result;
        app->route_result_pending = true;
        app->route_worker_busy = false;
        pthread_cond_broadcast(&app->route_worker_cond);
        pthread_mutex_unlock(&app->route_worker_mutex);
    }

    return NULL;
}

bool app_route_worker_init(AppState *app) {
    if (!app) {
        return false;
    }
    app->route_worker_enabled = false;
    app->route_worker_running = false;
    app->route_worker_busy = false;
    app->route_job_pending = false;
    app->route_result_pending = false;
    app->route_latest_requested_id = 0u;
    app->route_latest_submitted_id = 0u;
    app->route_latest_applied_id = 0u;
    app->route_recompute_scheduled = false;
    app->route_recompute_due_time = 0.0;
    memset(&app->route_job, 0, sizeof(app->route_job));
    memset(&app->route_result, 0, sizeof(app->route_result));
    route_state_init(&app->route_worker_state);
    if (pthread_mutex_init(&app->route_worker_mutex, NULL) != 0) {
        return false;
    }
    if (pthread_cond_init(&app->route_worker_cond, NULL) != 0) {
        pthread_mutex_destroy(&app->route_worker_mutex);
        return false;
    }
    app->route_worker_running = true;
    if (pthread_create(&app->route_worker_thread, NULL, app_route_worker_thread_main, app) != 0) {
        app->route_worker_running = false;
        pthread_cond_destroy(&app->route_worker_cond);
        pthread_mutex_destroy(&app->route_worker_mutex);
        route_state_shutdown(&app->route_worker_state);
        return false;
    }
    app->route_worker_enabled = true;
    return true;
}

void app_route_worker_shutdown(AppState *app) {
    if (!app || !app->route_worker_enabled) {
        return;
    }
    pthread_mutex_lock(&app->route_worker_mutex);
    app->route_worker_running = false;
    pthread_cond_broadcast(&app->route_worker_cond);
    pthread_mutex_unlock(&app->route_worker_mutex);
    pthread_join(app->route_worker_thread, NULL);
    pthread_mutex_lock(&app->route_worker_mutex);
    app_route_result_clear(&app->route_result);
    app->route_result_pending = false;
    app->route_job_pending = false;
    pthread_mutex_unlock(&app->route_worker_mutex);
    pthread_cond_destroy(&app->route_worker_cond);
    pthread_mutex_destroy(&app->route_worker_mutex);
    route_state_shutdown(&app->route_worker_state);
    app->route_worker_enabled = false;
}

void app_route_worker_clear(AppState *app) {
    if (!app || !app->route_worker_enabled) {
        return;
    }
    pthread_mutex_lock(&app->route_worker_mutex);
    while (app->route_worker_busy) {
        pthread_cond_wait(&app->route_worker_cond, &app->route_worker_mutex);
    }
    app->route_job_pending = false;
    app_route_result_clear(&app->route_result);
    app->route_result_pending = false;
    app->route_recompute_scheduled = false;
    app->route_latest_requested_id = 0u;
    app->route_latest_submitted_id = 0u;
    app->route_latest_applied_id = 0u;
    route_state_clear(&app->route_worker_state);
    pthread_mutex_unlock(&app->route_worker_mutex);
}

void app_route_schedule_recompute(AppState *app, double debounce_sec) {
    if (!app || !app->route_worker_enabled || !app->route.has_start || !app->route.has_goal) {
        return;
    }
    if (debounce_sec < 0.0) {
        debounce_sec = 0.0;
    }

    double now = time_now_seconds();
    pthread_mutex_lock(&app->route_worker_mutex);
    app->route_recompute_scheduled = true;
    app->route_recompute_due_time = now + debounce_sec;
    app->route_latest_requested_id += 1u;
    pthread_mutex_unlock(&app->route_worker_mutex);
}

void app_route_poll_result(AppState *app) {
    if (!app || !app->route_worker_enabled) {
        return;
    }

    RouteComputeResult result = {0};
    bool have_result = false;
    RouteComputeJob submit_job = {0};
    bool should_submit = false;
    double now = time_now_seconds();

    pthread_mutex_lock(&app->route_worker_mutex);
    if (app->route_result_pending) {
        result = app->route_result;
        memset(&app->route_result, 0, sizeof(app->route_result));
        app->route_result_pending = false;
        have_result = true;
    }
    if (app->route_recompute_scheduled && now >= app->route_recompute_due_time &&
        app->route.loaded && app->route.has_start && app->route.has_goal) {
        submit_job.request_id = app->route_latest_requested_id;
        submit_job.start_node = app->route.start_node;
        submit_job.goal_node = app->route.goal_node;
        submit_job.fastest = app->route.fastest;
        submit_job.mode = app->route.mode;
        app->route_job = submit_job;
        app->route_job_pending = true;
        app->route_recompute_scheduled = false;
        app->route_latest_submitted_id = submit_job.request_id;
        should_submit = true;
    }
    if (should_submit) {
        pthread_cond_signal(&app->route_worker_cond);
    }
    pthread_mutex_unlock(&app->route_worker_mutex);

    if (!have_result) {
        return;
    }
    if (result.request_id != app->route_latest_submitted_id || result.request_id < app->route_latest_applied_id) {
        app_route_result_clear(&result);
        return;
    }
    if (!result.ok) {
        app_route_result_clear(&result);
        return;
    }

    app_route_path_move(&app->route.path, &result.path);
    app_route_path_move(&app->route.drive_path, &result.drive_path);
    app_route_path_move(&app->route.walk_path, &result.walk_path);
    app->route.start_node = result.start_node;
    app->route.goal_node = result.goal_node;
    app->route.has_start = true;
    app->route.has_goal = true;
    app->route.fastest = result.fastest;
    app->route.mode = result.mode;
    app->route.has_transfer = result.has_transfer;
    app->route.transfer_node = result.transfer_node;
    app->route_latest_applied_id = result.request_id;
    app_playback_reset(app);
}
