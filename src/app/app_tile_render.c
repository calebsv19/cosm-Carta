#include "app/app_internal.h"

#include "map/mercator.h"
#include "map/map_space.h"
#include "map/polygon_renderer.h"
#include "map/road_renderer.h"

#include <string.h>

static void app_init_vk_poly_fill_budget(AppState *app, VkPolyFillBudget *budget) {
    if (!budget) {
        return;
    }
    memset(budget, 0, sizeof(*budget));
    if (!app || renderer_get_backend(&app->renderer) != RENDERER_BACKEND_VULKAN) {
        return;
    }
    budget->enabled = true;
    budget->total_cap = layer_policy_vk_polygon_fill_index_budget(app->camera.zoom, app->visible_tile_count);
    budget->layer_cap[TILE_LAYER_POLY_WATER] =
        layer_policy_vk_polygon_fill_layer_budget(TILE_LAYER_POLY_WATER, budget->total_cap);
    budget->layer_cap[TILE_LAYER_POLY_PARK] =
        layer_policy_vk_polygon_fill_layer_budget(TILE_LAYER_POLY_PARK, budget->total_cap);
    budget->layer_cap[TILE_LAYER_POLY_LANDUSE] =
        layer_policy_vk_polygon_fill_layer_budget(TILE_LAYER_POLY_LANDUSE, budget->total_cap);
    budget->layer_cap[TILE_LAYER_POLY_BUILDING] =
        layer_policy_vk_polygon_fill_layer_budget(TILE_LAYER_POLY_BUILDING, budget->total_cap);
}

static void app_init_vk_poly_asset_build_budget(AppState *app, VkPolyAssetBuildBudget *budget) {
    if (!budget) {
        return;
    }
    budget->cap = 0u;
    budget->used = 0u;
    if (!app || renderer_get_backend(&app->renderer) != RENDERER_BACKEND_VULKAN || !app->vk_assets_enabled) {
        return;
    }

    // Keep polygon asset creation tightly bounded to avoid frame hitches on dense tiles.
    if (app->visible_tile_count <= 2u) {
        budget->cap = 2u;
    } else {
        budget->cap = 1u;
    }
}

void app_draw_region_bounds(AppState *app) {
    if (!app || !app->region.has_bounds) {
        return;
    }

    MercatorMeters min_m = mercator_from_latlon((LatLon){app->region.min_lat, app->region.min_lon});
    MercatorMeters max_m = mercator_from_latlon((LatLon){app->region.max_lat, app->region.max_lon});

    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 0.0f;
    float y1 = 0.0f;
    map_world_to_screen(&app->camera, app->width, app->height, (float)min_m.x, (float)max_m.y, &x0, &y0);
    map_world_to_screen(&app->camera, app->width, app->height, (float)max_m.x, (float)min_m.y, &x1, &y1);

    float left = x0 < x1 ? x0 : x1;
    float right = x0 < x1 ? x1 : x0;
    float top = y0 < y1 ? y0 : y1;
    float bottom = y0 < y1 ? y1 : y0;

    renderer_set_draw_color(&app->renderer, 80, 140, 220, 200);
    SDL_FRect rect = {left, top, right - left, bottom - top};
    renderer_draw_rect(&app->renderer, &rect);
}

static void app_draw_vk_mesh_for_coord(AppState *app,
                                       const VkRendererLineMesh *mesh,
                                       TileCoord coord) {
#if defined(MAPFORGE_HAVE_VK)
    if (!app || !mesh || !app->renderer.vk) {
        return;
    }
    MapTileAffine affine;
    if (!map_tile_affine_from_camera(&app->camera, app->renderer.width, app->renderer.height, coord, &affine)) {
        return;
    }
    vk_renderer_draw_line_mesh_affine((VkRenderer *)app->renderer.vk,
                                      mesh,
                                      affine.m00,
                                      affine.m01,
                                      affine.m02,
                                      affine.m10,
                                      affine.m11,
                                      affine.m12);
#else
    (void)app;
    (void)mesh;
    (void)coord;
#endif
}

static void app_draw_vk_tri_mesh_for_coord(AppState *app,
                                           const VkRendererTriMesh *mesh,
                                           TileCoord coord) {
#if defined(MAPFORGE_HAVE_VK)
    if (!app || !mesh || !app->renderer.vk) {
        return;
    }
    MapTileAffine affine;
    if (!map_tile_affine_from_camera(&app->camera, app->renderer.width, app->renderer.height, coord, &affine)) {
        return;
    }
    vk_renderer_draw_tri_mesh_affine((VkRenderer *)app->renderer.vk,
                                     mesh,
                                     affine.m00,
                                     affine.m01,
                                     affine.m02,
                                     affine.m10,
                                     affine.m11,
                                     affine.m12);
#else
    (void)app;
    (void)mesh;
    (void)coord;
#endif
}

static bool app_pick_road_tile_with_fallback(const AppState *app,
                                             TileLayerKind kind,
                                             TileCoord coord,
                                             const MftTile **out_tile,
                                             TileZoomBand *out_band) {
    if (!app || !out_tile || !out_band) {
        return false;
    }
    TileZoomBand target = app->layer_target_band[kind];
    TileZoomBand candidates[4] = {
        target,
        TILE_BAND_MID,
        TILE_BAND_COARSE,
        TILE_BAND_DEFAULT
    };
    uint32_t count = 1u;
    if (target == TILE_BAND_FINE) {
        candidates[1] = TILE_BAND_MID;
        candidates[2] = TILE_BAND_COARSE;
        candidates[3] = TILE_BAND_DEFAULT;
        count = 4u;
    } else if (target == TILE_BAND_MID) {
        candidates[1] = TILE_BAND_COARSE;
        candidates[2] = TILE_BAND_DEFAULT;
        count = 3u;
    } else if (target == TILE_BAND_COARSE) {
        candidates[1] = TILE_BAND_DEFAULT;
        count = 2u;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        TileZoomBand band = candidates[i];
        const MftTile *tile = tile_manager_peek_tile(&app->tile_managers[kind], coord, band);
        if (!tile) {
            continue;
        }
        *out_tile = tile;
        *out_band = band;
        return true;
    }
    return false;
}

static bool app_pick_polygon_tile_with_fallback(const AppState *app,
                                                TileLayerKind kind,
                                                TileCoord coord,
                                                const MftTile **out_tile,
                                                TileZoomBand *out_band) {
    if (!app || !out_tile || !out_band) {
        return false;
    }
    TileZoomBand target = app->layer_target_band[kind];
    TileZoomBand candidates[4] = {
        target,
        TILE_BAND_MID,
        TILE_BAND_COARSE,
        TILE_BAND_DEFAULT
    };
    uint32_t count = 1u;
    if (kind == TILE_LAYER_POLY_BUILDING) {
        // Buildings are intentionally conservative in band rollout.
        candidates[0] = target;
        candidates[1] = TILE_BAND_DEFAULT;
        count = (target == TILE_BAND_DEFAULT) ? 1u : 2u;
    } else if (target == TILE_BAND_FINE) {
        count = 4u;
    } else if (target == TILE_BAND_MID) {
        candidates[1] = TILE_BAND_COARSE;
        candidates[2] = TILE_BAND_DEFAULT;
        count = 3u;
    } else if (target == TILE_BAND_COARSE) {
        candidates[1] = TILE_BAND_DEFAULT;
        count = 2u;
    }
    for (uint32_t i = 0u; i < count; ++i) {
        TileZoomBand band = candidates[i];
        const MftTile *tile = tile_manager_peek_tile(&app->tile_managers[kind], coord, band);
        if (!tile) {
            continue;
        }
        *out_tile = tile;
        *out_band = band;
        return true;
    }
    return false;
}

static bool app_try_draw_vk_cached_tile(AppState *app, TileLayerKind kind, TileCoord coord, TileZoomBand band) {
    if (!app || renderer_get_backend(&app->renderer) != RENDERER_BACKEND_VULKAN || !app->vk_assets_enabled) {
        return false;
    }
#if defined(MAPFORGE_HAVE_VK)
    const VkTileCacheEntry *asset = vk_tile_cache_peek(&app->vk_tile_cache, kind, coord, band);
    if (!asset) {
        return false;
    }
    if (kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL) {
        bool drew_any = false;
        for (int rc = ROAD_CLASS_MOTORWAY; rc <= ROAD_CLASS_PATH; ++rc) {
            if (!asset->road_mesh_ready[rc]) {
                continue;
            }
            app_draw_vk_mesh_for_coord(app, &asset->road_mesh[rc], coord);
            drew_any = true;
        }
        return drew_any;
    }
    if (!asset->mesh_ready) {
        return false;
    }
    app_draw_vk_mesh_for_coord(app, &asset->mesh, coord);
    return true;
#else
    (void)kind;
    (void)coord;
    return false;
#endif
}

static bool app_try_draw_vk_cached_polygon_tile(AppState *app,
                                                TileLayerKind kind,
                                                TileCoord coord,
                                                TileZoomBand band,
                                                VkPolyFillBudget *budget,
                                                VkPolyAssetBuildBudget *asset_build_budget) {
    if (!app || renderer_get_backend(&app->renderer) != RENDERER_BACKEND_VULKAN || !app->vk_assets_enabled) {
        return false;
    }
#if defined(MAPFORGE_HAVE_VK)
    const VkTileCacheEntry *asset = vk_tile_cache_peek(&app->vk_tile_cache, kind, coord, band);
    (void)asset_build_budget;
    if (!asset) {
        return false;
    }

    bool drew_any = false;
    bool allow_fill = !app->polygon_outline_only;
    if (kind == TILE_LAYER_POLY_BUILDING && !app->building_fill_enabled) {
        allow_fill = false;
    }
    if (allow_fill) {
        if (asset->fill_mesh_ready) {
            uint32_t need = asset->fill_mesh.index_count;
            bool budget_ok = true;
            if (budget && budget->enabled) {
                uint32_t kind_cap = (kind < TILE_LAYER_COUNT) ? budget->layer_cap[kind] : 0u;
                uint32_t kind_used = (kind < TILE_LAYER_COUNT) ? budget->layer_used[kind] : 0u;
                if (budget->total_used + need > budget->total_cap ||
                    (kind_cap > 0u && kind_used + need > kind_cap)) {
                    budget_ok = false;
                }
            }
            if (budget_ok) {
                app_draw_vk_tri_mesh_for_coord(app, &asset->fill_mesh, coord);
                app->vk_poly_fill_drawn += 1u;
                app->vk_poly_fill_indices += need;
                if (budget && budget->enabled) {
                    budget->total_used += need;
                    if (kind < TILE_LAYER_COUNT) {
                        budget->layer_used[kind] += need;
                    }
                }
                drew_any = true;
            } else {
                app->vk_poly_fill_skip += 1u;
            }
        } else {
            app->vk_poly_fill_fail += 1u;
        }
    }

    if (kind == TILE_LAYER_POLY_WATER) {
        uint32_t lod = 2u;
        if (app->camera.zoom < 13.8f) {
            lod = 0u;
        } else if (app->camera.zoom < 15.0f) {
            lod = 1u;
        }
        if (asset->water_lod_mesh_ready[lod]) {
            app_draw_vk_mesh_for_coord(app, &asset->water_lod_mesh[lod], coord);
            drew_any = true;
        } else if (asset->water_lod_mesh_ready[2]) {
            app_draw_vk_mesh_for_coord(app, &asset->water_lod_mesh[2], coord);
            drew_any = true;
        } else if (asset->water_lod_mesh_ready[1]) {
            app_draw_vk_mesh_for_coord(app, &asset->water_lod_mesh[1], coord);
            drew_any = true;
        } else if (asset->water_lod_mesh_ready[0]) {
            app_draw_vk_mesh_for_coord(app, &asset->water_lod_mesh[0], coord);
            drew_any = true;
        }
    } else if (asset->mesh_ready) {
        app_draw_vk_mesh_for_coord(app, &asset->mesh, coord);
        drew_any = true;
    }
    return drew_any;
#else
    (void)kind;
    (void)coord;
    (void)budget;
    return false;
#endif
}

uint32_t app_draw_visible_tiles(AppState *app) {
    if (!app || !app->visible_valid) {
        return 0;
    }

    uint32_t visible = 0;
    bool vk_backend = renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN;
    bool allow_immediate_polygon_fallback = !(vk_backend && app->vk_assets_enabled);
    uint32_t vk_asset_misses = 0u;
    uint32_t total_line_budget = app->renderer.vk_line_budget;
    uint32_t reserve_line_budget = 0u;
    uint32_t road_line_budget = total_line_budget;
    if (vk_backend && total_line_budget > 0u) {
        reserve_line_budget = layer_policy_vk_reserved_line_budget(total_line_budget);
        road_line_budget = layer_policy_vk_road_line_budget(
            total_line_budget,
            reserve_line_budget,
            app_layer_runtime_enabled(app, TILE_LAYER_POLY_BUILDING));
        app->renderer.vk_line_budget = road_line_budget;
    }

    VkPolyFillBudget poly_fill_budget = {0};
    VkPolyAssetBuildBudget poly_asset_build_budget = {0};
    app_init_vk_poly_fill_budget(app, &poly_fill_budget);
    app_init_vk_poly_asset_build_budget(app, &poly_asset_build_budget);
    app->vk_poly_fill_drawn = 0u;
    app->vk_poly_fill_skip = 0u;
    app->vk_poly_fill_fail = 0u;
    app->vk_poly_fill_indices = 0u;
    app->vk_road_band_fallback_draws = 0u;
    uint32_t expected = 0;
    uint32_t done = 0;
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy || !app_layer_active_runtime(app, policy->kind)) {
            continue;
        }
        if (layer_policy_requires_full_ready(policy->kind)) {
            expected += app->layer_expected[policy->kind];
            done += app->layer_done[policy->kind];
        } else {
            expected += app->layer_visible_expected[policy->kind];
            done += app->layer_visible_loaded[policy->kind];
        }
    }

    for (uint32_t y = app->visible_top_left.y; y <= app->visible_bottom_right.y; ++y) {
        for (uint32_t x = app->visible_top_left.x; x <= app->visible_bottom_right.x; ++x) {
            TileCoord coord = {app->visible_zoom, x, y};
            bool local_ready = !layer_policy_requires_full_ready(TILE_LAYER_ROAD_LOCAL) ||
                app->layer_state[TILE_LAYER_ROAD_LOCAL] == LAYER_READINESS_READY;
            bool contour_ready = !layer_policy_requires_full_ready(TILE_LAYER_CONTOUR) ||
                app->layer_state[TILE_LAYER_CONTOUR] == LAYER_READINESS_READY;
            TileZoomBand local_band = app->layer_target_band[TILE_LAYER_ROAD_LOCAL];
            TileZoomBand contour_band = app->layer_target_band[TILE_LAYER_CONTOUR];
            TileZoomBand artery_band = app->layer_target_band[TILE_LAYER_ROAD_ARTERY];
            const MftTile *local = NULL;
            if (app_layer_active_runtime(app, TILE_LAYER_ROAD_LOCAL) && local_ready) {
                app_pick_road_tile_with_fallback(app, TILE_LAYER_ROAD_LOCAL, coord, &local, &local_band);
            }
            const MftTile *contour = (app_layer_active_runtime(app, TILE_LAYER_CONTOUR) &&
                contour_ready)
                ? tile_manager_peek_tile(&app->tile_managers[TILE_LAYER_CONTOUR], coord, contour_band) : NULL;
            const MftTile *artery = NULL;
            app_pick_road_tile_with_fallback(app, TILE_LAYER_ROAD_ARTERY, coord, &artery, &artery_band);
            if (local && local_band != app->layer_target_band[TILE_LAYER_ROAD_LOCAL]) {
                app->vk_road_band_fallback_draws += 1u;
            }
            if (artery && artery_band != app->layer_target_band[TILE_LAYER_ROAD_ARTERY]) {
                app->vk_road_band_fallback_draws += 1u;
            }
            if (local) {
                if (app_try_draw_vk_cached_tile(app, TILE_LAYER_ROAD_LOCAL, coord, local_band)) {
                } else {
                    if (vk_backend && app->vk_assets_enabled) {
                        vk_asset_misses += 1u;
                        app_vk_asset_enqueue(app, TILE_LAYER_ROAD_LOCAL, coord, local_band);
                    }
                    road_renderer_draw_tile(&app->renderer, &app->camera, local, app->single_line, app->road_zoom_bias);
                }
                visible += 1;
            }
            if (artery) {
                if (vk_backend && app->vk_assets_enabled &&
                    !vk_tile_cache_peek(&app->vk_tile_cache, TILE_LAYER_ROAD_ARTERY, coord, artery_band)) {
                    vk_asset_misses += 1u;
                    continue;
                }
                if (app_try_draw_vk_cached_tile(app, TILE_LAYER_ROAD_ARTERY, coord, artery_band)) {
                } else {
                    if (vk_backend && app->vk_assets_enabled) {
                        vk_asset_misses += 1u;
                        app_vk_asset_enqueue(app, TILE_LAYER_ROAD_ARTERY, coord, artery_band);
                    }
                    road_renderer_draw_tile(&app->renderer, &app->camera, artery, app->single_line, app->road_zoom_bias);
                }
                visible += 1;
            }
            if (contour) {
                road_renderer_draw_tile(&app->renderer, &app->camera, contour, true, 0.0f);
                visible += 1;
            }
        }
    }

    if (vk_backend && total_line_budget > 0u) {
        uint32_t polygon_cap = layer_policy_vk_polygon_line_budget_cap(
            app->renderer.vk_lines_drawn,
            total_line_budget,
            reserve_line_budget,
            road_line_budget);
        app->renderer.vk_line_budget = polygon_cap;
    }

    for (uint32_t y = app->visible_top_left.y; y <= app->visible_bottom_right.y; ++y) {
        for (uint32_t x = app->visible_top_left.x; x <= app->visible_bottom_right.x; ++x) {
            TileCoord coord = {app->visible_zoom, x, y};
            bool water_ready = !layer_policy_requires_full_ready(TILE_LAYER_POLY_WATER) ||
                app->layer_state[TILE_LAYER_POLY_WATER] == LAYER_READINESS_READY;
            bool park_ready = !layer_policy_requires_full_ready(TILE_LAYER_POLY_PARK) ||
                app->layer_state[TILE_LAYER_POLY_PARK] == LAYER_READINESS_READY;
            bool landuse_ready = !layer_policy_requires_full_ready(TILE_LAYER_POLY_LANDUSE) ||
                app->layer_state[TILE_LAYER_POLY_LANDUSE] == LAYER_READINESS_READY;
            bool building_ready = !layer_policy_requires_full_ready(TILE_LAYER_POLY_BUILDING) ||
                app->layer_state[TILE_LAYER_POLY_BUILDING] == LAYER_READINESS_READY;
            const MftTile *water = NULL;
            const MftTile *park = NULL;
            const MftTile *landuse = NULL;
            const MftTile *building = NULL;
            TileZoomBand water_band = app->layer_target_band[TILE_LAYER_POLY_WATER];
            TileZoomBand park_band = app->layer_target_band[TILE_LAYER_POLY_PARK];
            TileZoomBand landuse_band = app->layer_target_band[TILE_LAYER_POLY_LANDUSE];
            TileZoomBand building_band = app->layer_target_band[TILE_LAYER_POLY_BUILDING];
            if (app_layer_active_runtime(app, TILE_LAYER_POLY_WATER) && water_ready) {
                app_pick_polygon_tile_with_fallback(app, TILE_LAYER_POLY_WATER, coord, &water, &water_band);
            }
            if (app_layer_active_runtime(app, TILE_LAYER_POLY_PARK) && park_ready) {
                app_pick_polygon_tile_with_fallback(app, TILE_LAYER_POLY_PARK, coord, &park, &park_band);
            }
            if (app_layer_active_runtime(app, TILE_LAYER_POLY_LANDUSE) && landuse_ready) {
                app_pick_polygon_tile_with_fallback(app, TILE_LAYER_POLY_LANDUSE, coord, &landuse, &landuse_band);
            }
            if (app_layer_active_runtime(app, TILE_LAYER_POLY_BUILDING) && building_ready) {
                app_pick_polygon_tile_with_fallback(app, TILE_LAYER_POLY_BUILDING, coord, &building, &building_band);
            }
            if (water) {
                if (!app_try_draw_vk_cached_polygon_tile(
                        app, TILE_LAYER_POLY_WATER, coord, water_band, &poly_fill_budget, &poly_asset_build_budget)) {
                    if (vk_backend && app->vk_assets_enabled) {
                        vk_asset_misses += 1u;
                        app_vk_asset_enqueue(app, TILE_LAYER_POLY_WATER, coord, water_band);
                    }
                    if (allow_immediate_polygon_fallback) {
                        polygon_renderer_draw_tile(&app->renderer, &app->camera, (MftTile *)water, app->show_landuse,
                            app->building_zoom_bias, app->building_fill_enabled, app->polygon_outline_only);
                    }
                }
            }
            if (park) {
                if (!app_try_draw_vk_cached_polygon_tile(
                        app, TILE_LAYER_POLY_PARK, coord, park_band, &poly_fill_budget, &poly_asset_build_budget)) {
                    if (vk_backend && app->vk_assets_enabled) {
                        vk_asset_misses += 1u;
                        app_vk_asset_enqueue(app, TILE_LAYER_POLY_PARK, coord, park_band);
                    }
                    if (allow_immediate_polygon_fallback) {
                        polygon_renderer_draw_tile(&app->renderer, &app->camera, (MftTile *)park, app->show_landuse,
                            app->building_zoom_bias, app->building_fill_enabled, app->polygon_outline_only);
                    }
                }
            }
            if (landuse) {
                if (!app_try_draw_vk_cached_polygon_tile(
                        app, TILE_LAYER_POLY_LANDUSE, coord, landuse_band, &poly_fill_budget, &poly_asset_build_budget)) {
                    if (vk_backend && app->vk_assets_enabled) {
                        vk_asset_misses += 1u;
                        app_vk_asset_enqueue(app, TILE_LAYER_POLY_LANDUSE, coord, landuse_band);
                    }
                    if (allow_immediate_polygon_fallback) {
                        polygon_renderer_draw_tile(&app->renderer, &app->camera, (MftTile *)landuse, app->show_landuse,
                            app->building_zoom_bias, app->building_fill_enabled, app->polygon_outline_only);
                    }
                }
            }
            if (building) {
                if (!app_try_draw_vk_cached_polygon_tile(
                        app, TILE_LAYER_POLY_BUILDING, coord, building_band, &poly_fill_budget, &poly_asset_build_budget)) {
                    if (vk_backend && app->vk_assets_enabled) {
                        vk_asset_misses += 1u;
                        app_vk_asset_enqueue(app, TILE_LAYER_POLY_BUILDING, coord, building_band);
                    }
                    if (allow_immediate_polygon_fallback) {
                        polygon_renderer_draw_tile(&app->renderer, &app->camera, (MftTile *)building, app->show_landuse,
                            app->building_zoom_bias, app->building_fill_enabled, app->polygon_outline_only);
                    }
                }
            }
        }
    }

    if (vk_backend && total_line_budget > 0u) {
        app->renderer.vk_line_budget = total_line_budget;
    }

    app->loading_expected = expected;
    app->loading_done = done;
    app->vk_asset_misses = vk_asset_misses;

    return visible;
}
