#include "app/app_route_internal.h"
#include "route/cost.h"

#include "core/log.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

void *app_route_worker_thread_main(void *userdata) {
    AppState *app = (AppState *)userdata;
    if (!app) {
        return NULL;
    }

    for (;;) {
        RouteComputeJob job = {0};
        bool have_graph_job = false;
        uint32_t graph_job_request_id = 0u;
        char graph_job_path[MAPFORGE_REGION_PATH_CAPACITY] = {0};
        pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
        while (app->worker_state_bridge.route_worker_running &&
               !app->worker_state_bridge.route_job_pending &&
               !app->worker_state_bridge.route_graph_job_pending) {
            pthread_cond_wait(&app->worker_state_bridge.route_worker_cond, &app->worker_state_bridge.route_worker_mutex);
        }
        if (!app->worker_state_bridge.route_worker_running) {
            pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);
            break;
        }
        if (app->worker_state_bridge.route_graph_job_pending) {
            have_graph_job = true;
            graph_job_request_id = app->worker_state_bridge.route_graph_job_request_id;
            snprintf(graph_job_path,
                     sizeof(graph_job_path),
                     "%s",
                     app->worker_state_bridge.route_graph_job_path);
            app->worker_state_bridge.route_graph_job_pending = false;
        } else {
            job = app->worker_state_bridge.route_job;
            app->worker_state_bridge.route_job_pending = false;
        }
        app->worker_state_bridge.route_worker_busy = true;
        pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);

        if (have_graph_job) {
            RouteState loaded_main_state;
            RouteSnapIndex loaded_snap_index = {0};
            bool ok = false;

            route_state_init(&loaded_main_state);
            loaded_main_state.objective = app->worker_state_bridge.route_worker_state.objective;
            loaded_main_state.mode = app->worker_state_bridge.route_worker_state.mode;
            ok = route_state_load_graph(&loaded_main_state, graph_job_path);
            if (!ok) {
                log_error("Missing route graph for region path: %s", graph_job_path);
            } else if (!app_route_snap_index_build(&loaded_main_state.graph, &loaded_snap_index)) {
                log_error("Failed to build route snap index for graph path: %s", graph_job_path);
            }
            if (!route_state_load_graph(&app->worker_state_bridge.route_worker_state, graph_job_path)) {
                log_error("Worker route graph load failed for path: %s", graph_job_path);
                ok = false;
            }

            pthread_mutex_lock(&app->worker_state_bridge.route_worker_mutex);
            app->worker_state_bridge.route_job_pending = false;
            app_route_result_clear(&app->worker_state_bridge.route_result);
            app->worker_state_bridge.route_result_pending = false;

            if (app->worker_state_bridge.route_graph_result_pending) {
                route_state_shutdown(&app->worker_state_bridge.route_graph_result_state);
                route_state_init(&app->worker_state_bridge.route_graph_result_state);
                app_route_snap_index_free(&app->worker_state_bridge.route_graph_result_snap_index);
            }
            app->worker_state_bridge.route_graph_result_pending = true;
            app->worker_state_bridge.route_graph_result_ok = ok;
            app->worker_state_bridge.route_graph_result_request_id = graph_job_request_id;
            if (ok) {
                app->worker_state_bridge.route_graph_result_state = loaded_main_state;
                memset(&loaded_main_state, 0, sizeof(loaded_main_state));
                app->worker_state_bridge.route_graph_result_snap_index = loaded_snap_index;
                memset(&loaded_snap_index, 0, sizeof(loaded_snap_index));
            }
            app->worker_state_bridge.route_worker_busy = false;
            pthread_cond_broadcast(&app->worker_state_bridge.route_worker_cond);
            pthread_mutex_unlock(&app->worker_state_bridge.route_worker_mutex);

            route_state_shutdown(&loaded_main_state);
            app_route_snap_index_free(&loaded_snap_index);
            continue;
        }

        RouteComputeResult result = {0};
        result.request_id = job.request_id;
        result.start_node = job.start_node;
        result.goal_node = job.goal_node;
        result.objective = job.objective;
        result.mode = job.mode;

        app->worker_state_bridge.route_worker_state.objective = job.objective;
        app->worker_state_bridge.route_worker_state.mode = job.mode;
        bool ok = route_query_solve(&app->worker_state_bridge.route_worker_state, &job.start_anchor, &job.goal_anchor);
        if (ok) { result.start_node=app->worker_state_bridge.route_worker_state.start_node; result.goal_node=app->worker_state_bridge.route_worker_state.goal_node; }
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
