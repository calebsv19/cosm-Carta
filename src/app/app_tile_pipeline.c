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
        if (expected <= 8u) {
            return expected;
        }
        if (kind == TILE_LAYER_POLY_BUILDING) {
            if (expected <= 32u) {
                return 4u;
            }
            if (expected <= 128u) {
                return 6u;
            }
            return 8u;
        }
        if (expected <= 32u) {
            return 3u;
        }
        if (expected <= 128u) {
            return 4u;
        }
        return 6u;
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
        if (expected <= 8u) {
            return expected;
        }
        if (kind == TILE_LAYER_POLY_BUILDING) {
            if (expected <= 64u) {
                return 4u;
            }
            return 6u;
        }
        if (expected <= 32u) {
            return 3u;
        }
        return 4u;
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
    return layer_policy_zoom_start(kind, app ? app->view_state_bridge.building_zoom_bias : 0.0f);
}

bool app_layer_runtime_enabled(const AppState *app, TileLayerKind kind) {
    if (!app) {
        return false;
    }
    if (kind < 0 || kind >= TILE_LAYER_COUNT) {
        return false;
    }
    const LayerPolicy *policy = layer_policy_for(kind);
    if (policy && !policy->enabled) {
        if (!(kind == TILE_LAYER_CONTOUR && app->tile_state_bridge.contour_runtime_enabled)) {
            return false;
        }
    }
    return app->view_state_bridge.layer_user_enabled[kind];
}

float app_layer_fade_multiplier(const AppState *app, TileLayerKind kind) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return 1.0f;
    }
    if (!app->view_state_bridge.zoom_logic_enabled) {
        return 1.0f;
    }
    float start_zoom = ((float)app->view_state_bridge.layer_fade_start_milli[kind] / 1000.0f) * 20.0f;
    float span_zoom = 0.15f + ((float)app->view_state_bridge.layer_fade_speed_milli[kind] / 1000.0f) * 6.0f;
    float z = app->view_state_bridge.camera.zoom;
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
    if (!app->view_state_bridge.zoom_logic_enabled) {
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
    app->renderer.vk_line_budget = layer_policy_vk_line_budget(app->view_state_bridge.camera.zoom, app->tile_state_bridge.visible_tile_count);
}

static bool app_compute_visible_tile_bounds(AppState *app, uint16_t *out_z, TileCoord *out_top_left, TileCoord *out_bottom_right) {
    if (!app) {
        return false;
    }

    float ppm = camera_pixels_per_meter(&app->view_state_bridge.camera);
    float half_w_m = (float)app->width * 0.5f / ppm;
    float half_h_m = (float)app->height * 0.5f / ppm;

    float min_x = app->view_state_bridge.camera.x - half_w_m;
    float max_x = app->view_state_bridge.camera.x + half_w_m;
    float min_y = app->view_state_bridge.camera.y - half_h_m;
    float max_y = app->view_state_bridge.camera.y + half_h_m;

    uint16_t z = app_zoom_to_tile_level(app->view_state_bridge.camera.zoom, &app->region);
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
    if (item_a->lane < item_b->lane) {
        return -1;
    }
    if (item_a->lane > item_b->lane) {
        return 1;
    }
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

static uint32_t app_tile_ring_distance_from_bounds(TileCoord coord,
                                                   TileCoord top_left,
                                                   TileCoord bottom_right) {
    uint32_t dx = 0u;
    uint32_t dy = 0u;
    if (coord.x < top_left.x) {
        dx = top_left.x - coord.x;
    } else if (coord.x > bottom_right.x) {
        dx = coord.x - bottom_right.x;
    }
    if (coord.y < top_left.y) {
        dy = top_left.y - coord.y;
    } else if (coord.y > bottom_right.y) {
        dy = coord.y - bottom_right.y;
    }
    return dx > dy ? dx : dy;
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
    if (!app->view_state_bridge.zoom_logic_enabled) {
        return TILE_BAND_FINE;
    }
    return layer_policy_band_for_zoom(kind, app->view_state_bridge.camera.zoom, app->view_state_bridge.road_zoom_bias);
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
            if (tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[kind], coord, candidates[i])) {
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
            if (tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[kind], coord, candidates[i])) {
                return true;
            }
        }
        return false;
    }

    return tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[kind], coord, target_band) != NULL;
}

static bool app_visible_coord_has_present_hold_tile(const AppState *app,
                                                    TileLayerKind kind,
                                                    TileCoord coord,
                                                    double now_sec) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return false;
    }
    for (uint32_t i = 0u; i < APP_TILE_PRESENT_HOLD_CAPACITY; ++i) {
        const TilePresentHoldEntry *entry = &app->tile_state_bridge.present_hold[kind][i];
        if (!entry->occupied) {
            continue;
        }
        if (entry->expires_at <= now_sec) {
            continue;
        }
        if (entry->coord.z != coord.z || entry->coord.x != coord.x || entry->coord.y != coord.y) {
            continue;
        }
        if (tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[kind], coord, entry->band)) {
            return true;
        }
    }
    return false;
}

static float app_visible_layer_ideal_coverage_for_band(const AppState *app,
                                                       TileLayerKind kind,
                                                       TileZoomBand band) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return 1.0f;
    }
    if (!app->tile_state_bridge.visible_valid) {
        return 1.0f;
    }
    if (!app_layer_active_runtime(app, kind)) {
        return 1.0f;
    }

    uint32_t expected = 0u;
    uint32_t loaded = 0u;
    for (uint32_t y = app->tile_state_bridge.visible_top_left.y; y <= app->tile_state_bridge.visible_bottom_right.y; ++y) {
        for (uint32_t x = app->tile_state_bridge.visible_top_left.x; x <= app->tile_state_bridge.visible_bottom_right.x; ++x) {
            TileCoord coord = {app->tile_state_bridge.visible_zoom, x, y};
            expected += 1u;
            if (tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[kind], coord, band)) {
                loaded += 1u;
            }
        }
    }
    if (expected == 0u) {
        return 1.0f;
    }
    return (float)loaded / (float)expected;
}

static void app_refresh_visible_layer_coverage(AppState *app) {
    if (!app) {
        return;
    }

    app->tile_state_bridge.visible_ideal_count = 0u;
    app->tile_state_bridge.visible_renderable_count = 0u;
    app->tile_state_bridge.visible_missing_count = 0u;
    app->tile_state_bridge.visible_coverage_ratio = 1.0f;
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        app->tile_state_bridge.layer_visible_expected[i] = 0u;
        app->tile_state_bridge.layer_visible_loaded[i] = 0u;
        app->tile_state_bridge.layer_coverage_ratio[i] = 1.0f;
    }
    for (size_t i = 0; i < TILE_BAND_COUNT; ++i) {
        app->tile_state_bridge.band_visible_expected[i] = 0u;
        app->tile_state_bridge.band_visible_loaded[i] = 0u;
    }
    if (!app->tile_state_bridge.visible_valid) {
        return;
    }

    double now_sec = time_now_seconds();
    for (uint32_t y = app->tile_state_bridge.visible_top_left.y; y <= app->tile_state_bridge.visible_bottom_right.y; ++y) {
        for (uint32_t x = app->tile_state_bridge.visible_top_left.x; x <= app->tile_state_bridge.visible_bottom_right.x; ++x) {
            TileCoord coord = {app->tile_state_bridge.visible_zoom, x, y};
            bool coord_has_runtime_layer = false;
            bool coord_renderable = false;
            for (size_t i = 0; i < layer_policy_count(); ++i) {
                const LayerPolicy *policy = layer_policy_at(i);
                if (!policy) {
                    continue;
                }
                TileLayerKind kind = policy->kind;
                if (!app_layer_active_runtime(app, kind)) {
                    continue;
                }
                coord_has_runtime_layer = true;
                app->tile_state_bridge.layer_visible_expected[kind] += 1u;
                TileZoomBand band = app->tile_state_bridge.layer_target_band[kind];
                if ((size_t)band < TILE_BAND_COUNT) {
                    app->tile_state_bridge.band_visible_expected[band] += 1u;
                }
                bool layer_renderable = app_has_visible_tile_with_fallback(app, kind, coord, band);
                if (!layer_renderable) {
                    layer_renderable = app_visible_coord_has_present_hold_tile(app, kind, coord, now_sec);
                }
                if (layer_renderable) {
                    app->tile_state_bridge.layer_visible_loaded[kind] += 1u;
                    if ((size_t)band < TILE_BAND_COUNT) {
                        app->tile_state_bridge.band_visible_loaded[band] += 1u;
                    }
                    coord_renderable = true;
                }
            }
            if (coord_has_runtime_layer) {
                app->tile_state_bridge.visible_ideal_count += 1u;
                if (coord_renderable) {
                    app->tile_state_bridge.visible_renderable_count += 1u;
                } else {
                    app->tile_state_bridge.visible_missing_count += 1u;
                }
            }
        }
    }
    if (app->tile_state_bridge.visible_ideal_count > 0u) {
        app->tile_state_bridge.visible_coverage_ratio =
            (float)app->tile_state_bridge.visible_renderable_count / (float)app->tile_state_bridge.visible_ideal_count;
    }
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        uint32_t expected = app->tile_state_bridge.layer_visible_expected[i];
        if (expected > 0u) {
            app->tile_state_bridge.layer_coverage_ratio[i] =
                (float)app->tile_state_bridge.layer_visible_loaded[i] / (float)expected;
        }
    }
}

void app_clear_tile_queue(AppState *app) {
    if (!app) {
        return;
    }
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        free(app->tile_state_bridge.tile_queues[i].items);
        memset(&app->tile_state_bridge.tile_queues[i], 0, sizeof(app->tile_state_bridge.tile_queues[i]));
        app->tile_state_bridge.layer_expected[i] = 0;
        app->tile_state_bridge.layer_done[i] = 0;
        app->tile_state_bridge.layer_inflight[i] = 0;
        app->tile_state_bridge.layer_visible_expected[i] = 0;
        app->tile_state_bridge.layer_visible_loaded[i] = 0;
        app->tile_state_bridge.layer_state[i] = LAYER_READINESS_HIDDEN;
        app->tile_state_bridge.queue_band[i] = TILE_BAND_DEFAULT;
        app->tile_state_bridge.previous_target_band[i] = TILE_BAND_DEFAULT;
        app->tile_state_bridge.stable_target_band[i] = TILE_BAND_DEFAULT;
        app->tile_state_bridge.layer_target_band[i] = TILE_BAND_DEFAULT;
        app->tile_state_bridge.layer_band_last_change_time[i] = 0.0;
    }
    for (size_t i = 0; i < TILE_BAND_COUNT; ++i) {
        app->tile_state_bridge.band_visible_expected[i] = 0u;
        app->tile_state_bridge.band_visible_loaded[i] = 0u;
        app->tile_state_bridge.band_queue_depth[i] = 0u;
    }
    app->tile_state_bridge.queue_valid = false;
    app->tile_state_bridge.loading_layer_index = 0;
    app->tile_state_bridge.visible_ideal_count = 0u;
    app->tile_state_bridge.visible_renderable_count = 0u;
    app->tile_state_bridge.visible_missing_count = 0u;
    app->tile_state_bridge.visible_coverage_ratio = 1.0f;
    memset(app->tile_state_bridge.layer_coverage_ratio, 0, sizeof(app->tile_state_bridge.layer_coverage_ratio));
    memset(app->tile_state_bridge.coverage_gate_pending, 0, sizeof(app->tile_state_bridge.coverage_gate_pending));
    memset(app->tile_state_bridge.coverage_gate_target_band, 0, sizeof(app->tile_state_bridge.coverage_gate_target_band));
    memset(app->tile_state_bridge.coverage_gate_pending_since, 0, sizeof(app->tile_state_bridge.coverage_gate_pending_since));
    app->tile_state_bridge.coverage_gate_deferred_count = 0u;
    app->tile_state_bridge.coverage_gate_timeout_count = 0u;
    memset(app->tile_state_bridge.lane_queue_depth, 0, sizeof(app->tile_state_bridge.lane_queue_depth));
    memset(app->tile_state_bridge.lane_service_count, 0, sizeof(app->tile_state_bridge.lane_service_count));
    app->tile_state_bridge.lane_l0_pending = 0u;
    app->tile_state_bridge.lane_l0_pending_active = false;
    app->tile_state_bridge.lane_l0_pending_since = 0.0;
    app->tile_state_bridge.lane_l0_latency_ms = 0.0f;
    app->tile_state_bridge.lane_l0_dropped_visible_requests = 0u;
    app->tile_state_bridge.lane_l0_retry_visible_requests = 0u;
    app->tile_state_bridge.lifecycle_frame_index = 0u;
    app->tile_state_bridge.lifecycle_transition_count = 0u;
    app->tile_state_bridge.lifecycle_invalid_transition_count = 0u;
    app->tile_state_bridge.lifecycle_invalid_transition_total = 0u;
    memset(app->tile_state_bridge.lifecycle_transition_to_state, 0, sizeof(app->tile_state_bridge.lifecycle_transition_to_state));
    app->tile_state_bridge.lifecycle_renderable_ideal_count = 0u;
    app->tile_state_bridge.lifecycle_renderable_fallback_count = 0u;
    memset(app->tile_state_bridge.lifecycle_entries, 0, sizeof(app->tile_state_bridge.lifecycle_entries));
    app->tile_state_bridge.active_layer_valid = false;
    app->tile_state_bridge.transition_blend_draw_count = 0u;
    app->tile_state_bridge.present_hold_hits = 0u;
    app->tile_state_bridge.present_hold_misses = 0u;
    app->tile_state_bridge.present_hold_updates = 0u;
    memset(app->tile_state_bridge.present_hold, 0, sizeof(app->tile_state_bridge.present_hold));
    app->tile_state_bridge.last_queue_rebuild_time = 0.0;
    app_vk_poly_prep_clear(app);
    app_vk_asset_queue_clear(app);
}

static void app_rebuild_tile_queue_for_kind(AppState *app, TileQueue *queue, TileLayerKind kind,
    uint16_t z,
    TileCoord queue_top_left,
    TileCoord queue_bottom_right,
    TileCoord visible_top_left,
    TileCoord visible_bottom_right,
    TileZoomBand band) {
    if (!app) {
        return;
    }

    uint32_t total_tiles = (queue_bottom_right.x - queue_top_left.x + 1) * (queue_bottom_right.y - queue_top_left.y + 1);
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

    TileCoord center = tile_from_meters(z, (MercatorMeters){app->view_state_bridge.camera.x, app->view_state_bridge.camera.y});
    uint32_t count = 0;
    for (uint32_t y = queue_top_left.y; y <= queue_bottom_right.y; ++y) {
        for (uint32_t x = queue_top_left.x; x <= queue_bottom_right.x; ++x) {
            TileCoord coord = {z, x, y};
            const MftTile *ideal = tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[kind], coord, band);
            if (ideal) {
                continue;
            }
            TileQueueLane lane = TILE_QUEUE_LANE_L3_FAR_PREFETCH;
            uint32_t ring = app_tile_ring_distance_from_bounds(coord, visible_top_left, visible_bottom_right);
            if (ring == 0u) {
                bool has_fallback = app_has_visible_tile_with_fallback(app, kind, coord, band);
                lane = has_fallback ? TILE_QUEUE_LANE_L1_VISIBLE_REFINE : TILE_QUEUE_LANE_L0_VISIBLE_MISSING;
            } else if (ring == 1u) {
                lane = TILE_QUEUE_LANE_L2_NEAR_PREFETCH;
            }
            int dx = (int)x - (int)center.x;
            int dy = (int)y - (int)center.y;
            uint32_t dist2 = (uint32_t)(dx * dx + dy * dy);
            queue->items[count++] = (TileQueueItem){coord, dist2, lane};
        }
    }

    if (count > 1) {
        qsort(queue->items, count, sizeof(TileQueueItem), app_tile_queue_compare);
    }

    queue->count = count;
    queue->index = 0;
    app->tile_state_bridge.layer_expected[kind] = count;
    app->tile_state_bridge.layer_done[kind] = 0;
    app->tile_state_bridge.layer_inflight[kind] = 0;
}

static void app_process_tile_queue(AppState *app,
                                   TileQueue *queue,
                                   TileLayerKind kind,
                                   uint32_t budget,
                                   const uint32_t lane_caps[TILE_QUEUE_LANE_COUNT]) {
    if (!app || !queue || budget == 0 || queue->index >= queue->count) {
        return;
    }

    uint32_t lane_used[TILE_QUEUE_LANE_COUNT] = {0u, 0u, 0u, 0u};
    uint32_t remaining_budget = budget;
    while (remaining_budget > 0u && queue->index < queue->count) {
        TileQueueItem item = queue->items[queue->index];
        TileQueueLane lane = item.lane;
        if ((uint32_t)lane >= TILE_QUEUE_LANE_COUNT) {
            lane = TILE_QUEUE_LANE_L3_FAR_PREFETCH;
        }
        if (lane_used[lane] >= lane_caps[lane]) {
            break;
        }
        TileZoomBand band = app->tile_state_bridge.queue_band[kind];
        if (!tile_loader_enqueue(&app->tile_state_bridge.tile_loader, item.coord, kind, band, app->worker_state_bridge.tile_generation)) {
            if (lane == TILE_QUEUE_LANE_L0_VISIBLE_MISSING) {
                app->tile_state_bridge.lane_l0_dropped_visible_requests += 1u;
                app_tile_lifecycle_mark_visible_drop(app, kind, item.coord, band);
            }
            break;
        }
        app_tile_lifecycle_transition(app,
                                      kind,
                                      item.coord,
                                      band,
                                      APP_TILE_LIFECYCLE_REQUESTED,
                                      false,
                                      false,
                                      false,
                                      false);
        if (lane == TILE_QUEUE_LANE_L0_VISIBLE_MISSING &&
            app_tile_lifecycle_consume_visible_drop_retry(app, kind, item.coord, band)) {
            app->tile_state_bridge.lane_l0_retry_visible_requests += 1u;
        }
        queue->index += 1u;
        remaining_budget -= 1u;
        lane_used[lane] += 1u;
        app->tile_state_bridge.layer_inflight[kind] += 1;
        app->tile_state_bridge.lane_service_count[lane] += 1u;
    }
}

static void app_accumulate_queue_lane_depth(const TileQueue *queue, uint32_t io_depth[TILE_QUEUE_LANE_COUNT], uint32_t *io_l0_pending) {
    if (!queue || !io_depth) {
        return;
    }
    for (uint32_t i = queue->index; i < queue->count; ++i) {
        TileQueueLane lane = queue->items[i].lane;
        if ((uint32_t)lane >= TILE_QUEUE_LANE_COUNT) {
            lane = TILE_QUEUE_LANE_L3_FAR_PREFETCH;
        }
        io_depth[lane] += 1u;
    }
    if (!io_l0_pending) {
        return;
    }
    uint32_t pending = 0u;
    for (uint32_t i = queue->index; i < queue->count; ++i) {
        if (queue->items[i].lane != TILE_QUEUE_LANE_L0_VISIBLE_MISSING) {
            break;
        }
        pending += 1u;
    }
    *io_l0_pending += pending;
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
        if (!tile_loader_pop_result(&app->tile_state_bridge.tile_loader, &result)) {
            break;
        }

        if (!app_worker_contract_tile_request_is_current(app, result.request_id)) {
            if (result.ok) {
                mft_free_tile(&result.tile);
            }
            continue;
        }

        if (app->tile_state_bridge.layer_inflight[result.kind] > 0) {
            app->tile_state_bridge.layer_inflight[result.kind] -= 1;
        }
        if (app->tile_state_bridge.layer_done[result.kind] < app->tile_state_bridge.layer_expected[result.kind]) {
            app->tile_state_bridge.layer_done[result.kind] += 1;
        }

        if (!result.ok) {
            continue;
        }

        app_tile_lifecycle_transition(app,
                                      result.kind,
                                      result.coord,
                                      result.band,
                                      APP_TILE_LIFECYCLE_DECODED_CPU,
                                      true,
                                      false,
                                      false,
                                      false);

        if (app->tile_state_bridge.vk_assets_enabled && app_kind_is_polygon(result.kind)) {
            if (!app_vk_poly_prep_enqueue(app, &result)) {
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
            }
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

        if (app->tile_state_bridge.vk_assets_enabled &&
            (result.kind == TILE_LAYER_ROAD_ARTERY ||
             result.kind == TILE_LAYER_ROAD_LOCAL)) {
            const MftTile *cached = tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[result.kind], result.coord, result.band);
            if (cached) {
                vk_tile_cache_on_tile_loaded(&app->tile_state_bridge.vk_tile_cache, app->renderer.vk, result.kind, result.coord, result.band, cached);
                app_tile_lifecycle_transition(app,
                                              result.kind,
                                              result.coord,
                                              result.band,
                                              APP_TILE_LIFECYCLE_RENDERABLE,
                                              true,
                                              true,
                                              !is_ideal,
                                              is_ideal);
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
            app->tile_state_bridge.layer_state[kind] = LAYER_READINESS_HIDDEN;
            continue;
        }

        uint32_t expected = app->tile_state_bridge.layer_expected[kind];
        uint32_t done = app->tile_state_bridge.layer_done[kind];
        uint32_t inflight = app->tile_state_bridge.layer_inflight[kind];
        uint32_t visible_expected = app->tile_state_bridge.layer_visible_expected[kind];
        uint32_t visible_loaded = app->tile_state_bridge.layer_visible_loaded[kind];
        bool full_ready = layer_policy_requires_full_ready(kind);

        if (!full_ready) {
            if (visible_expected == 0u) {
                app->tile_state_bridge.layer_state[kind] = LAYER_READINESS_READY;
            } else if (visible_loaded >= visible_expected) {
                app->tile_state_bridge.layer_state[kind] = LAYER_READINESS_READY;
            } else {
                app->tile_state_bridge.layer_state[kind] = LAYER_READINESS_LOADING;
            }
            continue;
        }

        if (expected > 0 && done >= expected && inflight == 0) {
            app->tile_state_bridge.layer_state[kind] = LAYER_READINESS_READY;
            continue;
        }

        if (expected == 0 && inflight == 0) {
            app->tile_state_bridge.layer_state[kind] = LAYER_READINESS_READY;
            continue;
        }

        if (expected == 0 && app->tile_state_bridge.loading_no_data_time >= APP_TILE_NO_DATA_TIMEOUT) {
            app->tile_state_bridge.layer_state[kind] = LAYER_READINESS_READY;
            continue;
        }

        app->tile_state_bridge.layer_state[kind] = LAYER_READINESS_LOADING;
    }
}

void app_update_tile_queue(AppState *app) {
    if (!app) {
        return;
    }
    app_tile_lifecycle_begin_frame(app);
    double now = time_now_seconds();

    uint16_t z = 0;
    TileCoord top_left = {0};
    TileCoord bottom_right = {0};
    TileCoord queue_top_left = {0};
    TileCoord queue_bottom_right = {0};
    if (!app_compute_visible_tile_bounds(app, &z, &top_left, &bottom_right)) {
        app->tile_state_bridge.visible_valid = false;
        app->tile_state_bridge.loading_expected = 0;
        app->tile_state_bridge.loading_done = 0;
        app_clear_tile_queue(app);
        return;
    }

    app->tile_state_bridge.visible_zoom = z;
    app->tile_state_bridge.visible_top_left = top_left;
    app->tile_state_bridge.visible_bottom_right = bottom_right;
    app->tile_state_bridge.visible_valid = true;
    app->tile_state_bridge.visible_tile_count = (bottom_right.x - top_left.x + 1) * (bottom_right.y - top_left.y + 1);

    queue_top_left = top_left;
    queue_bottom_right = bottom_right;
    {
        const uint32_t tile_limit = tile_count(z);
        // Keep two-tile prefetch margin around the viewport so A2 can schedule near/far lanes.
        const uint32_t prefetch_margin = 2u;
        if (tile_limit > 0u) {
            queue_top_left.x = queue_top_left.x > prefetch_margin ? queue_top_left.x - prefetch_margin : 0u;
            queue_top_left.y = queue_top_left.y > prefetch_margin ? queue_top_left.y - prefetch_margin : 0u;
            uint32_t max_index = tile_limit - 1u;
            queue_bottom_right.x = queue_bottom_right.x + prefetch_margin < max_index ? queue_bottom_right.x + prefetch_margin : max_index;
            queue_bottom_right.y = queue_bottom_right.y + prefetch_margin < max_index ? queue_bottom_right.y + prefetch_margin : max_index;
        }
    }

    bool band_plan_changed = false;
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy) {
            continue;
        }
        TileLayerKind kind = policy->kind;
        TileZoomBand proposed = app_layer_target_band(app, kind);
        TileZoomBand current = app->tile_state_bridge.stable_target_band[kind];

        if (!app->tile_state_bridge.queue_valid) {
            app->tile_state_bridge.previous_target_band[kind] = proposed;
            app->tile_state_bridge.stable_target_band[kind] = proposed;
            app->tile_state_bridge.layer_target_band[kind] = proposed;
            app->tile_state_bridge.layer_band_last_change_time[kind] = now;
            app->tile_state_bridge.coverage_gate_pending[kind] = false;
            app->tile_state_bridge.coverage_gate_target_band[kind] = proposed;
            app->tile_state_bridge.coverage_gate_pending_since[kind] = 0.0;
            continue;
        }

        if (current == proposed) {
            app->tile_state_bridge.coverage_gate_pending[kind] = false;
            app->tile_state_bridge.coverage_gate_target_band[kind] = proposed;
            app->tile_state_bridge.coverage_gate_pending_since[kind] = 0.0;
            app->tile_state_bridge.layer_target_band[kind] = current;
            continue;
        }

        bool should_commit_band = false;
        bool defer_commit = false;
        if (current != proposed) {
            double elapsed = now - app->tile_state_bridge.layer_band_last_change_time[kind];
            if (elapsed >= APP_TILE_BAND_SWITCH_DEBOUNCE_SEC) {
                bool pending_target_changed = !app->tile_state_bridge.coverage_gate_pending[kind] ||
                    app->tile_state_bridge.coverage_gate_target_band[kind] != proposed;
                if (pending_target_changed) {
                    app->tile_state_bridge.coverage_gate_pending[kind] = true;
                    app->tile_state_bridge.coverage_gate_target_band[kind] = proposed;
                    app->tile_state_bridge.coverage_gate_pending_since[kind] = now;
                }

                float global_coverage = app->tile_state_bridge.visible_coverage_ratio;
                float layer_coverage = app_visible_layer_ideal_coverage_for_band(app, kind, proposed);
                bool coverage_ready =
                    global_coverage >= APP_TILE_COVERAGE_GATE_GLOBAL_MIN &&
                    layer_coverage >= APP_TILE_COVERAGE_GATE_LAYER_MIN;
                if (coverage_ready) {
                    should_commit_band = true;
                } else {
                    app->tile_state_bridge.coverage_gate_deferred_count += 1u;
                    double wait_elapsed = now - app->tile_state_bridge.coverage_gate_pending_since[kind];
                    if (wait_elapsed >= APP_TILE_COVERAGE_GATE_MAX_WAIT_SEC) {
                        app->tile_state_bridge.coverage_gate_timeout_count += 1u;
                        should_commit_band = true;
                    } else {
                        defer_commit = true;
                    }
                }
            } else {
                app->tile_state_bridge.band_switch_deferred_count += 1u;
                defer_commit = true;
            }
        }

        if (should_commit_band) {
            app->tile_state_bridge.previous_target_band[kind] = current;
            app->tile_state_bridge.stable_target_band[kind] = proposed;
            app->tile_state_bridge.layer_target_band[kind] = proposed;
            app->tile_state_bridge.layer_band_last_change_time[kind] = now;
            app->tile_state_bridge.coverage_gate_pending[kind] = false;
            app->tile_state_bridge.coverage_gate_pending_since[kind] = 0.0;
            app->tile_state_bridge.coverage_gate_target_band[kind] = proposed;
            band_plan_changed = true;
        } else if (defer_commit) {
            app->tile_state_bridge.layer_target_band[kind] = current;
        } else {
            app->tile_state_bridge.layer_target_band[kind] = app->tile_state_bridge.stable_target_band[kind];
        }
    }

    bool visible_bounds_changed = !app->tile_state_bridge.queue_valid ||
        app->tile_state_bridge.queue_zoom != z ||
        app->tile_state_bridge.queue_top_left.x != queue_top_left.x || app->tile_state_bridge.queue_top_left.y != queue_top_left.y ||
        app->tile_state_bridge.queue_bottom_right.x != queue_bottom_right.x || app->tile_state_bridge.queue_bottom_right.y != queue_bottom_right.y;
    if (!visible_bounds_changed) {
        for (size_t i = 0; i < layer_policy_count(); ++i) {
            const LayerPolicy *policy = layer_policy_at(i);
            if (!policy) {
                continue;
            }
            TileZoomBand desired_queue_band = app->tile_state_bridge.layer_target_band[policy->kind];
            if (app->tile_state_bridge.coverage_gate_pending[policy->kind]) {
                desired_queue_band = app->tile_state_bridge.coverage_gate_target_band[policy->kind];
            }
            if (app->tile_state_bridge.queue_band[policy->kind] != desired_queue_band) {
                band_plan_changed = true;
                break;
            }
        }
    }

    bool bounds_changed = visible_bounds_changed || band_plan_changed;
    if (bounds_changed && !visible_bounds_changed && app->tile_state_bridge.queue_valid) {
        double since_rebuild = now - app->tile_state_bridge.last_queue_rebuild_time;
        if (since_rebuild < APP_TILE_QUEUE_REBUILD_MIN_SEC) {
            bounds_changed = false;
            app->tile_state_bridge.queue_rebuild_deferred_count += 1u;
        }
    }

    if (bounds_changed) {
        app_tile_lifecycle_mark_stale_outside_queue(app, queue_top_left, queue_bottom_right, z);
        app_worker_contract_bump_tile_generation(app);
        for (size_t i = 0; i < layer_policy_count(); ++i) {
            const LayerPolicy *policy = layer_policy_at(i);
            if (!policy) {
                continue;
            }
            TileLayerKind kind = policy->kind;
            TileZoomBand band = app->tile_state_bridge.layer_target_band[kind];
            if (app->tile_state_bridge.coverage_gate_pending[kind]) {
                band = app->tile_state_bridge.coverage_gate_target_band[kind];
            }
            app->tile_state_bridge.queue_band[kind] = band;
            app_rebuild_tile_queue_for_kind(app,
                                            &app->tile_state_bridge.tile_queues[kind],
                                            kind,
                                            z,
                                            queue_top_left,
                                            queue_bottom_right,
                                            top_left,
                                            bottom_right,
                                            band);
        }
        uint32_t queue_tile_count = (queue_bottom_right.x - queue_top_left.x + 1u) *
                                    (queue_bottom_right.y - queue_top_left.y + 1u);
        uint32_t buffer = queue_tile_count;
        if (buffer < 64u) {
            buffer = 64u;
        }
        uint32_t target_capacity = queue_tile_count + buffer;
        for (size_t i = 0; i < layer_policy_count(); ++i) {
            const LayerPolicy *policy = layer_policy_at(i);
            if (!policy || !app_layer_active_runtime(app, policy->kind)) {
                continue;
            }
            tile_manager_ensure_capacity(&app->tile_state_bridge.tile_managers[policy->kind], target_capacity);
        }
        app->tile_state_bridge.queue_top_left = queue_top_left;
        app->tile_state_bridge.queue_bottom_right = queue_bottom_right;
        app->tile_state_bridge.queue_zoom = z;
        app->tile_state_bridge.queue_valid = true;
        app->tile_state_bridge.loading_layer_index = 0;
        app->tile_state_bridge.last_queue_rebuild_time = now;
    }

    if (!app->tile_state_bridge.queue_valid) {
        return;
    }

    for (size_t i = 0; i < TILE_BAND_COUNT; ++i) {
        app->tile_state_bridge.band_queue_depth[i] = 0u;
    }
    for (size_t i = 0; i < TILE_QUEUE_LANE_COUNT; ++i) {
        app->tile_state_bridge.lane_queue_depth[i] = 0u;
        app->tile_state_bridge.lane_service_count[i] = 0u;
    }
    app->tile_state_bridge.lane_l0_pending = 0u;
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy || !app_layer_active_runtime(app, policy->kind)) {
            continue;
        }
        TileQueue *queue = &app->tile_state_bridge.tile_queues[policy->kind];
        uint32_t remaining = 0u;
        if (queue->count > queue->index) {
            remaining = queue->count - queue->index;
        }
        TileZoomBand band = app->tile_state_bridge.queue_band[policy->kind];
        if ((size_t)band < TILE_BAND_COUNT) {
            app->tile_state_bridge.band_queue_depth[band] += remaining + app->tile_state_bridge.layer_inflight[policy->kind];
        }
        app_accumulate_queue_lane_depth(queue,
                                        app->tile_state_bridge.lane_queue_depth,
                                        &app->tile_state_bridge.lane_l0_pending);
    }

    app_refresh_layer_states(app);
    app->tile_state_bridge.active_layer_valid = false;
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy || !app_layer_active_runtime(app, policy->kind)) {
            continue;
        }
        bool full_ready = layer_policy_requires_full_ready(policy->kind);
        if (full_ready) {
            if (app->tile_state_bridge.layer_done[policy->kind] >= app->tile_state_bridge.layer_expected[policy->kind] &&
                app->tile_state_bridge.layer_inflight[policy->kind] == 0) {
                continue;
            }
        } else {
            if (app->tile_state_bridge.layer_visible_loaded[policy->kind] >= app->tile_state_bridge.layer_visible_expected[policy->kind]) {
                continue;
            }
        }
        if (!app->tile_state_bridge.active_layer_valid) {
            app->tile_state_bridge.active_layer_valid = true;
            app->tile_state_bridge.active_layer_kind = policy->kind;
            app->tile_state_bridge.active_layer_expected = full_ready
                ? app->tile_state_bridge.layer_expected[policy->kind]
                : app->tile_state_bridge.layer_visible_expected[policy->kind];
        }
        TileQueue *queue = &app->tile_state_bridge.tile_queues[policy->kind];
        uint32_t expected = app->tile_state_bridge.layer_expected[policy->kind];
        uint32_t budget = app_tile_load_budget(policy->kind, expected);
        if (policy->kind == TILE_LAYER_ROAD_ARTERY || policy->kind == TILE_LAYER_ROAD_LOCAL) {
            if (budget > 8u) {
                budget = 8u;
            }
        } else if (policy->kind == TILE_LAYER_POLY_WATER ||
                   policy->kind == TILE_LAYER_POLY_PARK ||
                   policy->kind == TILE_LAYER_POLY_LANDUSE ||
                   policy->kind == TILE_LAYER_POLY_BUILDING) {
            uint32_t polygon_cap = 2u;
            if (app->tile_state_bridge.visible_tile_count <= 4u) {
                polygon_cap = 4u;
            } else if (app->tile_state_bridge.visible_tile_count <= 16u) {
                polygon_cap = 3u;
            }
            if (policy->kind == TILE_LAYER_POLY_BUILDING && polygon_cap < 4u) {
                polygon_cap += 1u;
            }
            if (budget > polygon_cap) {
                budget = polygon_cap;
            }
        }
        uint32_t lane_caps[TILE_QUEUE_LANE_COUNT] = {
            budget,
            budget,
            budget >= 3u ? 2u : 1u,
            budget >= 6u ? 1u : 0u
        };
        app_process_tile_queue(app, queue, policy->kind, budget, lane_caps);
    }

    for (size_t i = 0; i < TILE_BAND_COUNT; ++i) {
        app->tile_state_bridge.band_queue_depth[i] = 0u;
    }
    for (size_t i = 0; i < TILE_QUEUE_LANE_COUNT; ++i) {
        app->tile_state_bridge.lane_queue_depth[i] = 0u;
    }
    app->tile_state_bridge.lane_l0_pending = 0u;
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy || !app_layer_active_runtime(app, policy->kind)) {
            continue;
        }
        TileQueue *queue = &app->tile_state_bridge.tile_queues[policy->kind];
        uint32_t remaining = 0u;
        if (queue->count > queue->index) {
            remaining = queue->count - queue->index;
        }
        TileZoomBand band = app->tile_state_bridge.queue_band[policy->kind];
        if ((size_t)band < TILE_BAND_COUNT) {
            app->tile_state_bridge.band_queue_depth[band] += remaining + app->tile_state_bridge.layer_inflight[policy->kind];
        }
        app_accumulate_queue_lane_depth(queue,
                                        app->tile_state_bridge.lane_queue_depth,
                                        &app->tile_state_bridge.lane_l0_pending);
    }
    if (app->tile_state_bridge.lane_l0_pending > 0u) {
        app->tile_state_bridge.lane_l0_saturation_total += (uint64_t)app->tile_state_bridge.lane_l0_pending;
        if (!app->tile_state_bridge.lane_l0_pending_active) {
            app->tile_state_bridge.lane_l0_pending_active = true;
            app->tile_state_bridge.lane_l0_pending_since = now;
        }
        double now_end = time_now_seconds();
        app->tile_state_bridge.lane_l0_latency_ms =
            (float)((now_end - app->tile_state_bridge.lane_l0_pending_since) * 1000.0);
    } else {
        app->tile_state_bridge.lane_l0_pending_active = false;
        app->tile_state_bridge.lane_l0_pending_since = 0.0;
        app->tile_state_bridge.lane_l0_latency_ms = 0.0f;
    }
}
