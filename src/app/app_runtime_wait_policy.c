#include "app/app_internal.h"
#include "app/app_utility_workspace.h"

#include <math.h>
#include <stdlib.h>

enum {
    APP_RUNTIME_WAIT_IDLE_DEFAULT_MS = 1000,
    APP_RUNTIME_WAIT_BACKGROUND_BUSY_MS = 8,
    APP_RUNTIME_WAIT_IMPORT_POLL_MS = 50,
    APP_RUNTIME_WAIT_MIN_MS = 1,
    APP_RUNTIME_WAIT_MAX_MS = 5000
};

static float app_runtime_normalize_angle(float angle_rad) {
    const float tau = 6.28318530717958647693f;
    if (!isfinite(angle_rad)) {
        return 0.0f;
    }

    angle_rad = fmodf(angle_rad, tau);
    if (angle_rad <= -3.14159265358979323846f) {
        angle_rad += tau;
    } else if (angle_rad > 3.14159265358979323846f) {
        angle_rad -= tau;
    }
    return angle_rad;
}

static bool app_runtime_camera_has_settling_motion(const Camera *camera) {
    if (!camera) {
        return false;
    }
    return fabsf(camera->x_target - camera->x) > 0.5f ||
           fabsf(camera->y_target - camera->y) > 0.5f ||
           fabsf(camera->zoom_target - camera->zoom) > 0.01f ||
           fabsf(app_runtime_normalize_angle(camera->heading_target_rad - camera->heading_rad)) > 0.001f;
}

static bool app_runtime_input_has_continuous_activity(const InputState *input) {
    if (!input) {
        return false;
    }
    return input->mouse_buttons != 0u ||
           input->pan_left ||
           input->pan_right ||
           input->pan_up ||
           input->pan_down;
}

static int app_runtime_env_wait_override_ms(void) {
    const char *wait_env = getenv("MAPFORGE_LOOP_MAX_WAIT_MS");
    if (!wait_env || wait_env[0] == '\0') {
        return -1;
    }
    char *end = NULL;
    long parsed = strtol(wait_env, &end, 10);
    if (end == wait_env || parsed < APP_RUNTIME_WAIT_MIN_MS || parsed > APP_RUNTIME_WAIT_MAX_MS) {
        return -1;
    }
    return (int)parsed;
}

static bool app_runtime_route_worker_has_pending(const AppState *app) {
    if (!app || !app->worker_state_bridge.route_worker_enabled) {
        return false;
    }
    bool pending = false;
    pthread_mutex_lock((pthread_mutex_t *)&app->worker_state_bridge.route_worker_mutex);
    pending = app->worker_state_bridge.route_worker_busy ||
              app->worker_state_bridge.route_job_pending ||
              app->worker_state_bridge.route_graph_job_pending ||
              app->worker_state_bridge.route_result_pending ||
              app->worker_state_bridge.route_graph_result_pending;
    pthread_mutex_unlock((pthread_mutex_t *)&app->worker_state_bridge.route_worker_mutex);
    return pending;
}

static bool app_runtime_tile_loader_has_pending(const AppState *app) {
    if (!app || !app->lifetime.tile_loader_initialized) {
        return false;
    }
    for (unsigned k = 0; k < TILE_LAYER_COUNT; ++k) {
        if (app->tile_state_bridge.layer_inflight[k] > 0) return true;
    }
    TileLoaderStats stats = {0};
    tile_loader_get_stats((TileLoader *)&app->tile_state_bridge.tile_loader, &stats);
    return stats.req_count > 0u || stats.res_count > 0u;
}

bool app_runtime_has_immediate_work(const AppState *app, double now_sec) {
    if (!app) {
        return true;
    }
    if(app->terrain_pending || (app->history_pending && app->ui_state_bridge.utility_workspace == APP_UTILITY_HIKING_HISTORY)) return true;
    if (app->ui_state_bridge.input.quit) {
        return true;
    }
    /* An outside-pack view is settled too; validity is not a work queue. */
    if (!app->runtime_frame_presented || app->runtime_redraw_requested) {
        return true;
    }
    if (app_runtime_input_has_continuous_activity(&app->ui_state_bridge.input)) {
        return true;
    }
    if (app_runtime_camera_has_settling_motion(&app->view_state_bridge.camera)) {
        return true;
    }
    if (app->viewport_scenario_active && !app->viewport_scenario_completed) {
        return true;
    }
    if (app->route_state_bridge.playback_playing) {
        return true;
    }
    if (app->route_state_bridge.dragging_start || app->route_state_bridge.dragging_goal) {
        return true;
    }
    if (app->route_state_bridge.route_recompute_scheduled &&
        now_sec >= app->route_state_bridge.route_recompute_due_time) {
        return true;
    }
    return false;
}

// Keeps finite asynchronous queues serviceable even when there are no native input events.
bool app_runtime_has_background_work(const AppState *app) {
    if (!app) return false;
    if (app->region_prepare || app_runtime_route_worker_has_pending(app) ||
        app_runtime_tile_loader_has_pending(app)) return true;
    for (unsigned k = 0; k < TILE_LAYER_COUNT; ++k) {
        const TileQueue *queue = &app->tile_state_bridge.tile_queues[k];
        if (app_layer_active_runtime(app, (TileLayerKind)k) &&
            (queue->index < queue->count || app->tile_state_bridge.coverage_gate_pending[k])) return true;
    }
    if (app->worker_state_bridge.vk_asset_job_count > 0u) return true;
    if (app->lifetime.vk_asset_worker_initialized) {
        pthread_mutex_lock((pthread_mutex_t *)&app->worker_state_bridge.vk_asset_worker_mutex);
        bool pending = app->worker_state_bridge.vk_asset_stage_count > 0u ||
            core_queue_mutex_size((CoreQueueMutex *)&app->worker_state_bridge.vk_asset_ready_queue) > 0u;
        pthread_mutex_unlock((pthread_mutex_t *)&app->worker_state_bridge.vk_asset_worker_mutex);
        if (pending) return true;
    }
    if (app->lifetime.vk_poly_prep_initialized) {
        VkPolyPrepStats stats = {0};
        app_vk_poly_prep_get_stats((AppState *)app, &stats);
        if (stats.busy || stats.in_count > 0u || stats.out_count > 0u) return true;
    }
    return false;
}

// Retains the current presentation on maintenance-only idle wakes.
bool app_runtime_frame_needed(const AppState *app, const AppRuntimeInputFrame *input, double now_sec) {
    return app_runtime_has_immediate_work(app, now_sec) ||
        (input && input->raw.sdl_event_count > 0u) ||
        app_runtime_has_background_work(app) || (app && app->ingest_import_running);
}

int app_runtime_compute_wait_timeout_ms(const AppState *app, double now_sec) {
    if (app_runtime_has_immediate_work(app, now_sec)) {
        return 0;
    }

    int timeout_ms = APP_RUNTIME_WAIT_IDLE_DEFAULT_MS;
    if (app_runtime_has_background_work(app)) {
        timeout_ms = APP_RUNTIME_WAIT_BACKGROUND_BUSY_MS;
    }
    if (app && app->ingest_import_running && timeout_ms > APP_RUNTIME_WAIT_IMPORT_POLL_MS) {
        timeout_ms = APP_RUNTIME_WAIT_IMPORT_POLL_MS;
    }

    if (app && app->route_state_bridge.route_recompute_scheduled) {
        double until_due_sec = app->route_state_bridge.route_recompute_due_time - now_sec;
        if (until_due_sec <= 0.0) {
            return 0;
        }
        int due_ms = (int)ceil(until_due_sec * 1000.0);
        if (due_ms < APP_RUNTIME_WAIT_MIN_MS) {
            due_ms = APP_RUNTIME_WAIT_MIN_MS;
        }
        if (due_ms < timeout_ms) {
            timeout_ms = due_ms;
        }
    }

    int env_override = app_runtime_env_wait_override_ms();
    if (env_override > 0 && timeout_ms > env_override) {
        timeout_ms = env_override;
    }

    if (timeout_ms < APP_RUNTIME_WAIT_MIN_MS) {
        timeout_ms = APP_RUNTIME_WAIT_MIN_MS;
    }
    if (timeout_ms > APP_RUNTIME_WAIT_MAX_MS) {
        timeout_ms = APP_RUNTIME_WAIT_MAX_MS;
    }
    return timeout_ms;
}
