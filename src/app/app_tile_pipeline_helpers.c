#include "app/app_internal.h"
#include "app/app_tile_pipeline_helpers.h"
#include "core/time.h"

bool app_kind_is_polygon(TileLayerKind kind) {
    return kind == TILE_LAYER_POLY_WATER ||
           kind == TILE_LAYER_POLY_PARK ||
           kind == TILE_LAYER_POLY_LANDUSE ||
           kind == TILE_LAYER_POLY_BUILDING;
}

bool app_tile_request_in_region_coverage(const AppState *app,
                                                TileLayerKind kind,
                                                TileZoomBand band,
                                                TileCoord coord) {
    if (!app) {
        return true;
    }
    return region_tile_coverage_contains(&app->region, kind, band, coord);
}

uint32_t app_tile_ring_distance_from_bounds(TileCoord coord,
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

TileZoomBand app_layer_target_band(const AppState *app, TileLayerKind kind) {
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

static uint32_t app_layer_cache_floor(TileLayerKind kind) {
    if (kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL) {
        return 96u;
    }
    if (kind == TILE_LAYER_POLY_BUILDING) {
        return 72u;
    }
    if (kind == TILE_LAYER_POLY_WATER ||
        kind == TILE_LAYER_POLY_PARK ||
        kind == TILE_LAYER_POLY_LANDUSE) {
        return 56u;
    }
    if (kind == TILE_LAYER_CONTOUR) {
        return 24u;
    }
    return 32u;
}

static uint32_t app_layer_cache_cap(TileLayerKind kind) {
    if (kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL) {
        return 384u;
    }
    if (kind == TILE_LAYER_POLY_BUILDING) {
        return 320u;
    }
    if (kind == TILE_LAYER_POLY_WATER ||
        kind == TILE_LAYER_POLY_PARK ||
        kind == TILE_LAYER_POLY_LANDUSE) {
        return 224u;
    }
    if (kind == TILE_LAYER_CONTOUR) {
        return 128u;
    }
    return 160u;
}

static uint32_t app_layer_cache_band_bonus(TileLayerKind kind, TileZoomBand band) {
    bool road = (kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL);
    bool polygon = app_kind_is_polygon(kind);
    if (road) {
        switch (band) {
            case TILE_BAND_FINE:
                return 96u;
            case TILE_BAND_MID:
                return 64u;
            case TILE_BAND_COARSE:
                return 32u;
            case TILE_BAND_DEFAULT:
            default:
                return 16u;
        }
    }
    if (polygon) {
        switch (band) {
            case TILE_BAND_FINE:
                return 48u;
            case TILE_BAND_MID:
                return 32u;
            case TILE_BAND_COARSE:
                return 16u;
            case TILE_BAND_DEFAULT:
            default:
                return 8u;
        }
    }
    switch (band) {
        case TILE_BAND_FINE:
            return 24u;
        case TILE_BAND_MID:
            return 16u;
        case TILE_BAND_COARSE:
            return 8u;
        case TILE_BAND_DEFAULT:
        default:
            return 4u;
    }
}

static uint32_t app_layer_cache_target(TileLayerKind kind,
                                       TileZoomBand band,
                                       uint32_t visible_tile_count,
                                       uint32_t queue_tile_count,
                                       bool runtime_active) {
    uint32_t floor = app_layer_cache_floor(kind);
    uint32_t cap = app_layer_cache_cap(kind);

    if (!runtime_active) {
        uint32_t standby = floor / 2u;
        if (standby < 16u) {
            standby = 16u;
        }
        if (standby > cap) {
            standby = cap;
        }
        return standby;
    }

    uint32_t target = floor;
    target += visible_tile_count;
    target += visible_tile_count / 2u;
    if (kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL) {
        target += visible_tile_count;
    } else if (kind == TILE_LAYER_POLY_BUILDING) {
        target += visible_tile_count / 2u;
    }
    if (queue_tile_count > visible_tile_count) {
        uint32_t prefetch = queue_tile_count - visible_tile_count;
        if (kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL) {
            target += prefetch;
        } else if (kind == TILE_LAYER_POLY_BUILDING) {
            target += prefetch / 2u;
        } else {
            target += prefetch / 3u;
        }
    }
    target += app_layer_cache_band_bonus(kind, band);

    if (target < floor) {
        target = floor;
    }
    if (target > cap) {
        target = cap;
    }
    return target;
}

void app_apply_layer_residency_budgets(AppState *app,
                                              TileCoord queue_top_left,
                                              TileCoord queue_bottom_right) {
    if (!app) {
        return;
    }

    memset(app->tile_state_bridge.cache_target, 0, sizeof(app->tile_state_bridge.cache_target));
    memset(app->tile_state_bridge.cache_resident, 0, sizeof(app->tile_state_bridge.cache_resident));
    memset(app->tile_state_bridge.cache_evicted_frame, 0, sizeof(app->tile_state_bridge.cache_evicted_frame));
    app->tile_state_bridge.cache_evicted_frame_total = 0u;

    if (!app->tile_state_bridge.visible_valid) {
        return;
    }

    uint32_t queue_tile_count = (queue_bottom_right.x - queue_top_left.x + 1u) *
                                (queue_bottom_right.y - queue_top_left.y + 1u);

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
        bool runtime_active = app_layer_active_runtime(app, kind);
        uint32_t target = app_layer_cache_target(kind,
                                                 band,
                                                 app->tile_state_bridge.visible_tile_count,
                                                 queue_tile_count,
                                                 runtime_active);
        app->tile_state_bridge.cache_target[kind] = target;

        TileManager *manager = &app->tile_state_bridge.tile_managers[kind];
        if (target > 0u) {
            tile_manager_ensure_capacity(manager, target);
        }

        TileManagerTrimPolicy trim_policy = {0};
        trim_policy.protect_visible_bounds = runtime_active;
        trim_policy.visible_top_left = app->tile_state_bridge.visible_top_left;
        trim_policy.visible_bottom_right = app->tile_state_bridge.visible_bottom_right;
        trim_policy.visible_zoom = app->tile_state_bridge.visible_zoom;
        trim_policy.protect_queue_bounds = runtime_active;
        trim_policy.queue_top_left = queue_top_left;
        trim_policy.queue_bottom_right = queue_bottom_right;
        trim_policy.queue_zoom = app->tile_state_bridge.visible_zoom;
        trim_policy.prefer_band = runtime_active;
        trim_policy.preferred_band = band;

        uint32_t evicted = tile_manager_trim_to_count(manager, target, &trim_policy);
        app->tile_state_bridge.cache_evicted_frame[kind] = evicted;
        app->tile_state_bridge.cache_evicted_total_by_layer[kind] += (uint64_t)evicted;
        app->tile_state_bridge.cache_evicted_frame_total += evicted;
        app->tile_state_bridge.cache_evicted_total += (uint64_t)evicted;
        app->tile_state_bridge.cache_resident[kind] = tile_manager_count(manager);
    }
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

static bool app_tile_has_parent_retention_with_candidates(const AppState *app,
                                                          TileLayerKind kind,
                                                          TileCoord coord,
                                                          const TileZoomBand *candidates,
                                                          uint32_t candidate_count) {
    if (!app || !candidates || candidate_count == 0u) {
        return false;
    }
    TileCoord parent = coord;
    for (uint32_t depth = 0u; depth < APP_TILE_RETENTION_PARENT_MAX_DEPTH; ++depth) {
        if (parent.z == 0u) {
            break;
        }
        parent.z -= 1u;
        parent.x >>= 1u;
        parent.y >>= 1u;
        for (uint32_t i = 0u; i < candidate_count; ++i) {
            TileZoomBand band = candidates[i];
            if (tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[kind], parent, band)) {
                return true;
            }
        }
    }
    return false;
}

bool app_has_visible_tile_with_fallback(const AppState *app,
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
        if (app_tile_has_parent_retention_with_candidates(app, kind, coord, candidates, count)) {
            return true;
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
        if (app_tile_has_parent_retention_with_candidates(app, kind, coord, candidates, count)) {
            return true;
        }
        return false;
    }

    if (tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[kind], coord, target_band)) {
        return true;
    }
    TileZoomBand fallback[1] = {target_band};
    return app_tile_has_parent_retention_with_candidates(app, kind, coord, fallback, 1u);
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

float app_visible_layer_ideal_coverage_for_band(const AppState *app,
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
            if (!app_tile_request_in_region_coverage(app, kind, band, coord)) {
                continue;
            }
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

void app_refresh_visible_layer_coverage(AppState *app) {
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
                TileZoomBand band = app->tile_state_bridge.layer_target_band[kind];
                if (!app_tile_request_in_region_coverage(app, kind, band, coord)) {
                    continue;
                }
                coord_has_runtime_layer = true;
                app->tile_state_bridge.layer_visible_expected[kind] += 1u;
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
