#include "app/app_trace_runtime.h"

#include "app/app_internal.h"
#include "core/log.h"
#include "core/time.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

static const char *kTraceLaneLifecycle = "lifecycle";

// Creates a writable trace folder outside packaged resources, retaining source-run defaults.
static bool app_trace_ensure_dirs(char *directory, size_t capacity) {
    const char *runtime_dir = getenv("MAPFORGE_RUNTIME_DIR");
    const char *parent = runtime_dir && runtime_dir[0] ? runtime_dir : "build";
    int written = snprintf(directory, capacity, "%s/traces", parent);
    if (written < 0 || (size_t)written >= capacity) {
        return false;
    }
    if (mkdir(parent, 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir(directory, 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}

bool app_trace_session_start(struct AppState *app) {
    if (!app) {
        return false;
    }

    app->trace_enabled = false;
    CoreTraceConfig trace_cfg = {
        .sample_capacity = APP_TRACE_SAMPLE_CAPACITY,
        .marker_capacity = APP_TRACE_MARKER_CAPACITY
    };
    CoreResult trace_init = core_trace_session_init(&app->trace_session, &trace_cfg);
    if (trace_init.code != CORE_OK) {
        log_error("core_trace_session_init failed: %s", trace_init.message);
        return false;
    }

    app->lifetime.trace_session_initialized = true;
    TileLoaderStats trace_stats = {0};
    tile_loader_get_stats(&app->tile_state_bridge.tile_loader, &trace_stats);
    app->trace_enabled = true;
    app->trace_start_time = time_now_seconds();
    core_trace_emit_marker(&app->trace_session, kTraceLaneLifecycle, 0.0, "trace_start");
    app->trace_last_tile_enqueue_drop_count = trace_stats.enqueue_drop_count;
    app->trace_last_tile_enqueue_evict_count = trace_stats.enqueue_evict_count;
    app->trace_last_tile_result_drop_count = trace_stats.result_drop_count;
    app->trace_last_tile_result_evict_count = trace_stats.result_evict_count;
    app->trace_last_vk_asset_drop_count = app->worker_state_bridge.vk_asset_job_drop_count;
    app->trace_last_vk_asset_evict_count = app->worker_state_bridge.vk_asset_job_evict_count;
    app->trace_last_vk_asset_stage_drop_count = app->worker_state_bridge.vk_asset_stage_drop_count;
    app->trace_last_vk_asset_stage_evict_count = app->worker_state_bridge.vk_asset_stage_evict_count;
    return true;
}

void app_trace_emit_frame_samples(struct AppState *app, double rel_time_s) {
    if (!app || !app->trace_enabled) {
        return;
    }
    core_trace_emit_sample_f32(&app->trace_session, "frame", rel_time_s, (float)app->frame_timings.frame_ms);
    core_trace_emit_sample_f32(&app->trace_session, "events", rel_time_s, (float)app->frame_timings.events_ms);
    core_trace_emit_sample_f32(&app->trace_session, "update", rel_time_s, (float)app->frame_timings.update_ms);
    core_trace_emit_sample_f32(&app->trace_session, "queue", rel_time_s, (float)app->frame_timings.queue_ms);
    core_trace_emit_sample_f32(&app->trace_session, "integrate", rel_time_s, (float)app->frame_timings.integrate_ms);
    core_trace_emit_sample_f32(&app->trace_session, "route", rel_time_s, (float)app->frame_timings.route_ms);
    core_trace_emit_sample_f32(&app->trace_session, "render", rel_time_s, (float)app->frame_timings.render_ms);
    core_trace_emit_sample_f32(&app->trace_session, "present", rel_time_s, (float)app->frame_timings.present_ms);
}

void app_trace_emit_queue_markers(struct AppState *app, double rel_time_s) {
    if (!app || !app->trace_enabled) {
        return;
    }

    TileLoaderStats stats = {0};
    tile_loader_get_stats(&app->tile_state_bridge.tile_loader, &stats);
    if (stats.enqueue_drop_count > app->trace_last_tile_enqueue_drop_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "tile_enq_drop");
    }
    if (stats.enqueue_evict_count > app->trace_last_tile_enqueue_evict_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "tile_enq_evict");
    }
    if (stats.result_drop_count > app->trace_last_tile_result_drop_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "tile_res_drop");
    }
    if (stats.result_evict_count > app->trace_last_tile_result_evict_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "tile_res_evict");
    }
    if (app->worker_state_bridge.vk_asset_job_drop_count > app->trace_last_vk_asset_drop_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "vk_job_drop");
    }
    if (app->worker_state_bridge.vk_asset_job_evict_count > app->trace_last_vk_asset_evict_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "vk_job_evict");
    }
    if (app->worker_state_bridge.vk_asset_stage_drop_count > app->trace_last_vk_asset_stage_drop_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "vk_stage_drop");
    }
    if (app->worker_state_bridge.vk_asset_stage_evict_count > app->trace_last_vk_asset_stage_evict_count) {
        core_trace_emit_marker(&app->trace_session, "queue", rel_time_s, "vk_stage_evict");
    }

    app->trace_last_tile_enqueue_drop_count = stats.enqueue_drop_count;
    app->trace_last_tile_enqueue_evict_count = stats.enqueue_evict_count;
    app->trace_last_tile_result_drop_count = stats.result_drop_count;
    app->trace_last_tile_result_evict_count = stats.result_evict_count;
    app->trace_last_vk_asset_drop_count = app->worker_state_bridge.vk_asset_job_drop_count;
    app->trace_last_vk_asset_evict_count = app->worker_state_bridge.vk_asset_job_evict_count;
    app->trace_last_vk_asset_stage_drop_count = app->worker_state_bridge.vk_asset_stage_drop_count;
    app->trace_last_vk_asset_stage_evict_count = app->worker_state_bridge.vk_asset_stage_evict_count;
}

void app_trace_shutdown(struct AppState *app) {
    if (!app || !app->trace_enabled) {
        return;
    }

    {
        double rel_time_s = time_now_seconds() - app->trace_start_time;
        if (rel_time_s < 0.0) {
            rel_time_s = 0.0;
        }
        core_trace_emit_marker(&app->trace_session, kTraceLaneLifecycle, rel_time_s, "trace_end");
    }

    char trace_directory[4096];
    CoreResult final_result = core_trace_finalize(&app->trace_session);
    if (final_result.code != CORE_OK) {
        log_error("core_trace_finalize failed: %s", final_result.message);
    } else if (!app_trace_ensure_dirs(trace_directory, sizeof(trace_directory))) {
        log_error("failed to create writable trace directory");
    } else {
        time_t now = time(NULL);
        struct tm local_tm = {0};
        localtime_r(&now, &local_tm);
        char filename[96];
        char path[4096];
        size_t named = strftime(filename, sizeof(filename), "mapforge_trace_%Y%m%d_%H%M%S.pack", &local_tm);
        int written = snprintf(path, sizeof(path), "%s/%s", trace_directory, filename);
        if (named == 0 || written < 0 || (size_t)written >= sizeof(path)) {
            log_error("trace export path exceeds capacity");
            core_trace_session_reset(&app->trace_session);
            app->trace_enabled = false;
            return;
        }
        CoreResult export_result = core_trace_export_pack(&app->trace_session, path);
        if (export_result.code != CORE_OK) {
            log_error("core_trace_export_pack failed: %s", export_result.message);
        } else {
            log_info("trace exported: %s", path);
        }
    }

    core_trace_session_reset(&app->trace_session);
    app->trace_enabled = false;
}
