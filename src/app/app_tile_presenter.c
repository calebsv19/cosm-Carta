#include "app/app_internal.h"

#include "core/log.h"
#include "map/polygon_renderer.h"
#include "map/road_renderer.h"

static bool app_tile_coord_equals(TileCoord a, TileCoord b) {
    return a.z == b.z && a.x == b.x && a.y == b.y;
}

void app_tile_presenter_reset_frame_counters(AppState *app) {
    if (!app) {
        return;
    }
    app->tile_state_bridge.transition_blend_draw_count = 0u;
    app->tile_state_bridge.present_hold_hits = 0u;
    app->tile_state_bridge.present_hold_misses = 0u;
    app->tile_state_bridge.present_hold_updates = 0u;
}

float app_tile_presenter_band_blend_mix(const AppState *app, TileLayerKind kind, double now_sec) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return 1.0f;
    }
    TileZoomBand from = app->tile_state_bridge.previous_target_band[kind];
    TileZoomBand to = app->tile_state_bridge.layer_target_band[kind];
    if (from == to) {
        return 1.0f;
    }
    double elapsed = now_sec - app->tile_state_bridge.layer_band_last_change_time[kind];
    if (elapsed <= 0.0) {
        return 0.0f;
    }
    if (elapsed >= APP_TILE_BAND_BLEND_WINDOW_SEC) {
        return 1.0f;
    }
    return (float)(elapsed / APP_TILE_BAND_BLEND_WINDOW_SEC);
}

bool app_tile_presenter_peek_tile_for_band(const AppState *app,
                                           TileLayerKind kind,
                                           TileCoord coord,
                                           TileZoomBand band,
                                           const MftTile **out_tile) {
    if (!app || !out_tile || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return false;
    }
    const MftTile *tile = tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[kind], coord, band);
    if (!tile) {
        return false;
    }
    *out_tile = tile;
    return true;
}

static uint32_t app_tile_presenter_fill_candidates(TileLayerKind kind,
                                                   TileZoomBand target,
                                                   TileZoomBand candidates[4]) {
    candidates[0] = target;
    candidates[1] = TILE_BAND_MID;
    candidates[2] = TILE_BAND_COARSE;
    candidates[3] = TILE_BAND_DEFAULT;
    uint32_t count = 1u;
    if (kind == TILE_LAYER_POLY_BUILDING) {
        if (target == TILE_BAND_FINE || target == TILE_BAND_MID) {
            candidates[0] = TILE_BAND_FINE;
            candidates[1] = TILE_BAND_MID;
            candidates[2] = TILE_BAND_COARSE;
            candidates[3] = TILE_BAND_DEFAULT;
            count = 4u;
        } else if (target == TILE_BAND_COARSE) {
            candidates[0] = TILE_BAND_MID;
            candidates[1] = TILE_BAND_COARSE;
            candidates[2] = TILE_BAND_DEFAULT;
            count = 3u;
        } else {
            candidates[0] = TILE_BAND_DEFAULT;
            count = 1u;
        }
        return count;
    }
    if (target == TILE_BAND_FINE) {
        count = 4u;
    } else if (target == TILE_BAND_MID) {
        candidates[1] = TILE_BAND_COARSE;
        candidates[2] = TILE_BAND_DEFAULT;
        count = 3u;
    } else if (target == TILE_BAND_COARSE) {
        candidates[1] = TILE_BAND_DEFAULT;
        count = 2u;
    }
    return count;
}

bool app_tile_presenter_pick_tile_with_fallback(const AppState *app,
                                                TileLayerKind kind,
                                                TileCoord coord,
                                                const MftTile **out_tile,
                                                TileZoomBand *out_band) {
    if (!app || !out_tile || !out_band || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return false;
    }
    TileZoomBand candidates[4];
    uint32_t count = app_tile_presenter_fill_candidates(kind, app->tile_state_bridge.layer_target_band[kind], candidates);
    for (uint32_t i = 0u; i < count; ++i) {
        TileZoomBand band = candidates[i];
        const MftTile *tile = tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[kind], coord, band);
        if (!tile) {
            continue;
        }
        *out_tile = tile;
        *out_band = band;
        return true;
    }
    return false;
}

static TilePresentHoldEntry *app_present_hold_find(AppState *app, TileLayerKind kind, TileCoord coord) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return NULL;
    }
    for (uint32_t i = 0; i < APP_TILE_PRESENT_HOLD_CAPACITY; ++i) {
        TilePresentHoldEntry *entry = &app->tile_state_bridge.present_hold[kind][i];
        if (!entry->occupied) {
            continue;
        }
        if (app_tile_coord_equals(entry->coord, coord)) {
            return entry;
        }
    }
    return NULL;
}

static TilePresentHoldEntry *app_present_hold_pick_slot(AppState *app, TileLayerKind kind, double now_sec) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return NULL;
    }
    TilePresentHoldEntry *oldest = NULL;
    for (uint32_t i = 0; i < APP_TILE_PRESENT_HOLD_CAPACITY; ++i) {
        TilePresentHoldEntry *entry = &app->tile_state_bridge.present_hold[kind][i];
        if (!entry->occupied || entry->expires_at <= now_sec) {
            return entry;
        }
        if (!oldest || entry->stamp < oldest->stamp) {
            oldest = entry;
        }
    }
    return oldest;
}

void app_tile_presenter_present_hold_remember(AppState *app,
                                              TileLayerKind kind,
                                              TileCoord coord,
                                              TileZoomBand band,
                                              double now_sec) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return;
    }
    TilePresentHoldEntry *entry = app_present_hold_find(app, kind, coord);
    if (!entry) {
        entry = app_present_hold_pick_slot(app, kind, now_sec);
    }
    if (!entry) {
        return;
    }
    entry->occupied = true;
    entry->coord = coord;
    entry->band = band;
    entry->expires_at = now_sec + APP_TILE_PRESENT_HOLD_TTL_SEC;
    entry->stamp = app->tile_state_bridge.present_hold_tick++;
    app->tile_state_bridge.present_hold_updates += 1u;
}

bool app_tile_presenter_present_hold_lookup(AppState *app,
                                            TileLayerKind kind,
                                            TileCoord coord,
                                            double now_sec,
                                            TileZoomBand *out_band) {
    if (!app || !out_band || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return false;
    }
    TilePresentHoldEntry *entry = app_present_hold_find(app, kind, coord);
    if (!entry) {
        app->tile_state_bridge.present_hold_misses += 1u;
        return false;
    }
    if (entry->expires_at <= now_sec) {
        entry->occupied = false;
        app->tile_state_bridge.present_hold_misses += 1u;
        return false;
    }
    *out_band = entry->band;
    entry->stamp = app->tile_state_bridge.present_hold_tick++;
    app->tile_state_bridge.present_hold_hits += 1u;
    return true;
}

bool app_tile_presenter_resolve_tile_for_present(AppState *app,
                                                 TileLayerKind kind,
                                                 TileCoord coord,
                                                 double now_sec,
                                                 const MftTile **out_tile,
                                                 TileZoomBand *out_band) {
    if (!app || !out_tile || !out_band || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return false;
    }
    if (app_tile_presenter_pick_tile_with_fallback(app, kind, coord, out_tile, out_band)) {
        return true;
    }
    TileZoomBand hold_band = TILE_BAND_DEFAULT;
    if (!app_tile_presenter_present_hold_lookup(app, kind, coord, now_sec, &hold_band)) {
        return false;
    }
    if (!app_tile_presenter_peek_tile_for_band(app, kind, coord, hold_band, out_tile)) {
        return false;
    }
    *out_band = hold_band;
    return true;
}

bool app_tile_presenter_draw_polygon_band_blend(AppState *app,
                                                TileLayerKind kind,
                                                TileCoord coord,
                                                float building_zoom_bias,
                                                float layer_opacity,
                                                double now_sec) {
    if (!app) {
        return false;
    }
    float band_mix = app_tile_presenter_band_blend_mix(app, kind, now_sec);
    TileZoomBand from_band = app->tile_state_bridge.previous_target_band[kind];
    TileZoomBand to_band = app->tile_state_bridge.layer_target_band[kind];
    if (from_band == to_band || band_mix >= 1.0f) {
        return false;
    }

    const MftTile *from_tile = NULL;
    const MftTile *to_tile = NULL;
    bool has_from = app_tile_presenter_peek_tile_for_band(app, kind, coord, from_band, &from_tile);
    bool has_to = app_tile_presenter_peek_tile_for_band(app, kind, coord, to_band, &to_tile);
    if (!has_from && !has_to) {
        return false;
    }
    if (has_from) {
        polygon_renderer_draw_tile(&app->renderer, &app->view_state_bridge.camera, (MftTile *)from_tile, app->view_state_bridge.show_landuse,
                                   building_zoom_bias, app->view_state_bridge.building_fill_enabled, app->view_state_bridge.polygon_outline_only,
                                   layer_opacity * (1.0f - band_mix));
    }
    if (has_to) {
        polygon_renderer_draw_tile(&app->renderer, &app->view_state_bridge.camera, (MftTile *)to_tile, app->view_state_bridge.show_landuse,
                                   building_zoom_bias, app->view_state_bridge.building_fill_enabled, app->view_state_bridge.polygon_outline_only,
                                   layer_opacity * band_mix);
    }
    app_tile_presenter_present_hold_remember(app, kind, coord, has_to ? to_band : from_band, now_sec);
    app->tile_state_bridge.transition_blend_draw_count += 1u;
    app->tile_state_bridge.draw_path_fallback_count += 1u;
    return true;
}

bool app_tile_presenter_draw_road_band_blend(AppState *app,
                                             TileLayerKind kind,
                                             TileCoord coord,
                                             bool single_line,
                                             float road_zoom_bias,
                                             float road_opacity,
                                             double now_sec) {
    if (!app) {
        return false;
    }
    float band_mix = app_tile_presenter_band_blend_mix(app, kind, now_sec);
    TileZoomBand from_band = app->tile_state_bridge.previous_target_band[kind];
    TileZoomBand to_band = app->tile_state_bridge.layer_target_band[kind];
    if (from_band == to_band || band_mix >= 1.0f) {
        return false;
    }

    const MftTile *from_tile = NULL;
    const MftTile *to_tile = NULL;
    bool has_from = app_tile_presenter_peek_tile_for_band(app, kind, coord, from_band, &from_tile);
    bool has_to = app_tile_presenter_peek_tile_for_band(app, kind, coord, to_band, &to_tile);
    if (!has_from && !has_to) {
        return false;
    }
    if (has_from) {
        road_renderer_draw_tile(&app->renderer, &app->view_state_bridge.camera, from_tile, single_line, road_zoom_bias, road_opacity * (1.0f - band_mix));
    }
    if (has_to) {
        road_renderer_draw_tile(&app->renderer, &app->view_state_bridge.camera, to_tile, single_line, road_zoom_bias, road_opacity * band_mix);
    }
    app_tile_presenter_present_hold_remember(app, kind, coord, has_to ? to_band : from_band, now_sec);
    app->tile_state_bridge.transition_blend_draw_count += 1u;
    app->tile_state_bridge.draw_path_fallback_count += 1u;
    return true;
}

bool app_tile_presenter_draw_road_layer(AppState *app,
                                        TileLayerKind kind,
                                        TileCoord coord,
                                        const MftTile *tile,
                                        TileZoomBand band,
                                        bool single_line,
                                        float road_zoom_bias,
                                        float road_opacity,
                                        double now_sec,
                                        uint32_t *io_vk_asset_misses) {
    if (!app || !tile) {
        return false;
    }

    if (app_tile_presenter_draw_road_band_blend(app, kind, coord, single_line, road_zoom_bias, road_opacity, now_sec)) {
        return true;
    }
    if (app_try_draw_vk_cached_tile(app, kind, coord, band)) {
        app_tile_presenter_present_hold_remember(app, kind, coord, band, now_sec);
        app->tile_state_bridge.draw_path_vk_count += 1u;
        return true;
    }

    bool vk_assets_mode = renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN &&
                          app->tile_state_bridge.vk_assets_enabled;
    if (vk_assets_mode) {
        if (io_vk_asset_misses) {
            *io_vk_asset_misses += 1u;
        }
        app_vk_asset_enqueue(app, kind, coord, band);
    }

    road_renderer_draw_tile(&app->renderer, &app->view_state_bridge.camera, tile, single_line, road_zoom_bias, road_opacity);
    app_tile_presenter_present_hold_remember(app, kind, coord, band, now_sec);
    app->tile_state_bridge.draw_path_fallback_count += 1u;
    return true;
}

bool app_tile_presenter_draw_polygon_layer(AppState *app,
                                           TileLayerKind kind,
                                           TileCoord coord,
                                           const MftTile *tile,
                                           TileZoomBand band,
                                           float building_zoom_bias,
                                           float layer_opacity,
                                           bool allow_immediate_polygon_fallback,
                                           bool allow_building_fallback,
                                           VkPolyFillBudget *poly_fill_budget,
                                           VkPolyAssetBuildBudget *poly_asset_build_budget,
                                           double now_sec,
                                           uint32_t *io_vk_asset_misses) {
    if (!app || !tile) {
        return false;
    }

    bool blended = app_tile_presenter_draw_polygon_band_blend(app, kind, coord, building_zoom_bias, layer_opacity, now_sec);
    if (blended) {
        return true;
    }

    if (app_try_draw_vk_cached_polygon_tile(app, kind, coord, band, poly_fill_budget, poly_asset_build_budget)) {
        app_tile_presenter_present_hold_remember(app, kind, coord, band, now_sec);
        app->tile_state_bridge.draw_path_vk_count += 1u;
        return true;
    }

    bool vk_assets_mode = renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN &&
                          app->tile_state_bridge.vk_assets_enabled;
    if (vk_assets_mode) {
        if (io_vk_asset_misses) {
            *io_vk_asset_misses += 1u;
        }
        app_vk_asset_enqueue(app, kind, coord, band);
    }

    if (allow_immediate_polygon_fallback || (kind == TILE_LAYER_POLY_BUILDING && allow_building_fallback)) {
        polygon_renderer_draw_tile(&app->renderer,
                                   &app->view_state_bridge.camera,
                                   (MftTile *)tile,
                                   app->view_state_bridge.show_landuse,
                                   building_zoom_bias,
                                   app->view_state_bridge.building_fill_enabled,
                                   app->view_state_bridge.polygon_outline_only,
                                   layer_opacity);
        app_tile_presenter_present_hold_remember(app, kind, coord, band, now_sec);
        app->tile_state_bridge.draw_path_fallback_count += 1u;
        return true;
    }

    return false;
}

bool app_tile_presenter_validate_frame_invariants(AppState *app,
                                                  uint32_t visible_tiles,
                                                  uint32_t vk_asset_misses) {
    if (!app) {
        return false;
    }
    bool vk_backend = renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN;
    bool vk_assets_mode = vk_backend && app->tile_state_bridge.vk_assets_enabled;
    bool valid = true;

    if (app->tile_state_bridge.transition_blend_draw_count > app->tile_state_bridge.draw_path_fallback_count) {
        valid = false;
    }
    if (!vk_backend && app->tile_state_bridge.draw_path_vk_count > 0u) {
        valid = false;
    }
    if (!vk_assets_mode && vk_asset_misses > 0u) {
        valid = false;
    }
    if (visible_tiles == 0u &&
        (app->tile_state_bridge.draw_path_vk_count > 0u ||
         app->tile_state_bridge.draw_path_fallback_count > 0u)) {
        valid = false;
    }
    if (valid) {
        return true;
    }

    app->tile_state_bridge.presenter_invariant_fail_count += 1u;
    static uint64_t next_log_ms = 0u;
    uint64_t now_ms = SDL_GetTicks64();
    if (now_ms >= next_log_ms) {
        log_error("tile_presenter invariant fail visible=%u vk_miss=%u backend=%s vk_assets=%s draw(vk=%u fallback=%u blend=%u)",
                  visible_tiles,
                  vk_asset_misses,
                  vk_backend ? "vk" : "sdl",
                  app->tile_state_bridge.vk_assets_enabled ? "on" : "off",
                  app->tile_state_bridge.draw_path_vk_count,
                  app->tile_state_bridge.draw_path_fallback_count,
                  app->tile_state_bridge.transition_blend_draw_count);
        next_log_ms = now_ms + 1000u;
    }
    return false;
}
