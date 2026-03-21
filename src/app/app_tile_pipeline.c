#include "app/app_internal.h"

#include "core/time.h"
#include "map/mercator.h"
#include "map/polygon_cache.h"

#include <stdlib.h>
#include <string.h>

uint32_t app_tile_load_budget(TileLayerKind kind, uint32_t expected) {
    bool polygon_layer = (kind == TILE_LAYER_POLY_WATER ||
                          kind == TILE_LAYER_POLY_PARK ||
                          kind == TILE_LAYER_POLY_LANDUSE ||
                          kind == TILE_LAYER_POLY_BUILDING);
    if (polygon_layer) {
        if (expected <= 4u) {
            return expected;
        }
        if (expected <= 16u) {
            return 2u;
        }
        return 1u;
    }
    if (expected <= 8) {
        return expected;
    }
    if (expected <= 32) {
        return 16;
    }
    if (expected <= 128) {
        return 32;
    }
    if (expected <= 256) {
        return 48;
    }
    return 64;
}

uint32_t app_tile_integrate_budget(TileLayerKind kind, uint32_t expected) {
    bool polygon_layer = (kind == TILE_LAYER_POLY_WATER ||
                          kind == TILE_LAYER_POLY_PARK ||
                          kind == TILE_LAYER_POLY_LANDUSE ||
                          kind == TILE_LAYER_POLY_BUILDING);
    if (polygon_layer) {
        if (expected <= 4u) {
            return expected;
        }
        if (kind == TILE_LAYER_POLY_BUILDING) {
            return 2u;
        }
        return 1u;
    }
    if (expected <= 32) {
        return 32;
    }
    if (expected <= 128) {
        return 48;
    }
    return 64;
}

float app_layer_zoom_start(const AppState *app, TileLayerKind kind) {
    return layer_policy_zoom_start(kind, app ? app->building_zoom_bias : 0.0f);
}

bool app_layer_runtime_enabled(const AppState *app, TileLayerKind kind) {
    if (!app) {
        return false;
    }
    if (kind < 0 || kind >= TILE_LAYER_COUNT) {
        return false;
    }
    return app->layer_user_enabled[kind];
}

float app_layer_fade_multiplier(const AppState *app, TileLayerKind kind) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return 1.0f;
    }
    if (!app->zoom_logic_enabled) {
        return 1.0f;
    }
    float start_zoom = ((float)app->layer_fade_start_milli[kind] / 1000.0f) * 20.0f;
    float span_zoom = 0.15f + ((float)app->layer_fade_speed_milli[kind] / 1000.0f) * 6.0f;
    float z = app->camera.zoom;
    if (z >= start_zoom) {
        return 1.0f;
    }
    if (z <= start_zoom - span_zoom) {
        return 0.0f;
    }
    return app_clampf((z - (start_zoom - span_zoom)) / span_zoom, 0.0f, 1.0f);
}

bool app_layer_active_runtime(const AppState *app, TileLayerKind kind) {
    if (!app_layer_runtime_enabled(app, kind)) {
        return false;
    }
    if (!app->zoom_logic_enabled) {
        return true;
    }
    return app_layer_fade_multiplier(app, kind) > 0.001f;
}

void app_update_vk_line_budget(AppState *app) {
    if (!app) {
        return;
    }
    if (renderer_get_backend(&app->renderer) != RENDERER_BACKEND_VULKAN) {
        return;
    }
    app->renderer.vk_line_budget = layer_policy_vk_line_budget(app->camera.zoom, app->visible_tile_count);
}

static bool app_compute_visible_tile_bounds(AppState *app, uint16_t *out_z, TileCoord *out_top_left, TileCoord *out_bottom_right) {
    if (!app) {
        return false;
    }

    float ppm = camera_pixels_per_meter(&app->camera);
    float half_w_m = (float)app->width * 0.5f / ppm;
    float half_h_m = (float)app->height * 0.5f / ppm;

    float min_x = app->camera.x - half_w_m;
    float max_x = app->camera.x + half_w_m;
    float min_y = app->camera.y - half_h_m;
    float max_y = app->camera.y + half_h_m;

    uint16_t z = app_zoom_to_tile_level(app->camera.zoom, &app->region);
    TileCoord top_left = tile_from_meters(z, (MercatorMeters){min_x, max_y});
    TileCoord bottom_right = tile_from_meters(z, (MercatorMeters){max_x, min_y});

    uint32_t count = tile_count(z);
    if (bottom_right.x >= count) {
        bottom_right.x = count - 1;
    }
    if (bottom_right.y >= count) {
        bottom_right.y = count - 1;
    }
    if (app->region.has_bounds) {
        MercatorMeters min_m = mercator_from_latlon((LatLon){app->region.min_lat, app->region.min_lon});
        MercatorMeters max_m = mercator_from_latlon((LatLon){app->region.max_lat, app->region.max_lon});
        TileCoord region_min = tile_from_meters(z, (MercatorMeters){min_m.x, max_m.y});
        TileCoord region_max = tile_from_meters(z, (MercatorMeters){max_m.x, min_m.y});
        if (top_left.x < region_min.x) {
            top_left.x = region_min.x;
        }
        if (top_left.y < region_min.y) {
            top_left.y = region_min.y;
        }
        if (bottom_right.x > region_max.x) {
            bottom_right.x = region_max.x;
        }
        if (bottom_right.y > region_max.y) {
            bottom_right.y = region_max.y;
        }
        if (top_left.x > bottom_right.x || top_left.y > bottom_right.y) {
            return false;
        }
    }

    if (out_z) {
        *out_z = z;
    }
    if (out_top_left) {
        *out_top_left = top_left;
    }
    if (out_bottom_right) {
        *out_bottom_right = bottom_right;
    }

    return true;
}

static int app_tile_queue_compare(const void *a, const void *b) {
    const TileQueueItem *item_a = (const TileQueueItem *)a;
    const TileQueueItem *item_b = (const TileQueueItem *)b;
    if (item_a->dist2 < item_b->dist2) {
        return -1;
    }
    if (item_a->dist2 > item_b->dist2) {
        return 1;
    }
    return 0;
}

static bool app_kind_is_polygon(TileLayerKind kind) {
    return kind == TILE_LAYER_POLY_WATER ||
           kind == TILE_LAYER_POLY_PARK ||
           kind == TILE_LAYER_POLY_LANDUSE ||
           kind == TILE_LAYER_POLY_BUILDING;
}

static TileZoomBand app_layer_target_band(const AppState *app, TileLayerKind kind) {
    if (!app) {
        return TILE_BAND_DEFAULT;
    }
    if (kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL) {
        if (!app->region.has_tile_pyramid_roads) {
            return TILE_BAND_DEFAULT;
        }
    } else if (kind == TILE_LAYER_POLY_BUILDING) {
        if (!app->region.has_tile_pyramid_buildings) {
            return TILE_BAND_DEFAULT;
        }
    }
    if (!app->zoom_logic_enabled) {
        return TILE_BAND_FINE;
    }
    return layer_policy_band_for_zoom(kind, app->camera.zoom, app->road_zoom_bias);
}

static uint32_t app_polygon_fallback_candidates(TileLayerKind kind,
                                                TileZoomBand target,
                                                TileZoomBand *out_bands,
                                                uint32_t out_cap) {
    if (!out_bands || out_cap == 0u) {
        return 0u;
    }

    uint32_t count = 0u;
    if (kind == TILE_LAYER_POLY_BUILDING) {
        if (target == TILE_BAND_FINE) {
            if (count < out_cap) out_bands[count++] = TILE_BAND_FINE;
            if (count < out_cap) out_bands[count++] = TILE_BAND_MID;
            if (count < out_cap) out_bands[count++] = TILE_BAND_COARSE;
            if (count < out_cap) out_bands[count++] = TILE_BAND_DEFAULT;
            return count;
        }
        if (target == TILE_BAND_MID) {
            if (count < out_cap) out_bands[count++] = TILE_BAND_FINE;
            if (count < out_cap) out_bands[count++] = TILE_BAND_MID;
            if (count < out_cap) out_bands[count++] = TILE_BAND_COARSE;
            if (count < out_cap) out_bands[count++] = TILE_BAND_DEFAULT;
            return count;
        }
        if (target == TILE_BAND_COARSE) {
            if (count < out_cap) out_bands[count++] = TILE_BAND_MID;
            if (count < out_cap) out_bands[count++] = TILE_BAND_COARSE;
            if (count < out_cap) out_bands[count++] = TILE_BAND_DEFAULT;
            return count;
        }
        out_bands[count++] = TILE_BAND_DEFAULT;
        return count;
    }

    if (target == TILE_BAND_FINE) {
        if (count < out_cap) out_bands[count++] = TILE_BAND_FINE;
        if (count < out_cap) out_bands[count++] = TILE_BAND_MID;
        if (count < out_cap) out_bands[count++] = TILE_BAND_COARSE;
        if (count < out_cap) out_bands[count++] = TILE_BAND_DEFAULT;
        return count;
    }
    if (target == TILE_BAND_MID) {
        if (count < out_cap) out_bands[count++] = TILE_BAND_MID;
        if (count < out_cap) out_bands[count++] = TILE_BAND_COARSE;
        if (count < out_cap) out_bands[count++] = TILE_BAND_DEFAULT;
        return count;
    }
    if (target == TILE_BAND_COARSE) {
        if (count < out_cap) out_bands[count++] = TILE_BAND_COARSE;
        if (count < out_cap) out_bands[count++] = TILE_BAND_DEFAULT;
        return count;
    }
    out_bands[count++] = TILE_BAND_DEFAULT;
    return count;
}

static bool app_has_visible_tile_with_fallback(const AppState *app,
                                               TileLayerKind kind,
                                               TileCoord coord,
                                               TileZoomBand target_band) {
    if (!app) {
        return false;
    }

    if (kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL) {
        TileZoomBand candidates[4] = {target_band, TILE_BAND_MID, TILE_BAND_COARSE, TILE_BAND_DEFAULT};
        uint32_t count = 1u;
        if (target_band == TILE_BAND_FINE) {
            count = 4u;
        } else if (target_band == TILE_BAND_MID) {
            candidates[1] = TILE_BAND_COARSE;
            candidates[2] = TILE_BAND_DEFAULT;
            count = 3u;
        } else if (target_band == TILE_BAND_COARSE) {
            candidates[1] = TILE_BAND_DEFAULT;
            count = 2u;
        }
        for (uint32_t i = 0u; i < count; ++i) {
            if (tile_manager_peek_tile(&app->tile_managers[kind], coord, candidates[i])) {
                return true;
            }
        }
        return false;
    }

    if (kind == TILE_LAYER_POLY_WATER ||
        kind == TILE_LAYER_POLY_PARK ||
        kind == TILE_LAYER_POLY_LANDUSE ||
        kind == TILE_LAYER_POLY_BUILDING) {
        TileZoomBand candidates[4] = {TILE_BAND_DEFAULT, TILE_BAND_DEFAULT, TILE_BAND_DEFAULT, TILE_BAND_DEFAULT};
        uint32_t count = app_polygon_fallback_candidates(kind, target_band, candidates, 4u);
        for (uint32_t i = 0u; i < count; ++i) {
            if (tile_manager_peek_tile(&app->tile_managers[kind], coord, candidates[i])) {
                return true;
            }
        }
        return false;
    }

    return tile_manager_peek_tile(&app->tile_managers[kind], coord, target_band) != NULL;
}

static uint32_t app_vk_asset_visible_ring_distance(const AppState *app, TileCoord coord) {
    if (!app || !app->visible_valid || coord.z != app->visible_zoom) {
        return UINT32_MAX / 4u;
    }

    uint32_t dx = 0u;
    uint32_t dy = 0u;
    if (coord.x < app->visible_top_left.x) {
        dx = app->visible_top_left.x - coord.x;
    } else if (coord.x > app->visible_bottom_right.x) {
        dx = coord.x - app->visible_bottom_right.x;
    }
    if (coord.y < app->visible_top_left.y) {
        dy = app->visible_top_left.y - coord.y;
    } else if (coord.y > app->visible_bottom_right.y) {
        dy = coord.y - app->visible_bottom_right.y;
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

static bool app_vk_asset_pop_job_at(AppState *app, uint32_t offset, VkAssetJob *out_job) {
    if (!app || !out_job || offset >= app->vk_asset_job_count) {
        return false;
    }

    uint32_t cap = APP_VK_ASSET_QUEUE_CAPACITY;
    uint32_t head = app->vk_asset_job_head;
    uint32_t idx = (head + offset) % cap;
    *out_job = app->vk_asset_jobs[idx];

    for (uint32_t i = offset; i + 1u < app->vk_asset_job_count; ++i) {
        uint32_t from = (head + i + 1u) % cap;
        uint32_t to = (head + i) % cap;
        app->vk_asset_jobs[to] = app->vk_asset_jobs[from];
    }

    if (app->vk_asset_job_count > 0u) {
        app->vk_asset_job_tail = (app->vk_asset_job_tail + cap - 1u) % cap;
        app->vk_asset_job_count -= 1u;
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
    for (uint32_t i = 0u; i < app->vk_asset_job_count; ++i) {
        uint32_t idx = (app->vk_asset_job_head + i) % APP_VK_ASSET_QUEUE_CAPACITY;
        const VkAssetJob *job = &app->vk_asset_jobs[idx];
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

    if (app->vk_asset_job_count >= APP_VK_ASSET_QUEUE_CAPACITY) {
        bool evicted = false;
        uint64_t evict_score = 0u;
        uint32_t evict_offset = 0u;
        for (uint32_t i = 0u; i < app->vk_asset_job_count; ++i) {
            uint32_t idx = (app->vk_asset_job_head + i) % APP_VK_ASSET_QUEUE_CAPACITY;
            const VkAssetJob *job = &app->vk_asset_jobs[idx];
            uint32_t stale = (job->request_id == app->tile_request_id) ? 0u : 1u;
            uint32_t ring = app_vk_asset_visible_ring_distance(app, job->coord);
            uint64_t score = ((uint64_t)stale << 63) |
                             ((uint64_t)ring << 24) |
                             (uint64_t)i;
            if (!evicted || score > evict_score) {
                evicted = true;
                evict_score = score;
                evict_offset = i;
            }
        }
        if (evicted) {
            VkAssetJob dropped = {0};
            if (app_vk_asset_pop_job_at(app, evict_offset, &dropped)) {
                app->vk_asset_job_evict_count += 1u;
            }
        }
    }

    if (app->vk_asset_job_count >= APP_VK_ASSET_QUEUE_CAPACITY) {
        app->vk_asset_job_drop_count += 1u;
        return false;
    }

    app->vk_asset_jobs[app->vk_asset_job_tail] = *in_job;
    app->vk_asset_job_tail = (app->vk_asset_job_tail + 1u) % APP_VK_ASSET_QUEUE_CAPACITY;
    app->vk_asset_job_count += 1u;
    return true;
}

static bool app_vk_asset_stage_push(AppState *app, const VkAssetJob *job) {
    if (!app || !job || app->vk_asset_stage_count >= APP_VK_ASSET_QUEUE_CAPACITY) {
        return false;
    }
    app->vk_asset_stage_jobs[app->vk_asset_stage_tail] = *job;
    app->vk_asset_stage_tail = (app->vk_asset_stage_tail + 1u) % APP_VK_ASSET_QUEUE_CAPACITY;
    app->vk_asset_stage_count += 1u;
    return true;
}

static bool app_vk_asset_stage_pop(AppState *app, VkAssetJob *out_job) {
    if (!app || !out_job || app->vk_asset_stage_count == 0u) {
        return false;
    }
    *out_job = app->vk_asset_stage_jobs[app->vk_asset_stage_head];
    memset(&app->vk_asset_stage_jobs[app->vk_asset_stage_head], 0, sizeof(app->vk_asset_stage_jobs[app->vk_asset_stage_head]));
    app->vk_asset_stage_head = (app->vk_asset_stage_head + 1u) % APP_VK_ASSET_QUEUE_CAPACITY;
    app->vk_asset_stage_count -= 1u;
    return true;
}

static bool app_vk_asset_stage_pop_at(AppState *app, uint32_t offset, VkAssetJob *out_job) {
    if (!app || offset >= app->vk_asset_stage_count) {
        return false;
    }
    uint32_t cap = APP_VK_ASSET_QUEUE_CAPACITY;
    uint32_t head = app->vk_asset_stage_head;
    uint32_t idx = (head + offset) % cap;
    if (out_job) {
        *out_job = app->vk_asset_stage_jobs[idx];
    }
    for (uint32_t i = offset; i + 1u < app->vk_asset_stage_count; ++i) {
        uint32_t from = (head + i + 1u) % cap;
        uint32_t to = (head + i) % cap;
        app->vk_asset_stage_jobs[to] = app->vk_asset_stage_jobs[from];
    }
    app->vk_asset_stage_tail = (app->vk_asset_stage_tail + cap - 1u) % cap;
    app->vk_asset_stage_count -= 1u;
    return true;
}

static bool app_vk_asset_ready_push(AppState *app, const VkAssetReadyJob *job) {
    if (!app || !job) {
        return false;
    }
    uint32_t slot = app->vk_asset_ready_write_seq % APP_VK_ASSET_READY_QUEUE_CAPACITY;
    app->vk_asset_ready_jobs[slot] = *job;
    void *token = (void *)(uintptr_t)(slot + 1u);
    if (!core_queue_mutex_push(&app->vk_asset_ready_queue, token)) {
        return false;
    }
    app->vk_asset_ready_write_seq += 1u;
    return true;
}

static bool app_vk_asset_ready_pop(AppState *app, VkAssetReadyJob *out_job) {
    if (!app || !out_job) {
        return false;
    }
    void *token = NULL;
    if (!core_queue_mutex_pop(&app->vk_asset_ready_queue, &token)) {
        return false;
    }
    uintptr_t encoded = (uintptr_t)token;
    if (encoded == 0u || encoded > APP_VK_ASSET_READY_QUEUE_CAPACITY) {
        return false;
    }
    uint32_t slot = (uint32_t)(encoded - 1u);
    *out_job = app->vk_asset_ready_jobs[slot];
    return true;
}

static void *app_vk_asset_worker_thread_main(void *userdata) {
    AppState *app = (AppState *)userdata;
    if (!app) {
        return NULL;
    }

    for (;;) {
        VkAssetJob stage_job = {0};
        pthread_mutex_lock(&app->vk_asset_worker_mutex);
        while (app->vk_asset_worker_running && app->vk_asset_stage_count == 0u) {
            pthread_cond_wait(&app->vk_asset_worker_cond, &app->vk_asset_worker_mutex);
        }
        if (!app->vk_asset_worker_running) {
            pthread_mutex_unlock(&app->vk_asset_worker_mutex);
            break;
        }
        if (!app_vk_asset_stage_pop(app, &stage_job)) {
            pthread_mutex_unlock(&app->vk_asset_worker_mutex);
            continue;
        }

        VkAssetReadyJob ready = {
            .coord = stage_job.coord,
            .kind = stage_job.kind,
            .band = stage_job.band,
            .request_id = stage_job.request_id
        };
        bool pushed = app_vk_asset_ready_push(app, &ready);
        if (!pushed && core_queue_mutex_size(&app->vk_asset_ready_queue) > 0u) {
            VkAssetReadyJob dropped = {0};
            app_vk_asset_ready_pop(app, &dropped);
            app->vk_asset_stage_evict_count += 1u;
            pushed = app_vk_asset_ready_push(app, &ready);
        }
        if (pushed) {
            app->vk_asset_stage_prepared_count += 1u;
        } else {
            app->vk_asset_stage_drop_count += 1u;
        }
        pthread_mutex_unlock(&app->vk_asset_worker_mutex);
    }

    return NULL;
}

static void app_refresh_visible_layer_coverage(AppState *app) {
    if (!app) {
        return;
    }

    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        app->layer_visible_expected[i] = 0u;
        app->layer_visible_loaded[i] = 0u;
    }
    for (size_t i = 0; i < TILE_BAND_COUNT; ++i) {
        app->band_visible_expected[i] = 0u;
        app->band_visible_loaded[i] = 0u;
    }
    if (!app->visible_valid) {
        return;
    }

    for (uint32_t y = app->visible_top_left.y; y <= app->visible_bottom_right.y; ++y) {
        for (uint32_t x = app->visible_top_left.x; x <= app->visible_bottom_right.x; ++x) {
            TileCoord coord = {app->visible_zoom, x, y};
            for (size_t i = 0; i < layer_policy_count(); ++i) {
                const LayerPolicy *policy = layer_policy_at(i);
                if (!policy) {
                    continue;
                }
                TileLayerKind kind = policy->kind;
                if (!app_layer_active_runtime(app, kind)) {
                    continue;
                }
                app->layer_visible_expected[kind] += 1u;
                TileZoomBand band = app->layer_target_band[kind];
                if ((size_t)band < TILE_BAND_COUNT) {
                    app->band_visible_expected[band] += 1u;
                }
                if (app_has_visible_tile_with_fallback(app, kind, coord, band)) {
                    app->layer_visible_loaded[kind] += 1u;
                    if ((size_t)band < TILE_BAND_COUNT) {
                        app->band_visible_loaded[band] += 1u;
                    }
                }
            }
        }
    }
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

        pthread_mutex_lock(&app->vk_poly_prep_mutex);
        while (app->vk_poly_prep_running && core_queue_mutex_size(&app->vk_poly_prep_in_queue) == 0u) {
            pthread_cond_wait(&app->vk_poly_prep_cond, &app->vk_poly_prep_mutex);
        }
        if (!app->vk_poly_prep_running) {
            pthread_mutex_unlock(&app->vk_poly_prep_mutex);
            break;
        }
        bool has_job = app_vk_poly_prep_queue_pop(
            &app->vk_poly_prep_in_queue,
            app->vk_poly_prep_in_jobs,
            &job);
        pthread_mutex_unlock(&app->vk_poly_prep_mutex);
        if (!has_job) {
            continue;
        }

        if (job.ok && app_kind_is_polygon(job.kind)) {
            polygon_cache_build(&job.tile);
        }

        pthread_mutex_lock(&app->vk_poly_prep_mutex);
        bool pushed = app_vk_poly_prep_queue_push(
            &app->vk_poly_prep_out_queue,
            app->vk_poly_prep_out_jobs,
            &app->vk_poly_prep_out_write_seq,
            &job);
        if (pushed) {
            app->vk_poly_prep_done_count += 1u;
        } else {
            app->vk_poly_prep_drop_count += 1u;
        }
        pthread_mutex_unlock(&app->vk_poly_prep_mutex);
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
    app->vk_poly_prep_enabled = false;
    app->vk_poly_prep_running = false;
    app->vk_poly_prep_in_write_seq = 0u;
    app->vk_poly_prep_out_write_seq = 0u;
    app->vk_poly_prep_enqueued_count = 0u;
    app->vk_poly_prep_done_count = 0u;
    app->vk_poly_prep_drop_count = 0u;
    if (!core_queue_mutex_init(&app->vk_poly_prep_in_queue,
                               app->vk_poly_prep_in_queue_backing,
                               APP_VK_POLY_PREP_QUEUE_CAPACITY)) {
        return false;
    }
    if (!core_queue_mutex_init(&app->vk_poly_prep_out_queue,
                               app->vk_poly_prep_out_queue_backing,
                               APP_VK_POLY_PREP_QUEUE_CAPACITY)) {
        core_queue_mutex_destroy(&app->vk_poly_prep_in_queue);
        return false;
    }
    if (pthread_mutex_init(&app->vk_poly_prep_mutex, NULL) != 0) {
        core_queue_mutex_destroy(&app->vk_poly_prep_out_queue);
        core_queue_mutex_destroy(&app->vk_poly_prep_in_queue);
        return false;
    }
    if (pthread_cond_init(&app->vk_poly_prep_cond, NULL) != 0) {
        pthread_mutex_destroy(&app->vk_poly_prep_mutex);
        core_queue_mutex_destroy(&app->vk_poly_prep_out_queue);
        core_queue_mutex_destroy(&app->vk_poly_prep_in_queue);
        return false;
    }
    app->vk_poly_prep_running = true;
    if (pthread_create(&app->vk_poly_prep_thread, NULL, app_vk_poly_prep_thread_main, app) != 0) {
        app->vk_poly_prep_running = false;
        pthread_cond_destroy(&app->vk_poly_prep_cond);
        pthread_mutex_destroy(&app->vk_poly_prep_mutex);
        core_queue_mutex_destroy(&app->vk_poly_prep_out_queue);
        core_queue_mutex_destroy(&app->vk_poly_prep_in_queue);
        return false;
    }
    app->vk_poly_prep_enabled = true;
    return true;
}

void app_vk_poly_prep_shutdown(AppState *app) {
    if (!app || !app->vk_poly_prep_enabled) {
        return;
    }
    pthread_mutex_lock(&app->vk_poly_prep_mutex);
    app->vk_poly_prep_running = false;
    pthread_cond_broadcast(&app->vk_poly_prep_cond);
    pthread_mutex_unlock(&app->vk_poly_prep_mutex);
    pthread_join(app->vk_poly_prep_thread, NULL);
    app_vk_poly_prep_clear(app);
    pthread_cond_destroy(&app->vk_poly_prep_cond);
    pthread_mutex_destroy(&app->vk_poly_prep_mutex);
    core_queue_mutex_destroy(&app->vk_poly_prep_out_queue);
    core_queue_mutex_destroy(&app->vk_poly_prep_in_queue);
    app->vk_poly_prep_enabled = false;
}

void app_vk_poly_prep_clear(AppState *app) {
    if (!app || !app->vk_poly_prep_enabled) {
        return;
    }
    pthread_mutex_lock(&app->vk_poly_prep_mutex);
    app->vk_poly_prep_in_write_seq = 0u;
    app->vk_poly_prep_out_write_seq = 0u;
    TileResult item = {0};
    while (app_vk_poly_prep_queue_pop(
               &app->vk_poly_prep_in_queue,
               app->vk_poly_prep_in_jobs,
               &item)) {
        if (item.ok) {
            mft_free_tile(&item.tile);
        }
        memset(&item, 0, sizeof(item));
    }
    while (app_vk_poly_prep_queue_pop(
               &app->vk_poly_prep_out_queue,
               app->vk_poly_prep_out_jobs,
               &item)) {
        if (item.ok) {
            mft_free_tile(&item.tile);
        }
        memset(&item, 0, sizeof(item));
    }
    pthread_mutex_unlock(&app->vk_poly_prep_mutex);
}

bool app_vk_poly_prep_enqueue(AppState *app, const TileResult *result) {
    if (!app || !result || !app->vk_poly_prep_enabled || !app_kind_is_polygon(result->kind)) {
        return false;
    }
    bool pushed = false;
    pthread_mutex_lock(&app->vk_poly_prep_mutex);
    pushed = app_vk_poly_prep_queue_push(
        &app->vk_poly_prep_in_queue,
        app->vk_poly_prep_in_jobs,
        &app->vk_poly_prep_in_write_seq,
        result);
    if (pushed) {
        app->vk_poly_prep_enqueued_count += 1u;
        pthread_cond_signal(&app->vk_poly_prep_cond);
    } else {
        app->vk_poly_prep_drop_count += 1u;
    }
    pthread_mutex_unlock(&app->vk_poly_prep_mutex);
    return pushed;
}

void app_vk_poly_prep_drain(AppState *app, uint32_t max_results, double max_time_slice_sec) {
    if (!app || !app->vk_poly_prep_enabled || max_results == 0u) {
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
            &app->vk_poly_prep_out_queue,
            app->vk_poly_prep_out_jobs,
            &result);
        if (!ok) {
            break;
        }

        if (result.request_id != app->tile_request_id) {
            if (result.ok) {
                mft_free_tile(&result.tile);
            }
            continue;
        }
        if (!result.ok) {
            continue;
        }
        if (!tile_manager_put_tile(&app->tile_managers[result.kind], result.coord, result.band, &result.tile)) {
            mft_free_tile(&result.tile);
            continue;
        }
        app_vk_asset_enqueue(app, result.kind, result.coord, result.band);
        drained += 1u;
    }
}

void app_vk_poly_prep_get_stats(AppState *app, VkPolyPrepStats *out_stats) {
    if (!out_stats) {
        return;
    }
    memset(out_stats, 0, sizeof(*out_stats));
    if (!app || !app->vk_poly_prep_enabled) {
        return;
    }
    pthread_mutex_lock(&app->vk_poly_prep_mutex);
    out_stats->in_count = (uint32_t)core_queue_mutex_size(&app->vk_poly_prep_in_queue);
    out_stats->out_count = (uint32_t)core_queue_mutex_size(&app->vk_poly_prep_out_queue);
    out_stats->enqueued_count = app->vk_poly_prep_enqueued_count;
    out_stats->done_count = app->vk_poly_prep_done_count;
    out_stats->drop_count = app->vk_poly_prep_drop_count;
    pthread_mutex_unlock(&app->vk_poly_prep_mutex);
}

void app_vk_asset_queue_clear(AppState *app) {
    if (!app) {
        return;
    }
    app->vk_asset_job_head = 0u;
    app->vk_asset_job_tail = 0u;
    app->vk_asset_job_count = 0u;
    if (app->vk_asset_worker_enabled) {
        pthread_mutex_lock(&app->vk_asset_worker_mutex);
        app->vk_asset_stage_head = 0u;
        app->vk_asset_stage_tail = 0u;
        app->vk_asset_stage_count = 0u;
        app->vk_asset_ready_write_seq = 0u;
        pthread_mutex_unlock(&app->vk_asset_worker_mutex);
        void *token = NULL;
        while (core_queue_mutex_pop(&app->vk_asset_ready_queue, &token)) {
        }
    }
}

bool app_vk_asset_worker_init(AppState *app) {
    if (!app) {
        return false;
    }
    app->vk_asset_worker_enabled = false;
    app->vk_asset_worker_running = false;
    app->vk_asset_stage_head = 0u;
    app->vk_asset_stage_tail = 0u;
    app->vk_asset_stage_count = 0u;
    app->vk_asset_ready_write_seq = 0u;
    app->vk_asset_stage_drop_count = 0u;
    app->vk_asset_stage_evict_count = 0u;
    app->vk_asset_stage_enqueued_count = 0u;
    app->vk_asset_stage_prepared_count = 0u;
    if (!core_queue_mutex_init(&app->vk_asset_ready_queue,
                               app->vk_asset_ready_queue_backing,
                               APP_VK_ASSET_READY_QUEUE_CAPACITY)) {
        return false;
    }
    if (pthread_mutex_init(&app->vk_asset_worker_mutex, NULL) != 0) {
        core_queue_mutex_destroy(&app->vk_asset_ready_queue);
        return false;
    }
    if (pthread_cond_init(&app->vk_asset_worker_cond, NULL) != 0) {
        pthread_mutex_destroy(&app->vk_asset_worker_mutex);
        core_queue_mutex_destroy(&app->vk_asset_ready_queue);
        return false;
    }
    app->vk_asset_worker_running = true;
    if (pthread_create(&app->vk_asset_worker_thread, NULL, app_vk_asset_worker_thread_main, app) != 0) {
        app->vk_asset_worker_running = false;
        pthread_cond_destroy(&app->vk_asset_worker_cond);
        pthread_mutex_destroy(&app->vk_asset_worker_mutex);
        core_queue_mutex_destroy(&app->vk_asset_ready_queue);
        return false;
    }
    app->vk_asset_worker_enabled = true;
    return true;
}

void app_vk_asset_worker_shutdown(AppState *app) {
    if (!app || !app->vk_asset_worker_enabled) {
        return;
    }
    pthread_mutex_lock(&app->vk_asset_worker_mutex);
    app->vk_asset_worker_running = false;
    pthread_cond_broadcast(&app->vk_asset_worker_cond);
    pthread_mutex_unlock(&app->vk_asset_worker_mutex);
    pthread_join(app->vk_asset_worker_thread, NULL);
    pthread_cond_destroy(&app->vk_asset_worker_cond);
    pthread_mutex_destroy(&app->vk_asset_worker_mutex);
    core_queue_mutex_destroy(&app->vk_asset_ready_queue);
    app->vk_asset_worker_enabled = false;
}

bool app_vk_asset_enqueue(AppState *app, TileLayerKind kind, TileCoord coord, TileZoomBand band) {
    bool supported_kind = app_kind_is_polygon(kind) ||
        kind == TILE_LAYER_ROAD_ARTERY ||
        kind == TILE_LAYER_ROAD_LOCAL;
    if (!app || !app->vk_assets_enabled || !supported_kind) {
        return false;
    }
    VkAssetJob in_job = {
        .coord = coord,
        .kind = kind,
        .band = band,
        .request_id = app->tile_request_id
    };

    if (!app->vk_asset_worker_enabled) {
        return app_vk_asset_main_admit_job(app, &in_job);
    }

    bool accepted = false;
    pthread_mutex_lock(&app->vk_asset_worker_mutex);
    for (uint32_t i = 0u; i < app->vk_asset_stage_count; ++i) {
        uint32_t idx = (app->vk_asset_stage_head + i) % APP_VK_ASSET_QUEUE_CAPACITY;
        const VkAssetJob *job = &app->vk_asset_stage_jobs[idx];
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
        if (app->vk_asset_stage_count >= APP_VK_ASSET_QUEUE_CAPACITY) {
            bool evicted = false;
            for (uint32_t i = 0u; i < app->vk_asset_stage_count; ++i) {
                uint32_t idx = (app->vk_asset_stage_head + i) % APP_VK_ASSET_QUEUE_CAPACITY;
                if (app->vk_asset_stage_jobs[idx].request_id != in_job.request_id) {
                    evicted = app_vk_asset_stage_pop_at(app, i, NULL);
                    break;
                }
            }
            if (!evicted) {
                evicted = app_vk_asset_stage_pop_at(app, 0u, NULL);
            }
            if (evicted) {
                app->vk_asset_stage_evict_count += 1u;
            }
        }
        if (app_vk_asset_stage_push(app, &in_job)) {
            app->vk_asset_stage_enqueued_count += 1u;
            accepted = true;
            pthread_cond_signal(&app->vk_asset_worker_cond);
        }
    }
    if (!accepted) {
        app->vk_asset_job_drop_count += 1u;
    }
    pthread_mutex_unlock(&app->vk_asset_worker_mutex);
    return accepted;
}

void app_process_vk_asset_queue(AppState *app, uint32_t max_jobs, double max_time_slice_sec) {
    if (!app || !app->vk_assets_enabled || !app->renderer.vk || max_jobs == 0u) {
        return;
    }
    if (app->vk_asset_worker_enabled) {
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
    while (app->vk_asset_job_count > 0u && processed < max_jobs) {
        if ((time_now_seconds() - start) >= max_time_slice_sec) {
            break;
        }

        bool have_best = false;
        uint64_t best_score = UINT64_MAX;
        uint32_t best_offset = 0u;
        bool have_stale = false;
        uint32_t stale_offset = 0u;

        for (uint32_t i = 0u; i < app->vk_asset_job_count; ++i) {
            uint32_t idx = (app->vk_asset_job_head + i) % APP_VK_ASSET_QUEUE_CAPACITY;
            const VkAssetJob *job = &app->vk_asset_jobs[idx];
            if (job->request_id != app->tile_request_id) {
                if (!have_stale) {
                    have_stale = true;
                    stale_offset = i;
                }
                continue;
            }

            const VkTileCacheEntry *resident = vk_tile_cache_peek(&app->vk_tile_cache, job->kind, job->coord, job->band);
            if (resident && resident->mesh_ready) {
                if (!have_stale) {
                    have_stale = true;
                    stale_offset = i;
                }
                continue;
            }

            const MftTile *tile = tile_manager_peek_tile(&app->tile_managers[job->kind], job->coord, job->band);
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
            uint64_t score = ((uint64_t)kind_load << 48) |
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

        if (job.request_id != app->tile_request_id) {
            continue;
        }
        const MftTile *tile = tile_manager_peek_tile(&app->tile_managers[job.kind], job.coord, job.band);
        if (!tile) {
            continue;
        }
        if (vk_tile_cache_on_tile_loaded(&app->vk_tile_cache, app->renderer.vk, job.kind, job.coord, job.band, tile)) {
            processed += 1u;
            if (job.kind < TILE_LAYER_COUNT) {
                built_by_kind[job.kind] += 1u;
            }
            app->vk_asset_job_build_count += 1u;
        }
    }
}

void app_clear_tile_queue(AppState *app) {
    if (!app) {
        return;
    }
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        free(app->tile_queues[i].items);
        memset(&app->tile_queues[i], 0, sizeof(app->tile_queues[i]));
        app->layer_expected[i] = 0;
        app->layer_done[i] = 0;
        app->layer_inflight[i] = 0;
        app->layer_visible_expected[i] = 0;
        app->layer_visible_loaded[i] = 0;
        app->layer_state[i] = LAYER_READINESS_HIDDEN;
        app->queue_band[i] = TILE_BAND_DEFAULT;
        app->layer_target_band[i] = TILE_BAND_DEFAULT;
    }
    for (size_t i = 0; i < TILE_BAND_COUNT; ++i) {
        app->band_visible_expected[i] = 0u;
        app->band_visible_loaded[i] = 0u;
        app->band_queue_depth[i] = 0u;
    }
    app->queue_valid = false;
    app->loading_layer_index = 0;
    app->active_layer_valid = false;
    app_vk_poly_prep_clear(app);
    app_vk_asset_queue_clear(app);
}

static void app_rebuild_tile_queue_for_kind(AppState *app, TileQueue *queue, TileLayerKind kind,
    uint16_t z, TileCoord top_left, TileCoord bottom_right, TileZoomBand band) {
    if (!app) {
        return;
    }

    uint32_t total_tiles = (bottom_right.x - top_left.x + 1) * (bottom_right.y - top_left.y + 1);
    if (total_tiles == 0) {
        if (queue) {
            queue->count = 0;
            queue->index = 0;
        }
        return;
    }

    if (!queue) {
        return;
    }

    if (total_tiles > queue->capacity) {
        TileQueueItem *items = (TileQueueItem *)realloc(queue->items, total_tiles * sizeof(TileQueueItem));
        if (!items) {
            queue->count = 0;
            queue->index = 0;
            return;
        }
        queue->items = items;
        queue->capacity = total_tiles;
    }

    TileCoord center = tile_from_meters(z, (MercatorMeters){app->camera.x, app->camera.y});
    uint32_t count = 0;
    for (uint32_t y = top_left.y; y <= bottom_right.y; ++y) {
        for (uint32_t x = top_left.x; x <= bottom_right.x; ++x) {
            TileCoord coord = {z, x, y};
            const MftTile *tile = tile_manager_peek_tile(&app->tile_managers[kind], coord, band);
            if (tile) {
                continue;
            }
            int dx = (int)x - (int)center.x;
            int dy = (int)y - (int)center.y;
            uint32_t dist2 = (uint32_t)(dx * dx + dy * dy);
            queue->items[count++] = (TileQueueItem){coord, dist2};
        }
    }

    if (count > 1) {
        qsort(queue->items, count, sizeof(TileQueueItem), app_tile_queue_compare);
    }

    queue->count = count;
    queue->index = 0;
    app->layer_expected[kind] = count;
    app->layer_done[kind] = 0;
    app->layer_inflight[kind] = 0;
}

static void app_process_tile_queue(AppState *app, TileQueue *queue, TileLayerKind kind, uint32_t budget) {
    if (!app || !queue || budget == 0 || queue->index >= queue->count) {
        return;
    }

    uint32_t remaining = queue->count - queue->index;
    uint32_t load_count = budget < remaining ? budget : remaining;
    for (uint32_t i = 0; i < load_count; ++i) {
        TileQueueItem item = queue->items[queue->index++];
        TileZoomBand band = app->queue_band[kind];
        if (!tile_loader_enqueue(&app->tile_loader, item.coord, kind, band, app->tile_request_id)) {
            queue->index -= 1;
            break;
        }
        app->layer_inflight[kind] += 1;
    }
}

void app_drain_tile_results(AppState *app, uint32_t budget) {
    if (!app || budget == 0) {
        return;
    }

    double start = time_now_seconds();
    for (uint32_t i = 0; i < budget; ++i) {
        if (i > 0 && (time_now_seconds() - start) >= APP_TILE_INTEGRATE_TIME_SLICE_SEC) {
            break;
        }
        TileResult result = {0};
        if (!tile_loader_pop_result(&app->tile_loader, &result)) {
            break;
        }

        if (result.request_id != app->tile_request_id) {
            if (result.ok) {
                mft_free_tile(&result.tile);
            }
            continue;
        }

        if (app->layer_inflight[result.kind] > 0) {
            app->layer_inflight[result.kind] -= 1;
        }
        if (app->layer_done[result.kind] < app->layer_expected[result.kind]) {
            app->layer_done[result.kind] += 1;
        }

        if (!result.ok) {
            continue;
        }

        if (app->vk_assets_enabled && app_kind_is_polygon(result.kind)) {
            if (!app_vk_poly_prep_enqueue(app, &result)) {
                if (!tile_manager_put_tile(&app->tile_managers[result.kind], result.coord, result.band, &result.tile)) {
                    mft_free_tile(&result.tile);
                    continue;
                }
                app_vk_asset_enqueue(app, result.kind, result.coord, result.band);
            }
            continue;
        }

        if (!tile_manager_put_tile(&app->tile_managers[result.kind], result.coord, result.band, &result.tile)) {
            mft_free_tile(&result.tile);
            continue;
        }

        if (app->vk_assets_enabled &&
            (result.kind == TILE_LAYER_ROAD_ARTERY ||
             result.kind == TILE_LAYER_ROAD_LOCAL)) {
            const MftTile *cached = tile_manager_peek_tile(&app->tile_managers[result.kind], result.coord, result.band);
            if (cached) {
                vk_tile_cache_on_tile_loaded(&app->vk_tile_cache, app->renderer.vk, result.kind, result.coord, result.band, cached);
            }
        }
    }
}

void app_refresh_layer_states(AppState *app) {
    if (!app) {
        return;
    }

    app_refresh_visible_layer_coverage(app);

    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy) {
            continue;
        }

        TileLayerKind kind = policy->kind;
        if (!app_layer_active_runtime(app, kind)) {
            app->layer_state[kind] = LAYER_READINESS_HIDDEN;
            continue;
        }

        uint32_t expected = app->layer_expected[kind];
        uint32_t done = app->layer_done[kind];
        uint32_t inflight = app->layer_inflight[kind];
        uint32_t visible_expected = app->layer_visible_expected[kind];
        uint32_t visible_loaded = app->layer_visible_loaded[kind];
        bool full_ready = layer_policy_requires_full_ready(kind);

        if (!full_ready) {
            if (visible_expected == 0u) {
                app->layer_state[kind] = LAYER_READINESS_READY;
            } else if (visible_loaded >= visible_expected) {
                app->layer_state[kind] = LAYER_READINESS_READY;
            } else {
                app->layer_state[kind] = LAYER_READINESS_LOADING;
            }
            continue;
        }

        if (expected > 0 && done >= expected && inflight == 0) {
            app->layer_state[kind] = LAYER_READINESS_READY;
            continue;
        }

        if (expected == 0 && inflight == 0) {
            app->layer_state[kind] = LAYER_READINESS_READY;
            continue;
        }

        if (expected == 0 && app->loading_no_data_time >= APP_TILE_NO_DATA_TIMEOUT) {
            app->layer_state[kind] = LAYER_READINESS_READY;
            continue;
        }

        app->layer_state[kind] = LAYER_READINESS_LOADING;
    }
}

void app_update_tile_queue(AppState *app) {
    if (!app) {
        return;
    }

    uint16_t z = 0;
    TileCoord top_left = {0};
    TileCoord bottom_right = {0};
    if (!app_compute_visible_tile_bounds(app, &z, &top_left, &bottom_right)) {
        app->visible_valid = false;
        app->loading_expected = 0;
        app->loading_done = 0;
        app_clear_tile_queue(app);
        return;
    }

    app->visible_zoom = z;
    app->visible_top_left = top_left;
    app->visible_bottom_right = bottom_right;
    app->visible_valid = true;
    app->visible_tile_count = (bottom_right.x - top_left.x + 1) * (bottom_right.y - top_left.y + 1);
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy) {
            continue;
        }
        app->layer_target_band[policy->kind] = app_layer_target_band(app, policy->kind);
    }

    bool bounds_changed = !app->queue_valid ||
        app->queue_zoom != z ||
        app->queue_top_left.x != top_left.x || app->queue_top_left.y != top_left.y ||
        app->queue_bottom_right.x != bottom_right.x || app->queue_bottom_right.y != bottom_right.y;
    if (!bounds_changed) {
        for (size_t i = 0; i < layer_policy_count(); ++i) {
            const LayerPolicy *policy = layer_policy_at(i);
            if (!policy) {
                continue;
            }
            if (app->queue_band[policy->kind] != app->layer_target_band[policy->kind]) {
                bounds_changed = true;
                break;
            }
        }
    }

    if (bounds_changed) {
        app->tile_request_id += 1;
        for (size_t i = 0; i < layer_policy_count(); ++i) {
            const LayerPolicy *policy = layer_policy_at(i);
            if (!policy) {
                continue;
            }
            TileLayerKind kind = policy->kind;
            TileZoomBand band = app->layer_target_band[kind];
            app->queue_band[kind] = band;
            app_rebuild_tile_queue_for_kind(app, &app->tile_queues[kind], kind, z, top_left, bottom_right, band);
        }
        uint32_t buffer = app->visible_tile_count;
        if (buffer < 64u) {
            buffer = 64u;
        }
        uint32_t target_capacity = app->visible_tile_count + buffer;
        for (size_t i = 0; i < layer_policy_count(); ++i) {
            const LayerPolicy *policy = layer_policy_at(i);
            if (!policy || !app_layer_active_runtime(app, policy->kind)) {
                continue;
            }
            tile_manager_ensure_capacity(&app->tile_managers[policy->kind], target_capacity);
        }
        app->queue_top_left = top_left;
        app->queue_bottom_right = bottom_right;
        app->queue_zoom = z;
        app->queue_valid = true;
        app->loading_layer_index = 0;
    }

    if (!app->queue_valid) {
        return;
    }

    for (size_t i = 0; i < TILE_BAND_COUNT; ++i) {
        app->band_queue_depth[i] = 0u;
    }
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy || !app_layer_active_runtime(app, policy->kind)) {
            continue;
        }
        TileQueue *queue = &app->tile_queues[policy->kind];
        uint32_t remaining = 0u;
        if (queue->count > queue->index) {
            remaining = queue->count - queue->index;
        }
        TileZoomBand band = app->queue_band[policy->kind];
        if ((size_t)band < TILE_BAND_COUNT) {
            app->band_queue_depth[band] += remaining + app->layer_inflight[policy->kind];
        }
    }

    app_refresh_layer_states(app);
    app->active_layer_valid = false;
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy || !app_layer_active_runtime(app, policy->kind)) {
            continue;
        }
        bool full_ready = layer_policy_requires_full_ready(policy->kind);
        if (full_ready) {
            if (app->layer_done[policy->kind] >= app->layer_expected[policy->kind] &&
                app->layer_inflight[policy->kind] == 0) {
                continue;
            }
        } else {
            if (app->layer_visible_loaded[policy->kind] >= app->layer_visible_expected[policy->kind]) {
                continue;
            }
        }
        if (!app->active_layer_valid) {
            app->active_layer_valid = true;
            app->active_layer_kind = policy->kind;
            app->active_layer_expected = full_ready
                ? app->layer_expected[policy->kind]
                : app->layer_visible_expected[policy->kind];
        }
        TileQueue *queue = &app->tile_queues[policy->kind];
        uint32_t expected = app->layer_expected[policy->kind];
        uint32_t budget = app_tile_load_budget(policy->kind, expected);
        if (policy->kind == TILE_LAYER_ROAD_ARTERY || policy->kind == TILE_LAYER_ROAD_LOCAL) {
            if (budget > 8u) {
                budget = 8u;
            }
        } else if (policy->kind == TILE_LAYER_POLY_WATER ||
                   policy->kind == TILE_LAYER_POLY_PARK ||
                   policy->kind == TILE_LAYER_POLY_LANDUSE ||
                   policy->kind == TILE_LAYER_POLY_BUILDING) {
            uint32_t polygon_cap = (policy->kind == TILE_LAYER_POLY_BUILDING) ? 2u : 1u;
            if (budget > polygon_cap) {
                budget = polygon_cap;
            }
        }
        app_process_tile_queue(app, queue, policy->kind, budget);
    }

    for (size_t i = 0; i < TILE_BAND_COUNT; ++i) {
        app->band_queue_depth[i] = 0u;
    }
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy || !app_layer_active_runtime(app, policy->kind)) {
            continue;
        }
        TileQueue *queue = &app->tile_queues[policy->kind];
        uint32_t remaining = 0u;
        if (queue->count > queue->index) {
            remaining = queue->count - queue->index;
        }
        TileZoomBand band = app->queue_band[policy->kind];
        if ((size_t)band < TILE_BAND_COUNT) {
            app->band_queue_depth[band] += remaining + app->layer_inflight[policy->kind];
        }
    }
}
