#include "app/app_internal.h"

#include "core/time.h"
#include "map/polygon_cache.h"

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

static bool app_kind_is_polygon(TileLayerKind kind) {
    return kind == TILE_LAYER_POLY_WATER ||
           kind == TILE_LAYER_POLY_PARK ||
           kind == TILE_LAYER_POLY_LANDUSE ||
           kind == TILE_LAYER_POLY_BUILDING;
}

static uint32_t app_vk_asset_visible_ring_distance(const AppState *app, TileCoord coord) {
    if (!app || !app->tile_state_bridge.visible_valid || coord.z != app->tile_state_bridge.visible_zoom) {
        return UINT32_MAX / 4u;
    }

    uint32_t dx = 0u;
    uint32_t dy = 0u;
    if (coord.x < app->tile_state_bridge.visible_top_left.x) {
        dx = app->tile_state_bridge.visible_top_left.x - coord.x;
    } else if (coord.x > app->tile_state_bridge.visible_bottom_right.x) {
        dx = coord.x - app->tile_state_bridge.visible_bottom_right.x;
    }
    if (coord.y < app->tile_state_bridge.visible_top_left.y) {
        dy = app->tile_state_bridge.visible_top_left.y - coord.y;
    } else if (coord.y > app->tile_state_bridge.visible_bottom_right.y) {
        dy = coord.y - app->tile_state_bridge.visible_bottom_right.y;
    }
    return dx + dy;
}

static uint32_t app_vk_asset_ring_bucket(uint32_t ring_distance) {
    if (ring_distance == 0u) {
        return 0u;
    }
    if (ring_distance <= 1u) {
        return 1u;
    }
    if (ring_distance <= 2u) {
        return 2u;
    }
    if (ring_distance <= 4u) {
        return 3u;
    }
    return 4u;
}

static uint32_t app_vk_asset_kind_priority(TileLayerKind kind) {
    if (kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL) {
        return 0u;
    }
    if (kind == TILE_LAYER_POLY_WATER ||
        kind == TILE_LAYER_POLY_PARK ||
        kind == TILE_LAYER_POLY_LANDUSE) {
        return 1u;
    }
    if (kind == TILE_LAYER_POLY_BUILDING) {
        return 2u;
    }
    return 3u;
}

static bool app_vk_asset_pop_job_at(AppState *app, uint32_t offset, VkAssetJob *out_job) {
    if (!app || !out_job || offset >= app->worker_state_bridge.vk_asset_job_count) {
        return false;
    }

    uint32_t cap = APP_VK_ASSET_QUEUE_CAPACITY;
    uint32_t head = app->worker_state_bridge.vk_asset_job_head;
    uint32_t idx = (head + offset) % cap;
    *out_job = app->worker_state_bridge.vk_asset_jobs[idx];

    for (uint32_t i = offset; i + 1u < app->worker_state_bridge.vk_asset_job_count; ++i) {
        uint32_t from = (head + i + 1u) % cap;
        uint32_t to = (head + i) % cap;
        app->worker_state_bridge.vk_asset_jobs[to] = app->worker_state_bridge.vk_asset_jobs[from];
    }

    if (app->worker_state_bridge.vk_asset_job_count > 0u) {
        app->worker_state_bridge.vk_asset_job_tail = (app->worker_state_bridge.vk_asset_job_tail + cap - 1u) % cap;
        app->worker_state_bridge.vk_asset_job_count -= 1u;
    }
    return true;
}

static bool app_vk_asset_main_has_duplicate(const AppState *app,
                                            TileLayerKind kind,
                                            TileCoord coord,
                                            TileZoomBand band,
                                            uint32_t request_id) {
    if (!app) {
        return false;
    }
    for (uint32_t i = 0u; i < app->worker_state_bridge.vk_asset_job_count; ++i) {
        uint32_t idx = (app->worker_state_bridge.vk_asset_job_head + i) % APP_VK_ASSET_QUEUE_CAPACITY;
        const VkAssetJob *job = &app->worker_state_bridge.vk_asset_jobs[idx];
        if (job->kind == kind &&
            job->band == band &&
            job->coord.z == coord.z &&
            job->coord.x == coord.x &&
            job->coord.y == coord.y &&
            job->request_id == request_id) {
            return true;
        }
    }
    return false;
}

static bool app_vk_asset_main_admit_job(AppState *app, const VkAssetJob *in_job) {
    if (!app || !in_job) {
        return false;
    }
    if (app_vk_asset_main_has_duplicate(app, in_job->kind, in_job->coord, in_job->band, in_job->request_id)) {
        return true;
    }

    if (app->worker_state_bridge.vk_asset_job_count >= APP_VK_ASSET_QUEUE_CAPACITY) {
        uint32_t request_ids[APP_VK_ASSET_QUEUE_CAPACITY];
        for (uint32_t i = 0u; i < app->worker_state_bridge.vk_asset_job_count; ++i) {
            uint32_t idx = (app->worker_state_bridge.vk_asset_job_head + i) % APP_VK_ASSET_QUEUE_CAPACITY;
            request_ids[i] = app->worker_state_bridge.vk_asset_jobs[idx].request_id;
        }
        uint32_t evict_offset = 0u;
        if (app_worker_contract_choose_evict_offset(request_ids,
                                                    app->worker_state_bridge.vk_asset_job_count,
                                                    app->worker_state_bridge.tile_generation,
                                                    &evict_offset)) {
            VkAssetJob dropped = {0};
            if (app_vk_asset_pop_job_at(app, evict_offset, &dropped)) {
                app->worker_state_bridge.vk_asset_job_evict_count += 1u;
            }
        }
    }

    if (app->worker_state_bridge.vk_asset_job_count >= APP_VK_ASSET_QUEUE_CAPACITY) {
        app->worker_state_bridge.vk_asset_job_drop_count += 1u;
        return false;
    }

    app->worker_state_bridge.vk_asset_jobs[app->worker_state_bridge.vk_asset_job_tail] = *in_job;
    app->worker_state_bridge.vk_asset_job_tail = (app->worker_state_bridge.vk_asset_job_tail + 1u) % APP_VK_ASSET_QUEUE_CAPACITY;
    app->worker_state_bridge.vk_asset_job_count += 1u;
    return true;
}

static bool app_vk_asset_stage_push(AppState *app, const VkAssetJob *job) {
    if (!app || !job || app->worker_state_bridge.vk_asset_stage_count >= APP_VK_ASSET_QUEUE_CAPACITY) {
        return false;
    }
    app->worker_state_bridge.vk_asset_stage_jobs[app->worker_state_bridge.vk_asset_stage_tail] = *job;
    app->worker_state_bridge.vk_asset_stage_tail = (app->worker_state_bridge.vk_asset_stage_tail + 1u) % APP_VK_ASSET_QUEUE_CAPACITY;
    app->worker_state_bridge.vk_asset_stage_count += 1u;
    return true;
}

static bool app_vk_asset_stage_pop(AppState *app, VkAssetJob *out_job) {
    if (!app || !out_job || app->worker_state_bridge.vk_asset_stage_count == 0u) {
        return false;
    }
    *out_job = app->worker_state_bridge.vk_asset_stage_jobs[app->worker_state_bridge.vk_asset_stage_head];
    memset(&app->worker_state_bridge.vk_asset_stage_jobs[app->worker_state_bridge.vk_asset_stage_head], 0, sizeof(app->worker_state_bridge.vk_asset_stage_jobs[app->worker_state_bridge.vk_asset_stage_head]));
    app->worker_state_bridge.vk_asset_stage_head = (app->worker_state_bridge.vk_asset_stage_head + 1u) % APP_VK_ASSET_QUEUE_CAPACITY;
    app->worker_state_bridge.vk_asset_stage_count -= 1u;
    return true;
}

static bool app_vk_asset_stage_pop_at(AppState *app, uint32_t offset, VkAssetJob *out_job) {
    if (!app || offset >= app->worker_state_bridge.vk_asset_stage_count) {
        return false;
    }
    uint32_t cap = APP_VK_ASSET_QUEUE_CAPACITY;
    uint32_t head = app->worker_state_bridge.vk_asset_stage_head;
    uint32_t idx = (head + offset) % cap;
    if (out_job) {
        *out_job = app->worker_state_bridge.vk_asset_stage_jobs[idx];
    }
    for (uint32_t i = offset; i + 1u < app->worker_state_bridge.vk_asset_stage_count; ++i) {
        uint32_t from = (head + i + 1u) % cap;
        uint32_t to = (head + i) % cap;
        app->worker_state_bridge.vk_asset_stage_jobs[to] = app->worker_state_bridge.vk_asset_stage_jobs[from];
    }
    app->worker_state_bridge.vk_asset_stage_tail = (app->worker_state_bridge.vk_asset_stage_tail + cap - 1u) % cap;
    app->worker_state_bridge.vk_asset_stage_count -= 1u;
    return true;
}

static bool app_vk_asset_ready_push(AppState *app, const VkAssetReadyJob *job) {
    if (!app || !job) {
        return false;
    }
    uint32_t slot = app->worker_state_bridge.vk_asset_ready_write_seq % APP_VK_ASSET_READY_QUEUE_CAPACITY;
    app->worker_state_bridge.vk_asset_ready_jobs[slot] = *job;
    void *token = (void *)(uintptr_t)(slot + 1u);
    if (!core_queue_mutex_push(&app->worker_state_bridge.vk_asset_ready_queue, token)) {
        return false;
    }
    app->worker_state_bridge.vk_asset_ready_write_seq += 1u;
    return true;
}

static bool app_vk_asset_ready_pop(AppState *app, VkAssetReadyJob *out_job) {
    if (!app || !out_job) {
        return false;
    }
    void *token = NULL;
    if (!core_queue_mutex_pop(&app->worker_state_bridge.vk_asset_ready_queue, &token)) {
        return false;
    }
    uintptr_t encoded = (uintptr_t)token;
    if (encoded == 0u || encoded > APP_VK_ASSET_READY_QUEUE_CAPACITY) {
        return false;
    }
    uint32_t slot = (uint32_t)(encoded - 1u);
    *out_job = app->worker_state_bridge.vk_asset_ready_jobs[slot];
    return true;
}

static void *app_vk_asset_worker_thread_main(void *userdata) {
    AppState *app = (AppState *)userdata;
    if (!app) {
        return NULL;
    }

    for (;;) {
        VkAssetJob stage_job = {0};
        pthread_mutex_lock(&app->worker_state_bridge.vk_asset_worker_mutex);
        while (app->worker_state_bridge.vk_asset_worker_running && app->worker_state_bridge.vk_asset_stage_count == 0u) {
            pthread_cond_wait(&app->worker_state_bridge.vk_asset_worker_cond, &app->worker_state_bridge.vk_asset_worker_mutex);
        }
        if (!app->worker_state_bridge.vk_asset_worker_running) {
            pthread_mutex_unlock(&app->worker_state_bridge.vk_asset_worker_mutex);
            break;
        }
        if (!app_vk_asset_stage_pop(app, &stage_job)) {
            pthread_mutex_unlock(&app->worker_state_bridge.vk_asset_worker_mutex);
            continue;
        }

        VkAssetReadyJob ready = {
            .coord = stage_job.coord,
            .kind = stage_job.kind,
            .band = stage_job.band,
            .request_id = stage_job.request_id
        };
        bool pushed = app_vk_asset_ready_push(app, &ready);
        if (!pushed && core_queue_mutex_size(&app->worker_state_bridge.vk_asset_ready_queue) > 0u) {
            VkAssetReadyJob dropped = {0};
            app_vk_asset_ready_pop(app, &dropped);
            app->worker_state_bridge.vk_asset_stage_evict_count += 1u;
            pushed = app_vk_asset_ready_push(app, &ready);
        }
        if (pushed) {
            app->worker_state_bridge.vk_asset_stage_prepared_count += 1u;
        } else {
            app->worker_state_bridge.vk_asset_stage_drop_count += 1u;
        }
        pthread_mutex_unlock(&app->worker_state_bridge.vk_asset_worker_mutex);
    }

    return NULL;
}

static bool app_vk_poly_prep_queue_push(CoreQueueMutex *queue,
                                        TileResult *storage,
                                        uint32_t *write_seq,
                                        const TileResult *item) {
    if (!queue || !storage || !write_seq || !item) {
        return false;
    }
    uint32_t slot = *write_seq % APP_VK_POLY_PREP_QUEUE_CAPACITY;
    storage[slot] = *item;
    void *token = (void *)(uintptr_t)(slot + 1u);
    if (!core_queue_mutex_push(queue, token)) {
        return false;
    }
    *write_seq += 1u;
    return true;
}

static bool app_vk_poly_prep_queue_pop(CoreQueueMutex *queue,
                                       TileResult *storage,
                                       TileResult *out_item) {
    if (!queue || !storage || !out_item) {
        return false;
    }
    void *token = NULL;
    if (!core_queue_mutex_pop(queue, &token)) {
        return false;
    }
    uintptr_t encoded = (uintptr_t)token;
    if (encoded == 0u || encoded > APP_VK_POLY_PREP_QUEUE_CAPACITY) {
        return false;
    }
    uint32_t slot = (uint32_t)(encoded - 1u);
    *out_item = storage[slot];
    memset(&storage[slot], 0, sizeof(storage[slot]));
    return true;
}

static void *app_vk_poly_prep_thread_main(void *userdata) {
    AppState *app = (AppState *)userdata;
    if (!app) {
        return NULL;
    }

    for (;;) {
        TileResult job = {0};

        pthread_mutex_lock(&app->worker_state_bridge.vk_poly_prep_mutex);
        while (app->worker_state_bridge.vk_poly_prep_running && core_queue_mutex_size(&app->worker_state_bridge.vk_poly_prep_in_queue) == 0u) {
            pthread_cond_wait(&app->worker_state_bridge.vk_poly_prep_cond, &app->worker_state_bridge.vk_poly_prep_mutex);
        }
        if (!app->worker_state_bridge.vk_poly_prep_running) {
            pthread_mutex_unlock(&app->worker_state_bridge.vk_poly_prep_mutex);
            break;
        }
        bool has_job = app_vk_poly_prep_queue_pop(
            &app->worker_state_bridge.vk_poly_prep_in_queue,
            app->worker_state_bridge.vk_poly_prep_in_jobs,
            &job);
        pthread_mutex_unlock(&app->worker_state_bridge.vk_poly_prep_mutex);
        if (!has_job) {
            continue;
        }

        PolygonCacheBuildStats poly_stats = {0};
        if (job.ok && app_kind_is_polygon(job.kind)) {
            polygon_cache_build_with_stats(&job.tile, &poly_stats);
        }

        pthread_mutex_lock(&app->worker_state_bridge.vk_poly_prep_mutex);
        if (poly_stats.polygon_quarantined > 0u || poly_stats.ring_quarantine_total > 0u) {
            app->worker_state_bridge.vk_poly_prep_quarantine_job_count += 1u;
        }
        app->worker_state_bridge.vk_poly_prep_quarantine_polygon_count += poly_stats.polygon_quarantined;
        app->worker_state_bridge.vk_poly_prep_quarantine_ring_bounds_count += poly_stats.ring_bounds_quarantined;
        app->worker_state_bridge.vk_poly_prep_quarantine_ring_min_points_count += poly_stats.ring_min_points_quarantined;
        app->worker_state_bridge.vk_poly_prep_quarantine_ring_degenerate_count += poly_stats.ring_degenerate_quarantined;
        app->worker_state_bridge.vk_poly_prep_winding_normalized_count += poly_stats.ring_winding_normalized;
        bool pushed = app_vk_poly_prep_queue_push(
            &app->worker_state_bridge.vk_poly_prep_out_queue,
            app->worker_state_bridge.vk_poly_prep_out_jobs,
            &app->worker_state_bridge.vk_poly_prep_out_write_seq,
            &job);
        if (pushed) {
            app->worker_state_bridge.vk_poly_prep_done_count += 1u;
        } else {
            app->worker_state_bridge.vk_poly_prep_drop_count += 1u;
        }
        pthread_mutex_unlock(&app->worker_state_bridge.vk_poly_prep_mutex);
        if (!pushed && job.ok) {
            mft_free_tile(&job.tile);
        }
    }

    return NULL;
}

bool app_vk_poly_prep_init(AppState *app) {
    if (!app) {
        return false;
    }
    app->worker_state_bridge.vk_poly_prep_enabled = false;
    app->worker_state_bridge.vk_poly_prep_running = false;
    app->worker_state_bridge.vk_poly_prep_in_write_seq = 0u;
    app->worker_state_bridge.vk_poly_prep_out_write_seq = 0u;
    app->worker_state_bridge.vk_poly_prep_enqueued_count = 0u;
    app->worker_state_bridge.vk_poly_prep_done_count = 0u;
    app->worker_state_bridge.vk_poly_prep_drop_count = 0u;
    app->worker_state_bridge.vk_poly_prep_quarantine_job_count = 0u;
    app->worker_state_bridge.vk_poly_prep_quarantine_polygon_count = 0u;
    app->worker_state_bridge.vk_poly_prep_quarantine_ring_bounds_count = 0u;
    app->worker_state_bridge.vk_poly_prep_quarantine_ring_min_points_count = 0u;
    app->worker_state_bridge.vk_poly_prep_quarantine_ring_degenerate_count = 0u;
    app->worker_state_bridge.vk_poly_prep_winding_normalized_count = 0u;
    if (!core_queue_mutex_init(&app->worker_state_bridge.vk_poly_prep_in_queue,
                               app->worker_state_bridge.vk_poly_prep_in_queue_backing,
                               APP_VK_POLY_PREP_QUEUE_CAPACITY)) {
        return false;
    }
    if (!core_queue_mutex_init(&app->worker_state_bridge.vk_poly_prep_out_queue,
                               app->worker_state_bridge.vk_poly_prep_out_queue_backing,
                               APP_VK_POLY_PREP_QUEUE_CAPACITY)) {
        core_queue_mutex_destroy(&app->worker_state_bridge.vk_poly_prep_in_queue);
        return false;
    }
    if (pthread_mutex_init(&app->worker_state_bridge.vk_poly_prep_mutex, NULL) != 0) {
        core_queue_mutex_destroy(&app->worker_state_bridge.vk_poly_prep_out_queue);
        core_queue_mutex_destroy(&app->worker_state_bridge.vk_poly_prep_in_queue);
        return false;
    }
    if (pthread_cond_init(&app->worker_state_bridge.vk_poly_prep_cond, NULL) != 0) {
        pthread_mutex_destroy(&app->worker_state_bridge.vk_poly_prep_mutex);
        core_queue_mutex_destroy(&app->worker_state_bridge.vk_poly_prep_out_queue);
        core_queue_mutex_destroy(&app->worker_state_bridge.vk_poly_prep_in_queue);
        return false;
    }
    app->worker_state_bridge.vk_poly_prep_running = true;
    if (pthread_create(&app->worker_state_bridge.vk_poly_prep_thread, NULL, app_vk_poly_prep_thread_main, app) != 0) {
        app->worker_state_bridge.vk_poly_prep_running = false;
        pthread_cond_destroy(&app->worker_state_bridge.vk_poly_prep_cond);
        pthread_mutex_destroy(&app->worker_state_bridge.vk_poly_prep_mutex);
        core_queue_mutex_destroy(&app->worker_state_bridge.vk_poly_prep_out_queue);
        core_queue_mutex_destroy(&app->worker_state_bridge.vk_poly_prep_in_queue);
        return false;
    }
    app->worker_state_bridge.vk_poly_prep_enabled = true;
    return true;
}

void app_vk_poly_prep_shutdown(AppState *app) {
    if (!app || !app->worker_state_bridge.vk_poly_prep_enabled) {
        return;
    }
    pthread_mutex_lock(&app->worker_state_bridge.vk_poly_prep_mutex);
    app->worker_state_bridge.vk_poly_prep_running = false;
    pthread_cond_broadcast(&app->worker_state_bridge.vk_poly_prep_cond);
    pthread_mutex_unlock(&app->worker_state_bridge.vk_poly_prep_mutex);
    pthread_join(app->worker_state_bridge.vk_poly_prep_thread, NULL);
    app_vk_poly_prep_clear(app);
    pthread_cond_destroy(&app->worker_state_bridge.vk_poly_prep_cond);
    pthread_mutex_destroy(&app->worker_state_bridge.vk_poly_prep_mutex);
    core_queue_mutex_destroy(&app->worker_state_bridge.vk_poly_prep_out_queue);
    core_queue_mutex_destroy(&app->worker_state_bridge.vk_poly_prep_in_queue);
    app->worker_state_bridge.vk_poly_prep_enabled = false;
}

void app_vk_poly_prep_clear(AppState *app) {
    if (!app || !app->worker_state_bridge.vk_poly_prep_enabled) {
        return;
    }
    pthread_mutex_lock(&app->worker_state_bridge.vk_poly_prep_mutex);
    app->worker_state_bridge.vk_poly_prep_in_write_seq = 0u;
    app->worker_state_bridge.vk_poly_prep_out_write_seq = 0u;
    TileResult item = {0};
    while (app_vk_poly_prep_queue_pop(
               &app->worker_state_bridge.vk_poly_prep_in_queue,
               app->worker_state_bridge.vk_poly_prep_in_jobs,
               &item)) {
        if (item.ok) {
            mft_free_tile(&item.tile);
        }
        memset(&item, 0, sizeof(item));
    }
    while (app_vk_poly_prep_queue_pop(
               &app->worker_state_bridge.vk_poly_prep_out_queue,
               app->worker_state_bridge.vk_poly_prep_out_jobs,
               &item)) {
        if (item.ok) {
            mft_free_tile(&item.tile);
        }
        memset(&item, 0, sizeof(item));
    }
    pthread_mutex_unlock(&app->worker_state_bridge.vk_poly_prep_mutex);
}

bool app_vk_poly_prep_enqueue(AppState *app, const TileResult *result) {
    if (!app || !result || !app->worker_state_bridge.vk_poly_prep_enabled || !app_kind_is_polygon(result->kind)) {
        return false;
    }
    bool pushed = false;
    pthread_mutex_lock(&app->worker_state_bridge.vk_poly_prep_mutex);
    pushed = app_vk_poly_prep_queue_push(
        &app->worker_state_bridge.vk_poly_prep_in_queue,
        app->worker_state_bridge.vk_poly_prep_in_jobs,
        &app->worker_state_bridge.vk_poly_prep_in_write_seq,
        result);
    if (pushed) {
        app->worker_state_bridge.vk_poly_prep_enqueued_count += 1u;
        pthread_cond_signal(&app->worker_state_bridge.vk_poly_prep_cond);
    } else {
        app->worker_state_bridge.vk_poly_prep_drop_count += 1u;
    }
    pthread_mutex_unlock(&app->worker_state_bridge.vk_poly_prep_mutex);
    return pushed;
}

void app_vk_poly_prep_drain(AppState *app, uint32_t max_results, double max_time_slice_sec) {
    if (!app || !app->worker_state_bridge.vk_poly_prep_enabled || max_results == 0u) {
        return;
    }
    double start = time_now_seconds();
    uint32_t drained = 0u;
    while (drained < max_results) {
        if (drained > 0u && (time_now_seconds() - start) >= max_time_slice_sec) {
            break;
        }
        TileResult result = {0};
        bool ok = app_vk_poly_prep_queue_pop(
            &app->worker_state_bridge.vk_poly_prep_out_queue,
            app->worker_state_bridge.vk_poly_prep_out_jobs,
            &result);
        if (!ok) {
            break;
        }

        if (!app_worker_contract_tile_request_is_current(app, result.request_id)) {
            if (result.ok) {
                mft_free_tile(&result.tile);
            }
            continue;
        }
        if (!result.ok) {
            continue;
        }
        if (!tile_manager_put_tile(&app->tile_state_bridge.tile_managers[result.kind], result.coord, result.band, &result.tile)) {
            mft_free_tile(&result.tile);
            continue;
        }
        bool is_ideal = (app->tile_state_bridge.layer_target_band[result.kind] == result.band);
        app_tile_lifecycle_transition(app,
                                      result.kind,
                                      result.coord,
                                      result.band,
                                      APP_TILE_LIFECYCLE_RENDERABLE,
                                      true,
                                      false,
                                      !is_ideal,
                                      is_ideal);
        app_vk_asset_enqueue(app, result.kind, result.coord, result.band);
        drained += 1u;
    }
}

void app_vk_poly_prep_get_stats(AppState *app, VkPolyPrepStats *out_stats) {
    if (!out_stats) {
        return;
    }
    memset(out_stats, 0, sizeof(*out_stats));
    if (!app || !app->worker_state_bridge.vk_poly_prep_enabled) {
        return;
    }
    pthread_mutex_lock(&app->worker_state_bridge.vk_poly_prep_mutex);
    out_stats->in_count = (uint32_t)core_queue_mutex_size(&app->worker_state_bridge.vk_poly_prep_in_queue);
    out_stats->out_count = (uint32_t)core_queue_mutex_size(&app->worker_state_bridge.vk_poly_prep_out_queue);
    out_stats->enqueued_count = app->worker_state_bridge.vk_poly_prep_enqueued_count;
    out_stats->done_count = app->worker_state_bridge.vk_poly_prep_done_count;
    out_stats->drop_count = app->worker_state_bridge.vk_poly_prep_drop_count;
    out_stats->quarantine_job_count = app->worker_state_bridge.vk_poly_prep_quarantine_job_count;
    out_stats->quarantine_polygon_count = app->worker_state_bridge.vk_poly_prep_quarantine_polygon_count;
    out_stats->quarantine_ring_bounds_count = app->worker_state_bridge.vk_poly_prep_quarantine_ring_bounds_count;
    out_stats->quarantine_ring_min_points_count = app->worker_state_bridge.vk_poly_prep_quarantine_ring_min_points_count;
    out_stats->quarantine_ring_degenerate_count = app->worker_state_bridge.vk_poly_prep_quarantine_ring_degenerate_count;
    out_stats->winding_normalized_count = app->worker_state_bridge.vk_poly_prep_winding_normalized_count;
    pthread_mutex_unlock(&app->worker_state_bridge.vk_poly_prep_mutex);
}

void app_vk_asset_queue_clear(AppState *app) {
    if (!app) {
        return;
    }
    app->worker_state_bridge.vk_asset_job_head = 0u;
    app->worker_state_bridge.vk_asset_job_tail = 0u;
    app->worker_state_bridge.vk_asset_job_count = 0u;
    if (app->worker_state_bridge.vk_asset_worker_enabled) {
        pthread_mutex_lock(&app->worker_state_bridge.vk_asset_worker_mutex);
        app->worker_state_bridge.vk_asset_stage_head = 0u;
        app->worker_state_bridge.vk_asset_stage_tail = 0u;
        app->worker_state_bridge.vk_asset_stage_count = 0u;
        app->worker_state_bridge.vk_asset_ready_write_seq = 0u;
        pthread_mutex_unlock(&app->worker_state_bridge.vk_asset_worker_mutex);
        void *token = NULL;
        while (core_queue_mutex_pop(&app->worker_state_bridge.vk_asset_ready_queue, &token)) {
        }
    }
}

bool app_vk_asset_worker_init(AppState *app) {
    if (!app) {
        return false;
    }
    app->worker_state_bridge.vk_asset_worker_enabled = false;
    app->worker_state_bridge.vk_asset_worker_running = false;
    app->worker_state_bridge.vk_asset_stage_head = 0u;
    app->worker_state_bridge.vk_asset_stage_tail = 0u;
    app->worker_state_bridge.vk_asset_stage_count = 0u;
    app->worker_state_bridge.vk_asset_ready_write_seq = 0u;
    app->worker_state_bridge.vk_asset_stage_drop_count = 0u;
    app->worker_state_bridge.vk_asset_stage_evict_count = 0u;
    app->worker_state_bridge.vk_asset_stage_enqueued_count = 0u;
    app->worker_state_bridge.vk_asset_stage_prepared_count = 0u;
    if (!core_queue_mutex_init(&app->worker_state_bridge.vk_asset_ready_queue,
                               app->worker_state_bridge.vk_asset_ready_queue_backing,
                               APP_VK_ASSET_READY_QUEUE_CAPACITY)) {
        return false;
    }
    if (pthread_mutex_init(&app->worker_state_bridge.vk_asset_worker_mutex, NULL) != 0) {
        core_queue_mutex_destroy(&app->worker_state_bridge.vk_asset_ready_queue);
        return false;
    }
    if (pthread_cond_init(&app->worker_state_bridge.vk_asset_worker_cond, NULL) != 0) {
        pthread_mutex_destroy(&app->worker_state_bridge.vk_asset_worker_mutex);
        core_queue_mutex_destroy(&app->worker_state_bridge.vk_asset_ready_queue);
        return false;
    }
    app->worker_state_bridge.vk_asset_worker_running = true;
    if (pthread_create(&app->worker_state_bridge.vk_asset_worker_thread, NULL, app_vk_asset_worker_thread_main, app) != 0) {
        app->worker_state_bridge.vk_asset_worker_running = false;
        pthread_cond_destroy(&app->worker_state_bridge.vk_asset_worker_cond);
        pthread_mutex_destroy(&app->worker_state_bridge.vk_asset_worker_mutex);
        core_queue_mutex_destroy(&app->worker_state_bridge.vk_asset_ready_queue);
        return false;
    }
    app->worker_state_bridge.vk_asset_worker_enabled = true;
    return true;
}

void app_vk_asset_worker_shutdown(AppState *app) {
    if (!app || !app->worker_state_bridge.vk_asset_worker_enabled) {
        return;
    }
    pthread_mutex_lock(&app->worker_state_bridge.vk_asset_worker_mutex);
    app->worker_state_bridge.vk_asset_worker_running = false;
    pthread_cond_broadcast(&app->worker_state_bridge.vk_asset_worker_cond);
    pthread_mutex_unlock(&app->worker_state_bridge.vk_asset_worker_mutex);
    pthread_join(app->worker_state_bridge.vk_asset_worker_thread, NULL);
    pthread_cond_destroy(&app->worker_state_bridge.vk_asset_worker_cond);
    pthread_mutex_destroy(&app->worker_state_bridge.vk_asset_worker_mutex);
    core_queue_mutex_destroy(&app->worker_state_bridge.vk_asset_ready_queue);
    app->worker_state_bridge.vk_asset_worker_enabled = false;
}

bool app_vk_asset_enqueue(AppState *app, TileLayerKind kind, TileCoord coord, TileZoomBand band) {
    bool supported_kind = app_kind_is_polygon(kind) ||
        kind == TILE_LAYER_ROAD_ARTERY ||
        kind == TILE_LAYER_ROAD_LOCAL;
    if (!app || !app->tile_state_bridge.vk_assets_enabled || !supported_kind) {
        return false;
    }
    VkAssetJob in_job = {
        .coord = coord,
        .kind = kind,
        .band = band,
        .request_id = app->worker_state_bridge.tile_generation
    };

    if (!app->worker_state_bridge.vk_asset_worker_enabled) {
        bool accepted_main = app_vk_asset_main_admit_job(app, &in_job);
        if (accepted_main) {
            app_tile_lifecycle_transition(app,
                                          kind,
                                          coord,
                                          band,
                                          APP_TILE_LIFECYCLE_UPLOADED_GPU,
                                          true,
                                          false,
                                          false,
                                          false);
        }
        return accepted_main;
    }

    bool accepted = false;
    pthread_mutex_lock(&app->worker_state_bridge.vk_asset_worker_mutex);
    for (uint32_t i = 0u; i < app->worker_state_bridge.vk_asset_stage_count; ++i) {
        uint32_t idx = (app->worker_state_bridge.vk_asset_stage_head + i) % APP_VK_ASSET_QUEUE_CAPACITY;
        const VkAssetJob *job = &app->worker_state_bridge.vk_asset_stage_jobs[idx];
        if (job->kind == in_job.kind &&
            job->band == in_job.band &&
            job->coord.z == in_job.coord.z &&
            job->coord.x == in_job.coord.x &&
            job->coord.y == in_job.coord.y &&
            job->request_id == in_job.request_id) {
            accepted = true;
            break;
        }
    }
    if (!accepted) {
        if (app->worker_state_bridge.vk_asset_stage_count >= APP_VK_ASSET_QUEUE_CAPACITY) {
            uint32_t request_ids[APP_VK_ASSET_QUEUE_CAPACITY];
            for (uint32_t i = 0u; i < app->worker_state_bridge.vk_asset_stage_count; ++i) {
                uint32_t idx = (app->worker_state_bridge.vk_asset_stage_head + i) % APP_VK_ASSET_QUEUE_CAPACITY;
                request_ids[i] = app->worker_state_bridge.vk_asset_stage_jobs[idx].request_id;
            }
            uint32_t evict_offset = 0u;
            bool evicted = false;
            if (app_worker_contract_choose_evict_offset(request_ids,
                                                        app->worker_state_bridge.vk_asset_stage_count,
                                                        app->worker_state_bridge.tile_generation,
                                                        &evict_offset)) {
                evicted = app_vk_asset_stage_pop_at(app, evict_offset, NULL);
            }
            if (evicted) {
                app->worker_state_bridge.vk_asset_stage_evict_count += 1u;
            }
        }
        if (app_vk_asset_stage_push(app, &in_job)) {
            app->worker_state_bridge.vk_asset_stage_enqueued_count += 1u;
            accepted = true;
            pthread_cond_signal(&app->worker_state_bridge.vk_asset_worker_cond);
        }
    }
    if (!accepted) {
        app->worker_state_bridge.vk_asset_stage_drop_count += 1u;
    }
    pthread_mutex_unlock(&app->worker_state_bridge.vk_asset_worker_mutex);
    if (accepted) {
        app_tile_lifecycle_transition(app,
                                      kind,
                                      coord,
                                      band,
                                      APP_TILE_LIFECYCLE_UPLOADED_GPU,
                                      true,
                                      false,
                                      false,
                                      false);
    }
    return accepted;
}

void app_process_vk_asset_queue(AppState *app, uint32_t max_jobs, double max_time_slice_sec) {
    if (!app || !app->tile_state_bridge.vk_assets_enabled || !app->renderer.vk || max_jobs == 0u) {
        return;
    }
    if (app->worker_state_bridge.vk_asset_worker_enabled) {
        while (true) {
            VkAssetReadyJob ready = {0};
            if (!app_vk_asset_ready_pop(app, &ready)) {
                break;
            }
            VkAssetJob admitted = {
                .coord = ready.coord,
                .kind = ready.kind,
                .band = ready.band,
                .request_id = ready.request_id
            };
            app_vk_asset_main_admit_job(app, &admitted);
        }
    }

    double start = time_now_seconds();
    uint32_t processed = 0u;
    uint32_t built_by_kind[TILE_LAYER_COUNT] = {0};
    while (app->worker_state_bridge.vk_asset_job_count > 0u && processed < max_jobs) {
        if ((time_now_seconds() - start) >= max_time_slice_sec) {
            break;
        }

        bool have_best = false;
        uint64_t best_score = UINT64_MAX;
        uint32_t best_offset = 0u;
        bool have_stale = false;
        uint32_t stale_offset = 0u;

        for (uint32_t i = 0u; i < app->worker_state_bridge.vk_asset_job_count; ++i) {
            uint32_t idx = (app->worker_state_bridge.vk_asset_job_head + i) % APP_VK_ASSET_QUEUE_CAPACITY;
            const VkAssetJob *job = &app->worker_state_bridge.vk_asset_jobs[idx];
            if (!app_worker_contract_tile_request_is_current(app, job->request_id)) {
                if (!have_stale) {
                    have_stale = true;
                    stale_offset = i;
                }
                continue;
            }

            const VkTileCacheEntry *resident = vk_tile_cache_peek(&app->tile_state_bridge.vk_tile_cache, job->kind, job->coord, job->band);
            if (resident && resident->mesh_ready) {
                if (!have_stale) {
                    have_stale = true;
                    stale_offset = i;
                }
                continue;
            }

            const MftTile *tile = tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[job->kind], job->coord, job->band);
            if (!tile) {
                if (!have_stale) {
                    have_stale = true;
                    stale_offset = i;
                }
                continue;
            }

            uint32_t kind_load = (job->kind < TILE_LAYER_COUNT) ? built_by_kind[job->kind] : 0u;
            uint32_t ring_distance = app_vk_asset_visible_ring_distance(app, job->coord);
            uint32_t ring_bucket = app_vk_asset_ring_bucket(ring_distance);
            uint32_t kind_priority = app_vk_asset_kind_priority(job->kind);
            uint64_t score = ((uint64_t)kind_priority << 56) |
                             ((uint64_t)kind_load << 48) |
                             ((uint64_t)ring_bucket << 40) |
                             ((uint64_t)ring_distance << 16) |
                             (uint64_t)i;
            if (!have_best || score < best_score) {
                have_best = true;
                best_score = score;
                best_offset = i;
            }
        }

        VkAssetJob job = {0};
        if (have_best) {
            if (!app_vk_asset_pop_job_at(app, best_offset, &job)) {
                break;
            }
        } else if (have_stale) {
            if (!app_vk_asset_pop_job_at(app, stale_offset, &job)) {
                break;
            }
            continue;
        } else {
            break;
        }

        if (!app_worker_contract_tile_request_is_current(app, job.request_id)) {
            continue;
        }
        const MftTile *tile = tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[job.kind], job.coord, job.band);
        if (!tile) {
            continue;
        }
        if (vk_tile_cache_on_tile_loaded(&app->tile_state_bridge.vk_tile_cache, app->renderer.vk, job.kind, job.coord, job.band, tile)) {
            bool is_ideal = (app->tile_state_bridge.layer_target_band[job.kind] == job.band);
            app_tile_lifecycle_transition(app,
                                          job.kind,
                                          job.coord,
                                          job.band,
                                          APP_TILE_LIFECYCLE_RENDERABLE,
                                          true,
                                          true,
                                          !is_ideal,
                                          is_ideal);
            processed += 1u;
            if (job.kind < TILE_LAYER_COUNT) {
                built_by_kind[job.kind] += 1u;
            }
            app->worker_state_bridge.vk_asset_job_build_count += 1u;
        }
    }
}
