#include "app/app.h"

#include "camera/camera.h"
#include "core/input.h"
#include "core/log.h"
#include "core/time.h"
#include "app/region.h"
#include "app/region_loader.h"
#include "map/polygon_renderer.h"
#include "map/layer_policy.h"
#include "map/road_renderer.h"
#include "map/tile_loader.h"
#include "map/tile_manager.h"
#include "map/tile_math.h"
#include "map/mercator.h"
#include "render/renderer.h"
#include "route/route.h"
#include "route/route_render.h"
#include "ui/debug_overlay.h"
#include "ui/font.h"

#include <SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const float kHeaderHeight = 34.0f;
static const uint32_t kTileIntegrateBudget = 8;
static const float kTileNoDataTimeout = 1.5f;

typedef struct TileQueueItem {
    TileCoord coord;
    uint32_t dist2;
} TileQueueItem;

typedef struct TileQueue {
    TileQueueItem *items;
    uint32_t count;
    uint32_t index;
    uint32_t capacity;
} TileQueue;

// Owns core application state for the main loop.
typedef struct AppState {
    SDL_Window *window;
    Renderer renderer;
    Camera camera;
    InputState input;
    DebugOverlay overlay;
    TileManager tile_managers[TILE_LAYER_COUNT];
    TileLoader tile_loader;
    bool single_line;
    RegionInfo region;
    int region_index;
    RouteState route;
    bool dragging_start;
    bool dragging_goal;
    bool has_hover;
    uint32_t hover_node;
    bool playback_playing;
    float playback_time_s;
    float playback_speed;
    bool show_landuse;
    float building_zoom_bias;
    bool building_fill_enabled;
    float road_zoom_bias;
    bool polygon_outline_only;
    uint32_t tile_request_id;
    TileQueue tile_queues[TILE_LAYER_COUNT];
    uint32_t layer_expected[TILE_LAYER_COUNT];
    uint32_t layer_done[TILE_LAYER_COUNT];
    uint32_t layer_inflight[TILE_LAYER_COUNT];
    LayerReadinessState layer_state[TILE_LAYER_COUNT];
    TileCoord queue_top_left;
    TileCoord queue_bottom_right;
    uint16_t queue_zoom;
    bool queue_valid;
    TileCoord visible_top_left;
    TileCoord visible_bottom_right;
    uint16_t visible_zoom;
    bool visible_valid;
    uint32_t loading_expected;
    uint32_t loading_done;
    float loading_no_data_time;
    size_t loading_layer_index;
    uint32_t visible_tile_count;
    TileLayerKind active_layer_kind;
    uint32_t active_layer_expected;
    bool active_layer_valid;
    int width;
    int height;
} AppState;

static const uint16_t kRegionMinZoom = 12;
static const uint16_t kRegionMaxZoom = 12;

static uint16_t app_zoom_to_tile_level(float zoom) {
    int level = (int)floorf(zoom + 0.5f);
    if (level < (int)kRegionMinZoom) {
        level = (int)kRegionMinZoom;
    }
    if (level > (int)kRegionMaxZoom) {
        level = (int)kRegionMaxZoom;
    }
    return (uint16_t)level;
}

static uint32_t app_sum_road_classes(const uint32_t *values, int first_class, int last_class) {
    if (!values || first_class < 0 || last_class < first_class) {
        return 0;
    }
    uint32_t sum = 0;
    for (int i = first_class; i <= last_class; ++i) {
        sum += values[i];
    }
    return sum;
}

static uint32_t app_tile_load_budget(uint32_t expected) {
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

static uint32_t app_tile_integrate_budget(uint32_t expected) {
    if (expected <= 32) {
        return 32;
    }
    if (expected <= 128) {
        return 48;
    }
    return 64;
}

static float clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float app_zoom_for_bounds(const Camera *camera, const RegionInfo *region, int screen_w, int screen_h, float padding) {
    if (!camera || !region || !region->has_bounds || screen_w <= 0 || screen_h <= 0) {
        return camera ? camera->zoom : 14.0f;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){region->min_lat, region->min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){region->max_lat, region->max_lon});

    double width = max_m.x - min_m.x;
    double height = max_m.y - min_m.y;
    if (width <= 0.0 || height <= 0.0) {
        return camera->zoom;
    }

    double target_w = (double)screen_w / (width * (double)padding);
    double target_h = (double)screen_h / (height * (double)padding);
    double ppm = target_w < target_h ? target_w : target_h;
    if (ppm <= 0.0) {
        return camera->zoom;
    }

    double world_size = mercator_world_size_meters();
    double zoom = log2(ppm * world_size / 256.0);
    return clampf((float)zoom, 10.0f, 18.0f);
}

static void app_center_camera_on_region(Camera *camera, const RegionInfo *region, int screen_w, int screen_h) {
    if (!camera || !region || !region->has_bounds) {
        return;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){region->min_lat, region->min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){region->max_lat, region->max_lon});

    camera->x = (float)((min_m.x + max_m.x) * 0.5);
    camera->y = (float)((min_m.y + max_m.y) * 0.5);
    camera->zoom = app_zoom_for_bounds(camera, region, screen_w, screen_h, 1.15f);
    camera->x_target = camera->x;
    camera->y_target = camera->y;
    camera->zoom_target = camera->zoom;
}

static float app_building_zoom_bias_for_region(const RegionInfo *region) {
    if (!region || !region->has_bounds) {
        return 0.0f;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){region->min_lat, region->min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){region->max_lat, region->max_lon});

    double width = max_m.x - min_m.x;
    double height = max_m.y - min_m.y;
    if (width <= 0.0 || height <= 0.0) {
        return 0.0f;
    }

    double span = width > height ? width : height;
    double base = 30000.0;
    double ratio = span / base;
    if (ratio < 1.0) {
        ratio = 1.0;
    }

    float bias = (float)log2(ratio) * 0.6f;
    if (bias < 0.0f) {
        bias = 0.0f;
    }
    if (bias > 1.5f) {
        bias = 1.5f;
    }
    return bias;
}

static float app_road_zoom_bias_for_region(const RegionInfo *region) {
    if (!region || !region->has_bounds) {
        return 0.0f;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){region->min_lat, region->min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){region->max_lat, region->max_lon});

    double width = max_m.x - min_m.x;
    double height = max_m.y - min_m.y;
    if (width <= 0.0 || height <= 0.0) {
        return 0.0f;
    }

    double span = width > height ? width : height;
    double base = 30000.0;
    double ratio = span / base;
    if (ratio < 1.0) {
        ratio = 1.0;
    }

    float bias = (float)log2(ratio) * 0.5f;
    if (bias < 0.0f) {
        bias = 0.0f;
    }
    if (bias > 2.0f) {
        bias = 2.0f;
    }
    return bias;
}

static float app_layer_zoom_start(const AppState *app, TileLayerKind kind) {
    return layer_policy_zoom_start(kind, app ? app->building_zoom_bias : 0.0f);
}

static bool app_layer_runtime_enabled(const AppState *app, TileLayerKind kind) {
    if (!app) {
        return false;
    }
    if (renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN &&
        kind == TILE_LAYER_POLY_BUILDING) {
        // Vulkan path temporarily disables buildings to keep frame/render stability.
        return false;
    }
    return true;
}

static bool app_layer_active_runtime(const AppState *app, TileLayerKind kind) {
    if (!app_layer_runtime_enabled(app, kind)) {
        return false;
    }
    return layer_policy_layer_active(kind, app->camera.zoom, app->building_zoom_bias);
}

static void app_update_vk_line_budget(AppState *app) {
    if (!app) {
        return;
    }
    if (renderer_get_backend(&app->renderer) != RENDERER_BACKEND_VULKAN) {
        return;
    }
    app->renderer.vk_line_budget = layer_policy_vk_line_budget(app->camera.zoom, app->visible_tile_count);
}

static void app_draw_region_bounds(AppState *app) {
    if (!app || !app->region.has_bounds) {
        return;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){app->region.min_lat, app->region.min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){app->region.max_lat, app->region.max_lon});

    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    camera_world_to_screen(&app->camera, (float)min_m.x, (float)max_m.y, app->width, app->height, &x0, &y0);
    camera_world_to_screen(&app->camera, (float)max_m.x, (float)min_m.y, app->width, app->height, &x1, &y1);

    float left = x0 < x1 ? x0 : x1;
    float right = x0 < x1 ? x1 : x0;
    float top = y0 < y1 ? y0 : y1;
    float bottom = y0 < y1 ? y1 : y0;

    renderer_set_draw_color(&app->renderer, 80, 140, 220, 200);
    SDL_FRect rect = {left, top, right - left, bottom - top};
    renderer_draw_rect(&app->renderer, &rect);
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

static void app_clear_tile_queue(AppState *app) {
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
            const MftTile *tile = NULL;
    tile = tile_manager_peek_tile(&app->tile_managers[kind], coord);
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

static void app_drain_tile_results(AppState *app, uint32_t budget) {
    if (!app || budget == 0) {
        return;
    }

    for (uint32_t i = 0; i < budget; ++i) {
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
        }
    }
}

static void app_refresh_layer_states(AppState *app) {
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

        // If the queue had nothing missing (everything already cached), this layer is ready now.
        if (expected == 0 && inflight == 0) {
            app->layer_state[kind] = LAYER_READINESS_READY;
            continue;
        }

        if (expected == 0 && app->loading_no_data_time >= kTileNoDataTimeout) {
            app->layer_state[kind] = LAYER_READINESS_READY;
            continue;
        }

        app->layer_state[kind] = LAYER_READINESS_LOADING;
    }
}

static void app_update_tile_queue(AppState *app) {
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
        uint32_t budget = app_tile_load_budget(expected);
        app_process_tile_queue(app, queue, policy->kind, budget);
        app->active_layer_valid = true;
        app->active_layer_kind = policy->kind;
        app->active_layer_expected = expected;
        break;
    }
}

static uint32_t app_draw_visible_tiles(AppState *app) {
    if (!app || !app->visible_valid) {
        return 0;
    }

    uint32_t visible = 0;
    bool vk_backend = renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN;
    uint32_t polygon_tile_budget = vk_backend ? 16u : UINT32_MAX;
    if (vk_backend && app->visible_tile_count > 40) {
        polygon_tile_budget = 8u;
    }
    if (vk_backend && app->visible_tile_count > 80) {
        polygon_tile_budget = 4u;
    }
    uint32_t total_line_budget = app->renderer.vk_line_budget;
    uint32_t reserve_line_budget = 0u;
    uint32_t road_line_budget = total_line_budget;
    if (vk_backend && total_line_budget > 0u) {
        reserve_line_budget = total_line_budget / 20u;
        if (reserve_line_budget < 1500u) {
            reserve_line_budget = 1500u;
        }
        if (reserve_line_budget > total_line_budget / 5u) {
            reserve_line_budget = total_line_budget / 5u;
        }
        uint32_t drawable_budget = total_line_budget > reserve_line_budget
            ? (total_line_budget - reserve_line_budget)
            : total_line_budget;
        float road_share = 0.82f;
        if (!app_layer_runtime_enabled(app, TILE_LAYER_POLY_BUILDING)) {
            road_share = 0.95f;
        }
        road_line_budget = (uint32_t)((float)drawable_budget * road_share);
        if (road_line_budget < drawable_budget / 2u) {
            road_line_budget = drawable_budget / 2u;
        }
        app->renderer.vk_line_budget = road_line_budget;
    }

    uint32_t polygon_tiles_drawn = 0;
    uint32_t expected = 0;
    uint32_t done = 0;
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy || !app_layer_active_runtime(app, policy->kind)) {
            continue;
        }
        expected += app->layer_expected[policy->kind];
        done += app->layer_done[policy->kind];
    }

    // Pass 1: roads first so polygons cannot starve transport visibility.
    for (uint32_t y = app->visible_top_left.y; y <= app->visible_bottom_right.y; ++y) {
        for (uint32_t x = app->visible_top_left.x; x <= app->visible_bottom_right.x; ++x) {
            TileCoord coord = {app->visible_zoom, x, y};
            bool local_ready = !layer_policy_requires_full_ready(TILE_LAYER_ROAD_LOCAL) ||
                app->layer_state[TILE_LAYER_ROAD_LOCAL] == LAYER_READINESS_READY;
            bool contour_ready = !layer_policy_requires_full_ready(TILE_LAYER_CONTOUR) ||
                app->layer_state[TILE_LAYER_CONTOUR] == LAYER_READINESS_READY;
            const MftTile *local = (app_layer_active_runtime(app, TILE_LAYER_ROAD_LOCAL) &&
                local_ready)
                ? tile_manager_peek_tile(&app->tile_managers[TILE_LAYER_ROAD_LOCAL], coord) : NULL;
            const MftTile *contour = (app_layer_active_runtime(app, TILE_LAYER_CONTOUR) &&
                contour_ready)
                ? tile_manager_peek_tile(&app->tile_managers[TILE_LAYER_CONTOUR], coord) : NULL;
            const MftTile *artery = tile_manager_peek_tile(&app->tile_managers[TILE_LAYER_ROAD_ARTERY], coord);
            if (local) {
                road_renderer_draw_tile(&app->renderer, &app->camera, local, app->single_line, app->road_zoom_bias);
                visible += 1;
            }
            if (artery) {
                road_renderer_draw_tile(&app->renderer, &app->camera, artery, app->single_line, app->road_zoom_bias);
                visible += 1;
            }
            if (contour) {
                // Phase A placeholder: contours are loaded/rendered through the polyline path.
                road_renderer_draw_tile(&app->renderer, &app->camera, contour, true, 0.0f);
                visible += 1;
            }
        }
    }

    if (vk_backend && total_line_budget > 0u) {
        uint32_t drawable_budget = total_line_budget > reserve_line_budget
            ? (total_line_budget - reserve_line_budget)
            : total_line_budget;
        uint32_t polygon_budget = drawable_budget > road_line_budget
            ? (drawable_budget - road_line_budget)
            : 0u;
        uint32_t polygon_cap = app->renderer.vk_lines_drawn + polygon_budget;
        if (polygon_cap > total_line_budget - reserve_line_budget) {
            polygon_cap = total_line_budget - reserve_line_budget;
        }
        if (polygon_cap < app->renderer.vk_lines_drawn) {
            polygon_cap = app->renderer.vk_lines_drawn;
        }
        app->renderer.vk_line_budget = polygon_cap;
    }

    // Pass 2: polygons after roads, with remaining budget only.
    for (uint32_t y = app->visible_top_left.y; y <= app->visible_bottom_right.y; ++y) {
        for (uint32_t x = app->visible_top_left.x; x <= app->visible_bottom_right.x; ++x) {
            TileCoord coord = {app->visible_zoom, x, y};
            bool allow_polygon_tile = polygon_tiles_drawn < polygon_tile_budget;
            bool water_ready = !layer_policy_requires_full_ready(TILE_LAYER_POLY_WATER) ||
                app->layer_state[TILE_LAYER_POLY_WATER] == LAYER_READINESS_READY;
            bool park_ready = !layer_policy_requires_full_ready(TILE_LAYER_POLY_PARK) ||
                app->layer_state[TILE_LAYER_POLY_PARK] == LAYER_READINESS_READY;
            bool landuse_ready = !layer_policy_requires_full_ready(TILE_LAYER_POLY_LANDUSE) ||
                app->layer_state[TILE_LAYER_POLY_LANDUSE] == LAYER_READINESS_READY;
            bool building_ready = !layer_policy_requires_full_ready(TILE_LAYER_POLY_BUILDING) ||
                app->layer_state[TILE_LAYER_POLY_BUILDING] == LAYER_READINESS_READY;
            const MftTile *water = (allow_polygon_tile &&
                app_layer_active_runtime(app, TILE_LAYER_POLY_WATER) &&
                water_ready)
                ? tile_manager_peek_tile(&app->tile_managers[TILE_LAYER_POLY_WATER], coord) : NULL;
            const MftTile *park = (allow_polygon_tile &&
                app_layer_active_runtime(app, TILE_LAYER_POLY_PARK) &&
                park_ready)
                ? tile_manager_peek_tile(&app->tile_managers[TILE_LAYER_POLY_PARK], coord) : NULL;
            const MftTile *landuse = (allow_polygon_tile &&
                app_layer_active_runtime(app, TILE_LAYER_POLY_LANDUSE) &&
                landuse_ready)
                ? tile_manager_peek_tile(&app->tile_managers[TILE_LAYER_POLY_LANDUSE], coord) : NULL;
            const MftTile *building = (allow_polygon_tile &&
                app_layer_active_runtime(app, TILE_LAYER_POLY_BUILDING) &&
                building_ready)
                ? tile_manager_peek_tile(&app->tile_managers[TILE_LAYER_POLY_BUILDING], coord) : NULL;
            if (water) {
                polygon_renderer_draw_tile(&app->renderer, &app->camera, (MftTile *)water, app->show_landuse,
                    app->building_zoom_bias, app->building_fill_enabled, app->polygon_outline_only);
                polygon_tiles_drawn += 1;
            }
            if (park) {
                polygon_renderer_draw_tile(&app->renderer, &app->camera, (MftTile *)park, app->show_landuse,
                    app->building_zoom_bias, app->building_fill_enabled, app->polygon_outline_only);
                polygon_tiles_drawn += 1;
            }
            if (landuse) {
                polygon_renderer_draw_tile(&app->renderer, &app->camera, (MftTile *)landuse, app->show_landuse,
                    app->building_zoom_bias, app->building_fill_enabled, app->polygon_outline_only);
                polygon_tiles_drawn += 1;
            }
            if (building) {
                polygon_renderer_draw_tile(&app->renderer, &app->camera, (MftTile *)building, app->show_landuse,
                    app->building_zoom_bias, app->building_fill_enabled, app->polygon_outline_only);
                polygon_tiles_drawn += 1;
            }
        }
    }

    if (vk_backend && total_line_budget > 0u) {
        app->renderer.vk_line_budget = total_line_budget;
    }

    app->loading_expected = expected;
    app->loading_done = done;

    return visible;
}

static bool app_load_route_graph(AppState *app) {
    if (!app) {
        return false;
    }

    char path[512];
    snprintf(path, sizeof(path), "data/regions/%s/graph/graph.bin", app->region.name);
    if (!route_state_load_graph(&app->route, path)) {
        log_error("Missing route graph for region: %s", app->region.name);
        return false;
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

static bool app_is_near_node(const AppState *app, float world_x, float world_y, uint32_t *out_node) {
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

static void app_update_hover(AppState *app) {
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

static bool app_mouse_over_node(const AppState *app, uint32_t node, float radius) {
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

static void app_draw_hover_marker(AppState *app) {
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

static SDL_FRect app_header_button_rect(const AppState *app) {
    float width = 140.0f;
    float height = 22.0f;
    float pad = 8.0f;
    SDL_FRect rect = {pad, (kHeaderHeight - height) * 0.5f, width, height};
    (void)app;
    return rect;
}

static bool app_header_button_hit(const AppState *app, int x, int y) {
    if (!app) {
        return false;
    }
    if (y < 0 || y > (int)kHeaderHeight) {
        return false;
    }
    SDL_FRect rect = app_header_button_rect(app);
    return x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h;
}

static const char *app_header_layer_chip_label(TileLayerKind kind) {
    switch (kind) {
        case TILE_LAYER_ROAD_ARTERY:
            return "ART";
        case TILE_LAYER_ROAD_LOCAL:
            return "LOC";
        case TILE_LAYER_POLY_WATER:
            return "WAT";
        case TILE_LAYER_POLY_PARK:
            return "PRK";
        case TILE_LAYER_POLY_LANDUSE:
            return "LND";
        case TILE_LAYER_POLY_BUILDING:
            return "BLD";
        default:
            return "---";
    }
}

static void app_draw_header_layer_chips(AppState *app, float left_limit, float box_y, float box_h, int text_h) {
    if (!app) {
        return;
    }

    // Order is from highest to lowest importance. We draw from this order so
    // critical transport layers remain visible when header space is tight.
    static const TileLayerKind kHeaderChipPriority[] = {
        TILE_LAYER_ROAD_ARTERY,
        TILE_LAYER_ROAD_LOCAL,
        TILE_LAYER_POLY_WATER,
        TILE_LAYER_POLY_PARK,
        TILE_LAYER_POLY_LANDUSE,
        TILE_LAYER_POLY_BUILDING
    };

    const float chip_w = 88.0f;
    const float chip_gap = 6.0f;
    const float right_pad = 8.0f;
    const float text_pad_x = 6.0f;
    const float progress_h = 3.0f;
    SDL_Color text_color = {225, 230, 240, 255};
    float available_w = ((float)app->width - right_pad) - left_limit;
    if (available_w < chip_w) {
        return;
    }
    int max_chips = (int)((available_w + chip_gap) / (chip_w + chip_gap));
    int total_chips = (int)(sizeof(kHeaderChipPriority) / sizeof(kHeaderChipPriority[0]));
    if (max_chips > total_chips) {
        max_chips = total_chips;
    }

    float cursor_right = (float)app->width - right_pad;

    for (int i = 0; i < max_chips; ++i) {
        TileLayerKind kind = kHeaderChipPriority[i];
        float chip_x = cursor_right - chip_w;
        if (chip_x < left_limit) {
            break;
        }

        uint32_t expected = app->layer_expected[kind];
        uint32_t done = app->layer_done[kind];
        bool runtime_active = app_layer_active_runtime(app, kind);
        bool is_ready = app->layer_state[kind] == LAYER_READINESS_READY;
        bool is_loading = app->layer_state[kind] == LAYER_READINESS_LOADING && expected > 0;

        if (!runtime_active || app->layer_state[kind] == LAYER_READINESS_HIDDEN) {
            renderer_set_draw_color(&app->renderer, 34, 38, 48, 200);
            renderer_fill_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
            renderer_set_draw_color(&app->renderer, 58, 64, 80, 220);
            renderer_draw_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
        } else if (is_loading) {
            renderer_set_draw_color(&app->renderer, 30, 42, 58, 220);
            renderer_fill_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
            renderer_set_draw_color(&app->renderer, 92, 146, 210, 230);
            renderer_draw_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
        } else if (is_ready) {
            renderer_set_draw_color(&app->renderer, 28, 50, 44, 220);
            renderer_fill_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
            renderer_set_draw_color(&app->renderer, 78, 170, 130, 230);
            renderer_draw_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
        } else {
            renderer_set_draw_color(&app->renderer, 30, 35, 46, 220);
            renderer_fill_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
            renderer_set_draw_color(&app->renderer, 80, 90, 110, 220);
            renderer_draw_rect(&app->renderer, &(SDL_FRect){chip_x, box_y, chip_w, box_h});
        }

        char chip_text[24];
        const char *tag = app_header_layer_chip_label(kind);
        if (is_loading) {
            snprintf(chip_text, sizeof(chip_text), "%s %u/%u", tag, done, expected);
        } else {
            snprintf(chip_text, sizeof(chip_text), "%s", tag);
        }

        ui_draw_text(&app->renderer,
                     (int)(chip_x + text_pad_x),
                     (int)(box_y + (box_h - (float)text_h) * 0.5f),
                     chip_text,
                     text_color,
                     1.0f);

        if (is_loading && expected > 0) {
            float progress = clampf((float)done / (float)expected, 0.0f, 1.0f);
            SDL_FRect bar_bg = {chip_x + text_pad_x, box_y + box_h - 6.0f, chip_w - text_pad_x * 2.0f, progress_h};
            renderer_set_draw_color(&app->renderer, 42, 56, 76, 220);
            renderer_fill_rect(&app->renderer, &bar_bg);
            SDL_FRect bar_fg = {bar_bg.x, bar_bg.y, bar_bg.w * progress, bar_bg.h};
            renderer_set_draw_color(&app->renderer, 110, 180, 255, 230);
            renderer_fill_rect(&app->renderer, &bar_fg);
        }

        cursor_right = chip_x - chip_gap;
    }
}

static void app_draw_header_bar(AppState *app) {
    if (!app) {
        return;
    }

    renderer_set_draw_color(&app->renderer, 18, 20, 28, 230);
    SDL_FRect bar = {0.0f, 0.0f, (float)app->width, kHeaderHeight};
    renderer_fill_rect(&app->renderer, &bar);

    SDL_FRect button = app_header_button_rect(app);
    renderer_set_draw_color(&app->renderer, 70, 80, 100, 220);
    renderer_draw_rect(&app->renderer, &button);

    SDL_FRect left = {button.x + 1.0f, button.y + 1.0f, (button.w * 0.5f) - 2.0f, button.h - 2.0f};
    SDL_FRect right = {button.x + button.w * 0.5f + 1.0f, button.y + 1.0f, (button.w * 0.5f) - 2.0f, button.h - 2.0f};

    if (app->route.mode == ROUTE_MODE_CAR) {
        renderer_set_draw_color(&app->renderer, 60, 140, 220, 220);
        renderer_fill_rect(&app->renderer, &left);
        renderer_set_draw_color(&app->renderer, 50, 70, 90, 200);
        renderer_fill_rect(&app->renderer, &right);
    } else {
        renderer_set_draw_color(&app->renderer, 50, 70, 90, 200);
        renderer_fill_rect(&app->renderer, &left);
        renderer_set_draw_color(&app->renderer, 60, 200, 130, 220);
        renderer_fill_rect(&app->renderer, &right);
    }

    SDL_Color label_color = {225, 230, 240, 255};
    int text_h = ui_font_line_height(1.0f);
    if (text_h > 0) {
        int car_w = ui_measure_text_width("CAR", 1.0f);
        int walk_w = ui_measure_text_width("WALK", 1.0f);
        int car_x = (int)(left.x + (left.w - car_w) * 0.5f);
        int walk_x = (int)(right.x + (right.w - walk_w) * 0.5f);
        int label_y = (int)(button.y + (button.h - (float)text_h) * 0.5f);
        ui_draw_text(&app->renderer, car_x, label_y, "CAR", label_color, 1.0f);
        ui_draw_text(&app->renderer, walk_x, label_y, "WALK", label_color, 1.0f);
    }

    float cursor_x = button.x + button.w + 12.0f;
    char speed_text[32];
    snprintf(speed_text, sizeof(speed_text), "Speed: %.1fx", app->playback_speed);
    char distance_text[48];
    char zoom_text[24];
    float km = 0.0f;
    float minutes = 0.0f;
    if (app->route.path.count > 1) {
        km = app->route.path.total_length_m / 1000.0f;
        minutes = app->route.path.total_time_s / 60.0f;
    }
    snprintf(distance_text, sizeof(distance_text), "Route: %.2f km | %.1f min", km, minutes);
    snprintf(zoom_text, sizeof(zoom_text), "Zoom: %.2f", app->camera.zoom);

    SDL_Color badge_fill = {30, 35, 46, 230};
    SDL_Color badge_outline = {80, 90, 110, 220};
    float pad_x = 8.0f;
    float pad_y = 3.0f;
    int speed_w = ui_measure_text_width(speed_text, 1.0f);
    int distance_w = ui_measure_text_width(distance_text, 1.0f);
    int zoom_w = ui_measure_text_width(zoom_text, 1.0f);
    float box_h = (float)text_h + pad_y * 2.0f;
    float box_y = (kHeaderHeight - box_h) * 0.5f;
    float speed_box_w = (float)speed_w + pad_x * 2.0f;
    SDL_FRect speed_box = {cursor_x, box_y, speed_box_w, box_h};
    renderer_set_draw_color(&app->renderer, badge_fill.r, badge_fill.g, badge_fill.b, badge_fill.a);
    renderer_fill_rect(&app->renderer, &speed_box);
    renderer_set_draw_color(&app->renderer, badge_outline.r, badge_outline.g, badge_outline.b, badge_outline.a);
    renderer_draw_rect(&app->renderer, &speed_box);
    ui_draw_text(&app->renderer, (int)(speed_box.x + pad_x), (int)(speed_box.y + (box_h - text_h) * 0.5f), speed_text, label_color, 1.0f);
    cursor_x += speed_box_w + 10.0f;

    float distance_box_w = (float)distance_w + pad_x * 2.0f;
    SDL_FRect distance_box = {cursor_x, box_y, distance_box_w, box_h};
    renderer_set_draw_color(&app->renderer, badge_fill.r, badge_fill.g, badge_fill.b, badge_fill.a);
    renderer_fill_rect(&app->renderer, &distance_box);
    renderer_set_draw_color(&app->renderer, badge_outline.r, badge_outline.g, badge_outline.b, badge_outline.a);
    renderer_draw_rect(&app->renderer, &distance_box);
    ui_draw_text(&app->renderer, (int)(distance_box.x + pad_x), (int)(distance_box.y + (box_h - text_h) * 0.5f), distance_text, label_color, 1.0f);

    cursor_x += distance_box_w + 10.0f;
    float zoom_box_w = (float)zoom_w + pad_x * 2.0f;
    SDL_FRect zoom_box = {cursor_x, box_y, zoom_box_w, box_h};
    renderer_set_draw_color(&app->renderer, badge_fill.r, badge_fill.g, badge_fill.b, badge_fill.a);
    renderer_fill_rect(&app->renderer, &zoom_box);
    renderer_set_draw_color(&app->renderer, badge_outline.r, badge_outline.g, badge_outline.b, badge_outline.a);
    renderer_draw_rect(&app->renderer, &zoom_box);
    ui_draw_text(&app->renderer, (int)(zoom_box.x + pad_x), (int)(zoom_box.y + (box_h - text_h) * 0.5f), zoom_text, label_color, 1.0f);
    cursor_x += zoom_box_w + 10.0f;

    app_draw_header_layer_chips(app, cursor_x + 8.0f, box_y, box_h, text_h);
}

static const char *app_layer_label(TileLayerKind kind) {
    return layer_policy_label(kind);
}

static const char *app_layer_runtime_state_label(const AppState *app, TileLayerKind kind) {
    if (!app) {
        return "off";
    }
    return app_layer_active_runtime(app, kind) ? "on" : "off";
}

static void app_draw_layer_debug(AppState *app) {
    if (!app || !app->overlay.enabled) {
        return;
    }

    SDL_Color color = {220, 230, 245, 255};
    int line_h = ui_font_line_height(1.0f);
    if (line_h <= 0) {
        return;
    }

    int x = 10;
    int y = (int)kHeaderHeight + 6;
    char line[128];

    snprintf(line, sizeof(line), "Visible tiles: %u", app->visible_tile_count);
    ui_draw_text(&app->renderer, x, y, line, color, 1.0f);
    y += line_h + 2;

    if (app->active_layer_valid) {
        snprintf(line, sizeof(line), "Active layer: %s", app_layer_label(app->active_layer_kind));
    } else {
        snprintf(line, sizeof(line), "Active layer: none");
    }
    ui_draw_text(&app->renderer, x, y, line, color, 1.0f);
    y += line_h + 4;

    snprintf(line, sizeof(line), "Load total: %u/%u no_data=%.1fs",
             app->loading_done, app->loading_expected, app->loading_no_data_time);
    ui_draw_text(&app->renderer, x, y, line, color, 1.0f);
    y += line_h + 4;

    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy) {
            continue;
        }
        TileLayerKind kind = policy->kind;
        float start = app_layer_zoom_start(app, kind);
        snprintf(line, sizeof(line), "%s z>=%.2f exp %u done %u in %u state=%s runtime=%s",
                 app_layer_label(kind),
                 start,
                 app->layer_expected[kind],
                 app->layer_done[kind],
                 app->layer_inflight[kind],
                 layer_policy_readiness_label(app->layer_state[kind]),
                 app_layer_runtime_state_label(app, kind));
        ui_draw_text(&app->renderer, x, y, line, color, 1.0f);
        y += line_h + 2;
    }
}

static void app_copy_overlay_text(AppState *app) {
    if (!app) {
        return;
    }

    char buffer[2048];
    size_t offset = 0;
    int written = snprintf(buffer + offset, sizeof(buffer) - offset,
                           "Region: %s\nZoom: %.2f\nVisible tiles: %u\nLoad total: %u/%u no_data=%.1fs\n",
                           app->region.name,
                           app->camera.zoom,
                           app->visible_tile_count,
                           app->loading_done,
                           app->loading_expected,
                           app->loading_no_data_time);
    if (written < 0) {
        return;
    }
    offset += (size_t)written;

    if (app->active_layer_valid) {
        written = snprintf(buffer + offset, sizeof(buffer) - offset,
                           "Active layer: %s\n",
                           app_layer_label(app->active_layer_kind));
    } else {
        written = snprintf(buffer + offset, sizeof(buffer) - offset, "Active layer: none\n");
    }
    if (written < 0) {
        return;
    }
    offset += (size_t)written;

    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy) {
            continue;
        }
        TileLayerKind kind = policy->kind;
        float start = app_layer_zoom_start(app, kind);
        written = snprintf(buffer + offset, sizeof(buffer) - offset,
                           "%s z>=%.2f exp %u done %u in %u state=%s runtime=%s\n",
                           app_layer_label(kind),
                           start,
                           app->layer_expected[kind],
                           app->layer_done[kind],
                           app->layer_inflight[kind],
                           layer_policy_readiness_label(app->layer_state[kind]),
                           app_layer_runtime_state_label(app, kind));
        if (written < 0) {
            return;
        }
        offset += (size_t)written;
        if (offset >= sizeof(buffer)) {
            break;
        }
    }

    SDL_SetClipboardText(buffer);
}

static void app_playback_reset(AppState *app) {
    if (!app) {
        return;
    }
    app->playback_time_s = 0.0f;
    app->playback_playing = false;
}

static void app_playback_update(AppState *app, float dt) {
    if (!app || !app->playback_playing || app->route.path.count < 2 || app->route.path.total_time_s <= 0.0f) {
        return;
    }

    app->playback_time_s += dt * app->playback_speed;
    if (app->playback_time_s >= app->route.path.total_time_s) {
        app->playback_time_s = app->route.path.total_time_s;
        app->playback_playing = false;
    }
    if (app->playback_time_s < 0.0f) {
        app->playback_time_s = 0.0f;
    }
}

static bool app_playback_position(const AppState *app, float *out_x, float *out_y) {
    if (!app || !out_x || !out_y || app->route.path.count < 2 ||
        !app->route.path.cumulative_time_s || app->route.path.total_time_s <= 0.0f) {
        return false;
    }

    float t = app->playback_time_s;
    if (t <= 0.0f) {
        uint32_t node = app->route.path.nodes[0];
        *out_x = (float)app->route.graph.node_x[node];
        *out_y = (float)app->route.graph.node_y[node];
        return true;
    }
    if (t >= app->route.path.total_time_s) {
        uint32_t node = app->route.path.nodes[app->route.path.count - 1];
        *out_x = (float)app->route.graph.node_x[node];
        *out_y = (float)app->route.graph.node_y[node];
        return true;
    }

    uint32_t segment = 0;
    for (uint32_t i = 0; i + 1 < app->route.path.count; ++i) {
        if (t <= app->route.path.cumulative_time_s[i + 1]) {
            segment = i;
            break;
        }
    }

    float t0 = app->route.path.cumulative_time_s[segment];
    float t1 = app->route.path.cumulative_time_s[segment + 1];
    float denom = t1 - t0;
    float alpha = denom > 0.0001f ? (t - t0) / denom : 0.0f;

    uint32_t a = app->route.path.nodes[segment];
    uint32_t b = app->route.path.nodes[segment + 1];
    float ax = (float)app->route.graph.node_x[a];
    float ay = (float)app->route.graph.node_y[a];
    float bx = (float)app->route.graph.node_x[b];
    float by = (float)app->route.graph.node_y[b];

    *out_x = ax + (bx - ax) * alpha;
    *out_y = ay + (by - ay) * alpha;
    return true;
}

static void app_draw_playback_marker(AppState *app) {
    if (!app) {
        return;
    }

    float wx = 0.0f;
    float wy = 0.0f;
    if (!app_playback_position(app, &wx, &wy)) {
        return;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->camera, wx, wy, app->width, app->height, &sx, &sy);
    renderer_set_draw_color(&app->renderer, 255, 230, 80, 240);
    SDL_FRect rect = {sx - 4.0f, sy - 4.0f, 8.0f, 8.0f};
    renderer_fill_rect(&app->renderer, &rect);
}

static void app_draw_route_panel(AppState *app) {
    if (!app || app->route.path.count < 2) {
        return;
    }

    float panel_w = 220.0f;
    float panel_h = 36.0f;
    float pad = 10.0f;
    SDL_FRect panel = {pad, kHeaderHeight + pad, panel_w, panel_h};
    renderer_set_draw_color(&app->renderer, 20, 26, 36, 210);
    renderer_fill_rect(&app->renderer, &panel);
    renderer_set_draw_color(&app->renderer, 80, 90, 110, 220);
    renderer_draw_rect(&app->renderer, &panel);

    if (app->route.path.total_time_s > 0.0f) {
        float progress = app->playback_time_s / app->route.path.total_time_s;
        if (progress < 0.0f) {
            progress = 0.0f;
        } else if (progress > 1.0f) {
            progress = 1.0f;
        }
        SDL_FRect bar = {panel.x + 8.0f, panel.y + panel.h - 10.0f, (panel.w - 16.0f) * progress, 4.0f};
        renderer_set_draw_color(&app->renderer, 80, 170, 255, 220);
        renderer_fill_rect(&app->renderer, &bar);
    }
}

static bool app_recompute_route(AppState *app) {
    if (!app || !app->route.has_start || !app->route.has_goal) {
        return false;
    }

    bool ok = route_state_route(&app->route, app->route.start_node, app->route.goal_node);
    if (ok) {
        app_playback_reset(app);
    }
    return ok;
}

static float app_next_playback_speed(float current, int direction) {
    static const float kSpeeds[] = {1.0f, 2.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f};
    size_t count = sizeof(kSpeeds) / sizeof(kSpeeds[0]);
    size_t idx = 0;
    for (size_t i = 0; i < count; ++i) {
        if (fabsf(kSpeeds[i] - current) < 0.01f) {
            idx = i;
            break;
        }
    }
    if (direction > 0 && idx + 1 < count) {
        idx += 1;
    } else if (direction < 0 && idx > 0) {
        idx -= 1;
    }
    return kSpeeds[idx];
}

static bool app_init(AppState *app) {
    if (!app) {
        return false;
    }

    app->width = 1280;
    app->height = 720;
    renderer_set_backend(&app->renderer, RENDERER_BACKEND_SDL);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        log_error("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    const char *backend_env = getenv("MAPFORGE_RENDER_BACKEND");
    if (backend_env && strcmp(backend_env, "vulkan") == 0) {
        renderer_set_backend(&app->renderer, RENDERER_BACKEND_VULKAN);
    }
    log_info("Requested render backend env='%s' resolved='%s'",
             backend_env ? backend_env : "",
             renderer_backend_name(renderer_get_backend(&app->renderer)));

    uint32_t window_flags = SDL_WINDOW_SHOWN;
    if (renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN) {
        window_flags |= SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    }

    app->window = SDL_CreateWindow(
        "MapForge",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        app->width,
        app->height,
        (int)window_flags
    );

    if (!app->window) {
        log_error("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    if (!renderer_init(&app->renderer, app->window, app->width, app->height)) {
        if (renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN) {
            log_error("renderer_init vulkan path failed, retrying SDL fallback: %s", SDL_GetError());
            SDL_DestroyWindow(app->window);
            app->window = SDL_CreateWindow(
                "MapForge",
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                app->width,
                app->height,
                SDL_WINDOW_SHOWN
            );
            if (!app->window) {
                log_error("SDL_CreateWindow fallback failed: %s", SDL_GetError());
                return false;
            }
            renderer_set_backend(&app->renderer, RENDERER_BACKEND_SDL);
            if (!renderer_init(&app->renderer, app->window, app->width, app->height)) {
                log_error("renderer_init SDL fallback failed: %s", SDL_GetError());
                return false;
            }
        } else {
            log_error("renderer_init failed: %s", SDL_GetError());
            return false;
        }
    }
    log_info("Render backend: %s (vulkan_available=%s)",
             renderer_backend_name(renderer_get_backend(&app->renderer)),
             app->renderer.vulkan_available ? "yes" : "no");

    if (TTF_Init() != 0) {
        log_error("TTF_Init failed: %s", TTF_GetError());
        return false;
    }
    ui_font_set("assets/fonts/Montserrat-Regular.ttf", 10);

    app->region_index = 0;
    const RegionInfo *info = region_get(app->region_index);
    if (!info) {
        log_error("No region configured");
        return false;
    }

    app->region = *info;
    region_load_meta(info, &app->region);

    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        if (!tile_manager_init(&app->tile_managers[i], 128, app->region.tiles_dir)) {
            log_error("tile_manager_init failed");
            return false;
        }
    }
    if (!tile_loader_init(&app->tile_loader, app->region.tiles_dir)) {
        log_error("tile_loader_init failed");
        return false;
    }

    input_init(&app->input);
    camera_init(&app->camera);
    if (app->region.has_center) {
        MercatorMeters center = mercator_from_latlon((LatLon){app->region.center_lat, app->region.center_lon});
        app->camera.x = (float)center.x;
        app->camera.y = (float)center.y;
    }
    app_center_camera_on_region(&app->camera, &app->region, app->width, app->height);
    debug_overlay_init(&app->overlay);
    app->single_line = false;
    route_state_init(&app->route);
    app_load_route_graph(app);
    app->dragging_start = false;
    app->dragging_goal = false;
    app->has_hover = false;
    app->playback_playing = false;
    app->playback_time_s = 0.0f;
    app->playback_speed = 1.0f;
    app->show_landuse = false;
    app->building_zoom_bias = app_building_zoom_bias_for_region(&app->region);
    app->building_fill_enabled = false;
    app->road_zoom_bias = app_road_zoom_bias_for_region(&app->region);
    app->polygon_outline_only = false;
    app->tile_request_id = 1;
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        memset(&app->tile_queues[i], 0, sizeof(app->tile_queues[i]));
    }
    app->queue_valid = false;
    app->visible_valid = false;
    app->loading_expected = 0;
    app->loading_done = 0;
    app->loading_no_data_time = 0.0f;
    app->loading_layer_index = 0;

    return true;
}

static void app_shutdown(AppState *app) {
    if (!app) {
        return;
    }

    if (TTF_WasInit()) {
        ui_font_shutdown(&app->renderer);
        TTF_Quit();
    }

    renderer_shutdown(&app->renderer);
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        tile_manager_shutdown(&app->tile_managers[i]);
    }
    tile_loader_shutdown(&app->tile_loader);
    route_state_shutdown(&app->route);
    app_clear_tile_queue(app);

    if (app->window) {
        SDL_DestroyWindow(app->window);
        app->window = NULL;
    }

    SDL_Quit();
}

int app_run(void) {
    AppState app = {0};
    if (!app_init(&app)) {
        app_shutdown(&app);
        return 1;
    }

    double last_time = time_now_seconds();
    double perf_next_log = last_time + 1.0;
    uint32_t last_loading_done = 0;
    double last_loading_progress_time = last_time;
    RendererBackend last_backend = renderer_get_backend(&app.renderer);

    while (!app.input.quit) {
        double frame_begin = time_now_seconds();
        input_begin_frame(&app.input);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input_handle_event(&app.input, &event);
        }
        double after_events = time_now_seconds();

        if (app.input.toggle_debug_pressed) {
            app.overlay.enabled = !app.overlay.enabled;
        }
        if (app.input.toggle_single_line_pressed) {
            app.single_line = !app.single_line;
        }
        if (app.input.toggle_region_pressed) {
            app.region_index = (app.region_index + 1) % region_count();
            const RegionInfo *info = region_get(app.region_index);
            if (info) {
                app.region = *info;
                region_load_meta(info, &app.region);
                for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
                    tile_manager_shutdown(&app.tile_managers[i]);
                    tile_manager_init(&app.tile_managers[i], 128, app.region.tiles_dir);
                }
                tile_loader_shutdown(&app.tile_loader);
                tile_loader_init(&app.tile_loader, app.region.tiles_dir);
                app_clear_tile_queue(&app);
                app.visible_valid = false;
                app.loading_expected = 0;
                app.loading_done = 0;
                app.loading_no_data_time = 0.0f;
                app.tile_request_id += 1;
                app_load_route_graph(&app);
                route_state_clear(&app.route);
                app_playback_reset(&app);
                app.building_zoom_bias = app_building_zoom_bias_for_region(&app.region);
                app.road_zoom_bias = app_road_zoom_bias_for_region(&app.region);
                app_center_camera_on_region(&app.camera, &app.region, app.width, app.height);
            }
        }
        if (app.input.toggle_profile_pressed) {
            app.route.fastest = !app.route.fastest;
            if (app.route.has_start && app.route.has_goal) {
                app_recompute_route(&app);
            }
        }
        if (app.input.toggle_landuse_pressed) {
            app.show_landuse = !app.show_landuse;
        }
        if (app.input.toggle_building_fill_pressed) {
            app.building_fill_enabled = !app.building_fill_enabled;
        }
        if (app.input.toggle_polygon_outline_pressed) {
            app.polygon_outline_only = !app.polygon_outline_only;
        }
        if (app.input.toggle_playback_pressed && app.route.path.count >= 2) {
            app.playback_playing = !app.playback_playing;
        }
        if (app.input.playback_step_forward && app.route.path.total_time_s > 0.0f) {
            app.playback_time_s += 5.0f;
            if (app.playback_time_s > app.route.path.total_time_s) {
                app.playback_time_s = app.route.path.total_time_s;
            }
        }
        if (app.input.playback_step_back && app.route.path.total_time_s > 0.0f) {
            app.playback_time_s -= 5.0f;
            if (app.playback_time_s < 0.0f) {
                app.playback_time_s = 0.0f;
            }
        }
        if (app.input.playback_speed_up) {
            app.playback_speed = app_next_playback_speed(app.playback_speed, 1);
        }
        if (app.input.playback_speed_down) {
            app.playback_speed = app_next_playback_speed(app.playback_speed, -1);
        }

        double now = time_now_seconds();
        float dt = (float)(now - last_time);
        last_time = now;

        if (!app.dragging_start && !app.dragging_goal && app.input.left_click_pressed) {
            bool over_start = app.route.has_start && app_mouse_over_node(&app, app.route.start_node, 7.0f);
            bool over_goal = app.route.has_goal && app_mouse_over_node(&app, app.route.goal_node, 7.0f);
            if (over_goal && !over_start) {
                app.dragging_goal = true;
            } else if (over_start) {
                app.dragging_start = true;
            }
        }
        if (!app.dragging_goal && app.input.right_click_pressed && app.route.has_goal &&
            app_mouse_over_node(&app, app.route.goal_node, 7.0f)) {
            app.dragging_goal = true;
        }

        bool over_start = app.route.has_start && app_mouse_over_node(&app, app.route.start_node, 7.0f);
        bool over_goal = app.route.has_goal && app_mouse_over_node(&app, app.route.goal_node, 7.0f);
        bool allow_mouse_pan = !(app.dragging_start || app.dragging_goal) &&
            !((app.input.mouse_buttons & SDL_BUTTON_LMASK) && (over_start || over_goal));
        camera_handle_input(&app.camera, &app.input, app.width, app.height, dt, allow_mouse_pan);
        camera_update(&app.camera, dt);
        debug_overlay_update(&app.overlay, dt);

        app_update_hover(&app);
        app_update_tile_queue(&app);
        uint32_t integrate_budget = app.active_layer_valid
            ? app_tile_integrate_budget(app.active_layer_expected)
            : kTileIntegrateBudget;
        app_drain_tile_results(&app, integrate_budget);
        app_refresh_layer_states(&app);
        app_update_vk_line_budget(&app);
        if (app.input.copy_overlay_pressed) {
            app_copy_overlay_text(&app);
        }
        double after_update = time_now_seconds();

        bool consumed_click = false;
        if (app.input.left_click_pressed && app_header_button_hit(&app, app.input.mouse_x, app.input.mouse_y)) {
            app.route.mode = (app.route.mode == ROUTE_MODE_CAR) ? ROUTE_MODE_WALK : ROUTE_MODE_CAR;
            if (app.route.has_start && app.route.has_goal) {
                app_recompute_route(&app);
            }
            consumed_click = true;
        } else if ((app.input.left_click_pressed || app.input.right_click_pressed || app.input.middle_click_pressed) &&
                   app.input.mouse_y <= (int)kHeaderHeight) {
            consumed_click = true;
        }

        if (!consumed_click && (app.input.left_click_pressed || app.input.right_click_pressed || app.input.middle_click_pressed)) {
            if (app.input.middle_click_pressed) {
                route_state_clear(&app.route);
                app_playback_reset(&app);
                app.dragging_start = false;
                app.dragging_goal = false;
            } else if (app.route.loaded) {
                float world_x = 0.0f;
                float world_y = 0.0f;
                camera_screen_to_world(&app.camera, (float)app.input.mouse_x, (float)app.input.mouse_y, app.width, app.height, &world_x, &world_y);
                if (app.input.left_click_pressed) {
                    if (app.has_hover && app.route.has_start && app.hover_node == app.route.start_node) {
                        app.dragging_start = true;
                    } else if (app.input.shift_down) {
                        uint32_t node = 0;
                        if (app_is_near_node(&app, world_x, world_y, &node)) {
                            app.route.start_node = node;
                            app.route.has_start = true;
                        }
                    }
                }
                if (app.input.right_click_pressed) {
                    if (app.has_hover && app.route.has_goal && app.hover_node == app.route.goal_node) {
                        app.dragging_goal = true;
                    } else {
                        uint32_t node = 0;
                        if (app_is_near_node(&app, world_x, world_y, &node)) {
                            app.route.goal_node = node;
                            app.route.has_goal = true;
                        }
                    }
                }
            }
        }
        if (app.input.enter_pressed && app.route.has_start && app.route.has_goal) {
            app_recompute_route(&app);
        }

        if (app.dragging_start || app.dragging_goal) {
            uint32_t node = 0;
            if (app.has_hover) {
                node = app.hover_node;
            }
            if (app.has_hover) {
                bool changed = false;
                if (app.dragging_start && node != app.route.start_node) {
                    app.route.start_node = node;
                    app.route.has_start = true;
                    changed = true;
                }
                if (app.dragging_goal && node != app.route.goal_node) {
                    app.route.goal_node = node;
                    app.route.has_goal = true;
                    changed = true;
                }
                if (changed && app.route.has_start && app.route.has_goal) {
                    app_recompute_route(&app);
                }
            }
        }

        if (app.input.left_click_released) {
            if (app.dragging_start) {
                app.dragging_start = false;
                if (app.route.has_start && app.route.has_goal) {
                    app_recompute_route(&app);
                }
            }
            if (app.dragging_goal) {
                app.dragging_goal = false;
                if (app.route.has_start && app.route.has_goal) {
                    app_recompute_route(&app);
                }
            }
        }
        if (app.input.right_click_released) {
            if (app.dragging_goal) {
                app.dragging_goal = false;
                if (app.route.has_start && app.route.has_goal) {
                    app_recompute_route(&app);
                }
            }
        }

        app_playback_update(&app, dt);

        renderer_begin_frame(&app.renderer);
        RendererBackend frame_backend = renderer_get_backend(&app.renderer);
        if (frame_backend != last_backend) {
            ui_font_invalidate_cache(&app.renderer);
            last_backend = frame_backend;
        }
        renderer_clear(&app.renderer, 20, 20, 28, 255);
        road_renderer_stats_reset();
        uint32_t visible_tiles = 0;
        visible_tiles = app_draw_visible_tiles(&app);
        RoadRenderStats road_stats = {0};
        road_renderer_stats_get(&road_stats);
        if (app.loading_expected > 0 && app.loading_done == 0) {
            app.loading_no_data_time += dt;
        } else {
            app.loading_no_data_time = 0.0f;
        }
        app_draw_region_bounds(&app);
        route_render_draw(&app.renderer, &app.camera, &app.route.graph, &app.route.path, &app.route.drive_path, &app.route.walk_path,
            app.route.has_start, app.route.start_node,
            app.route.has_goal, app.route.goal_node,
            app.route.has_transfer, app.route.transfer_node);
        app_draw_hover_marker(&app);
        app_draw_playback_marker(&app);
        app_draw_route_panel(&app);
        app_draw_header_bar(&app);
        app_draw_layer_debug(&app);
        debug_overlay_render(&app.overlay, &app.renderer);
        renderer_end_frame(&app.renderer);
        double after_render = time_now_seconds();

        app.overlay.visible_tiles = visible_tiles;
        uint32_t cached_total = 0;
        uint32_t capacity_total = 0;
        for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
            cached_total += tile_manager_count(&app.tile_managers[i]);
            capacity_total += tile_manager_capacity(&app.tile_managers[i]);
        }
        app.overlay.cached_tiles = cached_total;
        app.overlay.cache_capacity = capacity_total;

        if (app.overlay.enabled) {
            char title[128];
            const char *profile = app.route.fastest ? "fastest" : "shortest";
            const char *tier = road_renderer_zoom_tier_label(app.camera.zoom);
            const char *mode = app.route.mode == ROUTE_MODE_CAR ? "car" : "walk";
            if (app.route.path.count > 1) {
                float km = app.route.path.total_length_m / 1000.0f;
                float minutes = app.route.path.total_time_s / 60.0f;
                if (app.route.drive_path.count > 1 && app.route.walk_path.count > 1) {
                    float drive_minutes = app.route.drive_path.total_time_s / 60.0f;
                    float walk_minutes = app.route.walk_path.total_time_s / 60.0f;
                    snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %.2f km | drive %.1f min + walk %.1f min | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                             app.region.name, mode, profile, tier, km, drive_minutes, walk_minutes, app.playback_speed, app.overlay.fps, app.overlay.visible_tiles, app.overlay.cached_tiles, app.overlay.cache_capacity);
                } else {
                    snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %.2f km | %.1f min | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                             app.region.name, mode, profile, tier, km, minutes, app.playback_speed, app.overlay.fps, app.overlay.visible_tiles, app.overlay.cached_tiles, app.overlay.cache_capacity);
                }
            } else {
                const char *graph_status = app.route.loaded ? "graph ok" : "graph missing";
                snprintf(title, sizeof(title), "MapForge | %s | %s | %s | %s | %s | %.1fx | FPS: %.1f | Tiles: %u | Cache: %u/%u",
                         app.region.name, mode, profile, tier, graph_status, app.playback_speed, app.overlay.fps, app.overlay.visible_tiles, app.overlay.cached_tiles, app.overlay.cache_capacity);
            }
            SDL_SetWindowTitle(app.window, title);
        } else {
            SDL_SetWindowTitle(app.window, "MapForge");
        }

        if (app.loading_done != last_loading_done) {
            last_loading_done = app.loading_done;
            last_loading_progress_time = after_render;
        }

        double frame_ms = (after_render - frame_begin) * 1000.0;
        double events_ms = (after_events - frame_begin) * 1000.0;
        double update_ms = (after_update - after_events) * 1000.0;
        double render_ms = (after_render - after_update) * 1000.0;
        bool long_frame = frame_ms >= 120.0;
        bool stuck_loading = app.loading_expected > 0 &&
            app.loading_done < app.loading_expected &&
            (after_render - last_loading_progress_time) >= 1.5;
        if (long_frame || stuck_loading || after_render >= perf_next_log) {
            TileLoaderStats stats = {0};
            tile_loader_get_stats(&app.tile_loader, &stats);
            if (renderer_get_backend(&app.renderer) == RENDERER_BACKEND_VULKAN) {
                uint32_t drawn_major = app_sum_road_classes(road_stats.drawn_by_class, ROAD_CLASS_MOTORWAY, ROAD_CLASS_TERTIARY);
                uint32_t drawn_local = app_sum_road_classes(road_stats.drawn_by_class, ROAD_CLASS_RESIDENTIAL, ROAD_CLASS_SERVICE);
                uint32_t drawn_path = app_sum_road_classes(road_stats.drawn_by_class, ROAD_CLASS_FOOTWAY, ROAD_CLASS_PATH);
                uint32_t filt_major = app_sum_road_classes(road_stats.filtered_by_class, ROAD_CLASS_MOTORWAY, ROAD_CLASS_TERTIARY);
                uint32_t filt_local = app_sum_road_classes(road_stats.filtered_by_class, ROAD_CLASS_RESIDENTIAL, ROAD_CLASS_SERVICE);
                uint32_t filt_path = app_sum_road_classes(road_stats.filtered_by_class, ROAD_CLASS_FOOTWAY, ROAD_CLASS_PATH);
                log_info("perf region=%s backend=vk frame=%.1fms events=%.1f update=%.1f render=%.1f zoom=%.2f vis=%u load=%u/%u active=%s "
                         "req=%u/%u res=%u/%u enq=%llu drop=%llu out=%llu out_drop=%llu miss=%llu ok=%llu fail=%llu "
                         "vk_begin=%d vk_begin_fail_total=%llu vk_recreate=%u vk_geom=%u/%u vk_geom_skip=%u vk_lines=%u vk_line_skip=%u vk_line_budget=%u vk_rect=%u vk_fill=%u "
                         "road_draw(m=%u l=%u p=%u) road_filter(m=%u l=%u p=%u)",
                         app.region.name,
                         frame_ms, events_ms, update_ms, render_ms,
                         app.camera.zoom,
                         app.visible_tile_count,
                         app.loading_done, app.loading_expected,
                         app.active_layer_valid ? app_layer_label(app.active_layer_kind) : "none",
                         stats.req_count, stats.req_capacity,
                         stats.res_count, stats.res_capacity,
                         (unsigned long long)stats.enqueued_count,
                         (unsigned long long)stats.enqueue_drop_count,
                         (unsigned long long)stats.produced_count,
                         (unsigned long long)stats.result_drop_count,
                         (unsigned long long)stats.missing_count,
                         (unsigned long long)stats.load_ok_count,
                         (unsigned long long)stats.load_fail_count,
                         app.renderer.vk_last_begin_result,
                         (unsigned long long)app.renderer.vk_begin_failures_total,
                         app.renderer.vk_swapchain_recreates,
                         app.renderer.vk_geom_used,
                         app.renderer.vk_geom_budget,
                         app.renderer.vk_geom_budget_skips,
                         app.renderer.vk_lines_drawn,
                         app.renderer.vk_line_budget_skips,
                         app.renderer.vk_line_budget,
                         app.renderer.vk_rects_drawn,
                         app.renderer.vk_rects_filled,
                         drawn_major, drawn_local, drawn_path,
                         filt_major, filt_local, filt_path);
            } else {
                log_info("perf region=%s backend=sdl frame=%.1fms events=%.1f update=%.1f render=%.1f zoom=%.2f vis=%u load=%u/%u active=%s "
                         "req=%u/%u res=%u/%u enq=%llu drop=%llu out=%llu out_drop=%llu miss=%llu ok=%llu fail=%llu",
                         app.region.name,
                         frame_ms, events_ms, update_ms, render_ms,
                         app.camera.zoom,
                         app.visible_tile_count,
                         app.loading_done, app.loading_expected,
                         app.active_layer_valid ? app_layer_label(app.active_layer_kind) : "none",
                         stats.req_count, stats.req_capacity,
                         stats.res_count, stats.res_capacity,
                         (unsigned long long)stats.enqueued_count,
                         (unsigned long long)stats.enqueue_drop_count,
                         (unsigned long long)stats.produced_count,
                         (unsigned long long)stats.result_drop_count,
                         (unsigned long long)stats.missing_count,
                         (unsigned long long)stats.load_ok_count,
                         (unsigned long long)stats.load_fail_count);
            }
            perf_next_log = after_render + 1.0;
        }
    }

    app_shutdown(&app);
    return 0;
}
