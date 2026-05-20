#include "app_headless_render_internal.h"

#include "app/app_tile_pipeline_helpers.h"
#include "map/layer_policy.h"

#include <string.h>

static bool map_forge_headless_compute_visible_tile_bounds(AppState *app,
                                                           uint16_t *out_z,
                                                           TileCoord *out_top_left,
                                                           TileCoord *out_bottom_right) {
    float min_x = 0.0f;
    float max_x = 0.0f;
    float min_y = 0.0f;
    float max_y = 0.0f;
    if (!app) {
        return false;
    }

    if (!camera_visible_world_aabb(&app->view_state_bridge.camera,
                                   app->width,
                                   app->height,
                                   &min_x,
                                   &min_y,
                                   &max_x,
                                   &max_y)) {
        return false;
    }

    uint16_t z = app_zoom_to_tile_level(app->view_state_bridge.camera.zoom, &app->region);
    TileCoord top_left = tile_from_meters(z, (MercatorMeters){min_x, max_y});
    TileCoord bottom_right = tile_from_meters(z, (MercatorMeters){max_x, min_y});
    uint32_t count = tile_count(z);
    if (count == 0u) {
        return false;
    }
    if (bottom_right.x >= count) {
        bottom_right.x = count - 1u;
    }
    if (bottom_right.y >= count) {
        bottom_right.y = count - 1u;
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

static void map_forge_headless_seed_layer_defaults(AppState *app) {
    if (!app) {
        return;
    }

    app->view_state_bridge.show_landuse = true;
    app->view_state_bridge.building_zoom_bias = app_building_zoom_bias_for_region(&app->region);
    app->view_state_bridge.building_fill_enabled = false;
    app->view_state_bridge.road_zoom_bias = app_road_zoom_bias_for_region(&app->region);
    app->view_state_bridge.polygon_outline_only = true;
    app->view_state_bridge.zoom_logic_enabled = true;
    app->single_line = true;
    app->tile_state_bridge.presenter_invariants_enabled = false;
    app->tile_state_bridge.contour_runtime_enabled = false;
    app_runtime_budget_policy_init(app);
    app_runtime_budget_reset_frame(app);

    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        app->view_state_bridge.layer_user_enabled[i] = true;
        app->view_state_bridge.layer_opacity_milli[i] = 1000u;
        app->view_state_bridge.layer_fade_speed_milli[i] = 170u;
        {
            float zoom_start = app_layer_zoom_start(app, (TileLayerKind)i);
            if (zoom_start < 0.0f) {
                zoom_start = 0.0f;
            }
            if (zoom_start > 20.0f) {
                zoom_start = 20.0f;
            }
            app->view_state_bridge.layer_fade_start_milli[i] = (uint16_t)(zoom_start * 50.0f);
        }
        app->tile_state_bridge.queue_band[i] = TILE_BAND_DEFAULT;
        app->tile_state_bridge.previous_target_band[i] = TILE_BAND_DEFAULT;
        app->tile_state_bridge.stable_target_band[i] = TILE_BAND_DEFAULT;
        app->tile_state_bridge.layer_target_band[i] = TILE_BAND_DEFAULT;
        app->tile_state_bridge.layer_state[i] = LAYER_READINESS_HIDDEN;
        app->tile_state_bridge.layer_coverage_ratio[i] = 1.0f;
    }
}

static uint32_t map_forge_headless_band_candidates(TileLayerKind kind,
                                                   TileZoomBand target,
                                                   TileZoomBand out_bands[4]) {
    uint32_t count = 0u;
    if (!out_bands) {
        return 0u;
    }

    if (kind == TILE_LAYER_POLY_BUILDING) {
        if (target == TILE_BAND_FINE || target == TILE_BAND_MID) {
            out_bands[count++] = TILE_BAND_FINE;
            out_bands[count++] = TILE_BAND_MID;
            out_bands[count++] = TILE_BAND_COARSE;
            out_bands[count++] = TILE_BAND_DEFAULT;
            return count;
        }
        if (target == TILE_BAND_COARSE) {
            out_bands[count++] = TILE_BAND_MID;
            out_bands[count++] = TILE_BAND_COARSE;
            out_bands[count++] = TILE_BAND_DEFAULT;
            return count;
        }
        out_bands[count++] = TILE_BAND_DEFAULT;
        return count;
    }

    if (target == TILE_BAND_FINE) {
        out_bands[count++] = TILE_BAND_FINE;
        out_bands[count++] = TILE_BAND_MID;
        out_bands[count++] = TILE_BAND_COARSE;
        out_bands[count++] = TILE_BAND_DEFAULT;
        return count;
    }
    if (target == TILE_BAND_MID) {
        out_bands[count++] = TILE_BAND_MID;
        out_bands[count++] = TILE_BAND_COARSE;
        out_bands[count++] = TILE_BAND_DEFAULT;
        return count;
    }
    if (target == TILE_BAND_COARSE) {
        out_bands[count++] = TILE_BAND_COARSE;
        out_bands[count++] = TILE_BAND_DEFAULT;
        return count;
    }

    out_bands[count++] = TILE_BAND_DEFAULT;
    return count;
}

static TileZoomBand map_forge_headless_effective_band_for_visible_zoom(const AppState *app,
                                                                       TileLayerKind kind,
                                                                       TileZoomBand target_band,
                                                                       uint16_t visible_zoom) {
    TileZoomBand candidates[4] = {TILE_BAND_DEFAULT, TILE_BAND_DEFAULT, TILE_BAND_DEFAULT, TILE_BAND_DEFAULT};
    uint32_t count = map_forge_headless_band_candidates(kind, target_band, candidates);
    if (!app) {
        return target_band;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        TileZoomBand candidate = candidates[i];
        if (region_tile_coverage_has_zoom(&app->region, kind, candidate, visible_zoom)) {
            return candidate;
        }
    }
    return target_band;
}

bool map_forge_headless_map_layers_init(AppState *app,
                                        const Renderer *renderer,
                                        const RegionInfo *region,
                                        int width,
                                        int height) {
    if (!app || !renderer || !region || width <= 0 || height <= 0) {
        return false;
    }

    memset(app, 0, sizeof(*app));
    app->renderer = *renderer;
    app->region = *region;
    app->width = width;
    app->height = height;
    map_forge_headless_seed_layer_defaults(app);

    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        if (!tile_manager_init_with_source_for_layer(&app->tile_state_bridge.tile_managers[i],
                                                     256u,
                                                     &app->region.tile_source,
                                                     (TileLayerKind)i)) {
            map_forge_headless_map_layers_shutdown(app);
            return false;
        }
    }
    return true;
}

void map_forge_headless_map_layers_shutdown(AppState *app) {
    if (!app) {
        return;
    }
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        tile_manager_shutdown(&app->tile_state_bridge.tile_managers[i]);
    }
    memset(app, 0, sizeof(*app));
}

bool map_forge_headless_map_layers_prepare_frame(AppState *app,
                                                 const Renderer *renderer,
                                                 const Camera *camera) {
    if (!app || !renderer || !camera) {
        return false;
    }

    app->renderer = *renderer;
    app->view_state_bridge.camera = *camera;
    if (!map_forge_headless_compute_visible_tile_bounds(app,
                                                        &app->tile_state_bridge.visible_zoom,
                                                        &app->tile_state_bridge.visible_top_left,
                                                        &app->tile_state_bridge.visible_bottom_right)) {
        app->tile_state_bridge.visible_valid = false;
        return false;
    }

    app->tile_state_bridge.visible_valid = true;
    app->tile_state_bridge.visible_tile_count =
        (app->tile_state_bridge.visible_bottom_right.x - app->tile_state_bridge.visible_top_left.x + 1u) *
        (app->tile_state_bridge.visible_bottom_right.y - app->tile_state_bridge.visible_top_left.y + 1u);

    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        TileLayerKind kind = (TileLayerKind)i;
        if (!app_layer_runtime_enabled(app, kind)) {
            app->tile_state_bridge.layer_state[i] = LAYER_READINESS_HIDDEN;
            app->tile_state_bridge.layer_expected[i] = 0u;
            app->tile_state_bridge.layer_done[i] = 0u;
            app->tile_state_bridge.layer_visible_expected[i] = 0u;
            app->tile_state_bridge.layer_visible_loaded[i] = 0u;
            continue;
        }
        {
            TileZoomBand policy_band = app_layer_target_band(app, kind);
            TileZoomBand effective_band =
                map_forge_headless_effective_band_for_visible_zoom(app,
                                                                  kind,
                                                                  policy_band,
                                                                  app->tile_state_bridge.visible_zoom);
            app->tile_state_bridge.previous_target_band[i] = policy_band;
            app->tile_state_bridge.stable_target_band[i] = effective_band;
            app->tile_state_bridge.layer_target_band[i] = effective_band;
            app->tile_state_bridge.queue_band[i] = effective_band;
            app->tile_state_bridge.layer_state[i] = LAYER_READINESS_READY;
        }
    }

    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        TileLayerKind kind = (TileLayerKind)i;
        TileZoomBand band = app->tile_state_bridge.layer_target_band[i];
        uint32_t expected = 0u;
        uint32_t loaded = 0u;
        if (!app_layer_active_runtime(app, kind)) {
            app->tile_state_bridge.layer_expected[i] = 0u;
            app->tile_state_bridge.layer_done[i] = 0u;
            app->tile_state_bridge.layer_visible_expected[i] = 0u;
            app->tile_state_bridge.layer_visible_loaded[i] = 0u;
            continue;
        }
        for (uint32_t y = app->tile_state_bridge.visible_top_left.y;
             y <= app->tile_state_bridge.visible_bottom_right.y;
             ++y) {
            for (uint32_t x = app->tile_state_bridge.visible_top_left.x;
                 x <= app->tile_state_bridge.visible_bottom_right.x;
                 ++x) {
                TileCoord coord = {app->tile_state_bridge.visible_zoom, x, y};
                if (!app_tile_request_in_region_coverage(app, kind, band, coord)) {
                    continue;
                }
                expected += 1u;
                if (tile_manager_get_tile(&app->tile_state_bridge.tile_managers[i], coord, band)) {
                    loaded += 1u;
                }
            }
        }
        app->tile_state_bridge.layer_expected[i] = expected;
        app->tile_state_bridge.layer_done[i] = loaded;
        app->tile_state_bridge.layer_visible_expected[i] = expected;
        app->tile_state_bridge.layer_visible_loaded[i] = loaded;
    }

    app_refresh_visible_layer_coverage(app);
    return true;
}

void map_forge_headless_map_layers_draw(AppState *app, AppVisibleTileRenderStats *out_stats) {
    AppVisibleTileRenderStats local_stats;
    if (!app || !app->tile_state_bridge.visible_valid) {
        return;
    }
    if (!out_stats) {
        out_stats = &local_stats;
    }
    memset(out_stats, 0, sizeof(*out_stats));
    app_draw_visible_tiles(app, out_stats);
}
