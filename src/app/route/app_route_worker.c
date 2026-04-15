#include "app/app_route_internal.h"

#include "core/log.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

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
