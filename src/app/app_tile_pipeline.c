#include "app/app_internal.h"

#include "core/time.h"
#include "map/mercator.h"

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
    (void)kind;
    return true;
}

bool app_layer_active_runtime(const AppState *app, TileLayerKind kind) {
    if (!app_layer_runtime_enabled(app, kind)) {
        return false;
    }
    return layer_policy_layer_active(kind, app->camera.zoom, app->building_zoom_bias);
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

    uint16_t z = app_zoom_to_tile_level(app->camera.zoom);
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
        app->layer_state[i] = LAYER_READINESS_HIDDEN;
    }
    app->queue_valid = false;
    app->loading_layer_index = 0;
    app->active_layer_valid = false;
}

static void app_rebuild_tile_queue_for_kind(AppState *app, TileQueue *queue, TileLayerKind kind,
    uint16_t z, TileCoord top_left, TileCoord bottom_right) {
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
            const MftTile *tile = tile_manager_peek_tile(&app->tile_managers[kind], coord);
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
        if (!tile_loader_enqueue(&app->tile_loader, item.coord, kind, app->tile_request_id)) {
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

        if (!tile_manager_put_tile(&app->tile_managers[result.kind], result.coord, &result.tile)) {
            mft_free_tile(&result.tile);
            continue;
        }

        if (app->vk_assets_enabled &&
            (result.kind == TILE_LAYER_ROAD_ARTERY ||
             result.kind == TILE_LAYER_ROAD_LOCAL)) {
            const MftTile *cached = tile_manager_peek_tile(&app->tile_managers[result.kind], result.coord);
            if (cached) {
                vk_tile_cache_on_tile_loaded(&app->vk_tile_cache, app->renderer.vk, result.kind, result.coord, cached);
            }
        }
    }
}

void app_refresh_layer_states(AppState *app) {
    if (!app) {
        return;
    }

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

    bool bounds_changed = !app->queue_valid ||
        app->queue_zoom != z ||
        app->queue_top_left.x != top_left.x || app->queue_top_left.y != top_left.y ||
        app->queue_bottom_right.x != bottom_right.x || app->queue_bottom_right.y != bottom_right.y;

    if (bounds_changed) {
        app->tile_request_id += 1;
        for (size_t i = 0; i < layer_policy_count(); ++i) {
            const LayerPolicy *policy = layer_policy_at(i);
            if (!policy) {
                continue;
            }
            TileLayerKind kind = policy->kind;
            app_rebuild_tile_queue_for_kind(app, &app->tile_queues[kind], kind, z, top_left, bottom_right);
        }
        uint32_t buffer = app->visible_tile_count / 2;
        if (buffer < 16) {
            buffer = 16;
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

    app_refresh_layer_states(app);
    app->active_layer_valid = false;
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy || !app_layer_active_runtime(app, policy->kind)) {
            continue;
        }
        if (app->layer_done[policy->kind] >= app->layer_expected[policy->kind] &&
            app->layer_inflight[policy->kind] == 0) {
            continue;
        }
        TileQueue *queue = &app->tile_queues[policy->kind];
        uint32_t expected = app->layer_expected[policy->kind];
        uint32_t budget = app_tile_load_budget(policy->kind, expected);
        app_process_tile_queue(app, queue, policy->kind, budget);
        app->active_layer_valid = true;
        app->active_layer_kind = policy->kind;
        app->active_layer_expected = expected;
        break;
    }
}
