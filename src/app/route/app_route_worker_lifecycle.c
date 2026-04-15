#include "app/app_internal.h"
#include "app/app_route_internal.h"

#include "core/time.h"

#include <pthread.h>
#include <string.h>

bool app_route_worker_init(AppState *app) {
    if (!app) {
        return false;
    }
    app->worker_state_bridge.route_worker_enabled = false;
    app->worker_state_bridge.route_worker_running = false;
    app->worker_state_bridge.route_worker_busy = false;
    app->worker_state_bridge.route_job_pending = false;
    app->worker_state_bridge.route_result_pending = false;
    app->worker_state_bridge.route_graph_job_pending = false;
    app->worker_state_bridge.route_graph_job_request_id = 0u;
    app->worker_state_bridge.route_graph_job_path[0] = '\0';
    app->worker_state_bridge.route_graph_result_pending = false;
    app->worker_state_bridge.route_graph_result_ok = false;
    app->worker_state_bridge.route_graph_result_request_id = 0u;
    app_worker_contract_reset_route_pipeline(app);
    app->route_state_bridge.route_recompute_scheduled = false;
    app->route_state_bridge.route_recompute_due_time = 0.0;
    app->route_state_bridge.route_graph_loading = false;
    app->route_state_bridge.route_graph_load_request_id = 0u;
    memset(&app->worker_state_bridge.route_job, 0, sizeof(app->worker_state_bridge.route_job));
    memset(&app->worker_state_bridge.route_result, 0, sizeof(app->worker_state_bridge.route_result));
    route_state_init(&app->worker_state_bridge.route_worker_state);
    route_state_init(&app->worker_state_bridge.route_graph_result_state);
    memset(&app->worker_state_bridge.route_graph_result_snap_index, 0, sizeof(app->worker_state_bridge.route_graph_result_snap_index));
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
        route_state_shutdown(&app->worker_state_bridge.route_graph_result_state);
        app_route_snap_index_free(&app->worker_state_bridge.route_graph_result_snap_index);
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
    app->worker_state_bridge.route_graph_job_pending = false;
    app->worker_state_bridge.route_graph_job_request_id = 0u;
    app->worker_state_bridge.route_graph_job_path[0] = '\0';
    app->worker_state_bridge.route_graph_result_pending = false;
    app->worker_state_bridge.route_graph_result_ok = false;
    app->worker_state_bridge.route_graph_result_request_id = 0u;
    route_state_shutdown(&app->worker_state_bridge.route_graph_result_state);
    route_state_init(&app->worker_state_bridge.route_graph_result_state);
    app_route_snap_index_free(&app->worker_state_bridge.route_graph_result_snap_index);
    pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);
    pthread_cond_destroy(&app->worker_state_bridge.route_worker_cond);
    pthread_mutex_destroy(&app->worker_state_bridge.route_worker_mutex);
    route_state_shutdown(&app->worker_state_bridge.route_worker_state);
    route_state_shutdown(&app->worker_state_bridge.route_graph_result_state);
    app_route_snap_index_free(&app->worker_state_bridge.route_graph_result_snap_index);
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
    app->worker_state_bridge.route_graph_job_pending = false;
    app->worker_state_bridge.route_graph_job_request_id = 0u;
    app->worker_state_bridge.route_graph_job_path[0] = '\0';
    if (app->worker_state_bridge.route_graph_result_pending) {
        route_state_shutdown(&app->worker_state_bridge.route_graph_result_state);
        route_state_init(&app->worker_state_bridge.route_graph_result_state);
        app_route_snap_index_free(&app->worker_state_bridge.route_graph_result_snap_index);
        app->worker_state_bridge.route_graph_result_pending = false;
    }
    app->worker_state_bridge.route_graph_result_ok = false;
    app->worker_state_bridge.route_graph_result_request_id = 0u;
    app->route_state_bridge.route_recompute_scheduled = false;
    app->route_state_bridge.route_graph_loading = false;
    app_worker_contract_reset_route_pipeline(app);
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
    app_worker_contract_next_route_request(app);
    pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);
}
