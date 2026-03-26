#include "app/app_internal.h"

#include "core/time.h"
#include "core/log.h"
#include "map/mercator.h"
#include "map/map_space.h"
#include "map/polygon_renderer.h"
#include "map/road_renderer.h"

#include <stdlib.h>
#include <string.h>

static float app_policy_zoom(const AppState *app) {
    if (!app) {
        return 0.0f;
    }
    return app->view_state_bridge.zoom_logic_enabled ? app->view_state_bridge.camera.zoom : 16.0f;
}

static float app_layer_opacity_scale(const AppState *app, TileLayerKind kind) {
    if (!app || kind < 0 || kind >= TILE_LAYER_COUNT) {
        return 1.0f;
    }
    float value = (float)app->view_state_bridge.layer_opacity_milli[kind] / 1000.0f;
    if (value < 0.0f) {
        value = 0.0f;
    }
    if (value > 1.0f) {
        value = 1.0f;
    }
    return value * app_layer_fade_multiplier(app, kind);
}

static bool app_has_custom_layer_opacity(const AppState *app) {
    if (!app) {
        return false;
    }
    for (int i = 0; i < TILE_LAYER_COUNT; ++i) {
        if (app->view_state_bridge.layer_opacity_milli[i] < 1000u) {
            return true;
        }
    }
    if (app->view_state_bridge.zoom_logic_enabled) {
        const TileLayerKind polygon_layers[] = {
            TILE_LAYER_POLY_WATER,
            TILE_LAYER_POLY_PARK,
            TILE_LAYER_POLY_LANDUSE,
            TILE_LAYER_POLY_BUILDING
        };
        for (size_t i = 0; i < sizeof(polygon_layers) / sizeof(polygon_layers[0]); ++i) {
            if (app_layer_fade_multiplier(app, polygon_layers[i]) < 0.999f) {
                return true;
            }
        }
    }
    return false;
}

static void app_init_vk_poly_fill_budget(AppState *app, VkPolyFillBudget *budget) {
    if (!budget) {
        return;
    }
    memset(budget, 0, sizeof(*budget));
    if (!app || renderer_get_backend(&app->renderer) != RENDERER_BACKEND_VULKAN) {
        return;
    }
    budget->enabled = true;
    budget->total_cap = layer_policy_vk_polygon_fill_index_budget(app_policy_zoom(app), app->tile_state_bridge.visible_tile_count);
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
    if (!app || renderer_get_backend(&app->renderer) != RENDERER_BACKEND_VULKAN || !app->tile_state_bridge.vk_assets_enabled) {
        return;
    }

    // Keep polygon asset creation tightly bounded to avoid frame hitches on dense tiles.
    if (app->tile_state_bridge.visible_tile_count <= 2u) {
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
    map_world_to_screen(&app->view_state_bridge.camera, app->width, app->height, (float)min_m.x, (float)max_m.y, &x0, &y0);
    map_world_to_screen(&app->view_state_bridge.camera, app->width, app->height, (float)max_m.x, (float)min_m.y, &x1, &y1);

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
    if (!map_tile_affine_from_camera(&app->view_state_bridge.camera, app->renderer.width, app->renderer.height, coord, &affine)) {
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

static void app_draw_vk_mesh_for_coord_tinted(AppState *app,
                                              const VkRendererLineMesh *mesh,
                                              TileCoord coord,
                                              float tint_r,
                                              float tint_g,
                                              float tint_b,
                                              float tint_a) {
#if defined(MAPFORGE_HAVE_VK)
    if (!app || !mesh || !app->renderer.vk) {
        return;
    }
    MapTileAffine affine;
    if (!map_tile_affine_from_camera(&app->view_state_bridge.camera, app->renderer.width, app->renderer.height, coord, &affine)) {
        return;
    }
    if (tint_r < 0.0f) tint_r = 0.0f;
    if (tint_g < 0.0f) tint_g = 0.0f;
    if (tint_b < 0.0f) tint_b = 0.0f;
    if (tint_a < 0.0f) tint_a = 0.0f;
    if (tint_r > 1.0f) tint_r = 1.0f;
    if (tint_g > 1.0f) tint_g = 1.0f;
    if (tint_b > 1.0f) tint_b = 1.0f;
    if (tint_a > 1.0f) tint_a = 1.0f;
    vk_renderer_draw_line_mesh_affine_tinted((VkRenderer *)app->renderer.vk,
                                             mesh,
                                             affine.m00,
                                             affine.m01,
                                             affine.m02,
                                             affine.m10,
                                             affine.m11,
                                             affine.m12,
                                             tint_r,
                                             tint_g,
                                             tint_b,
                                             tint_a);
#else
    (void)app;
    (void)mesh;
    (void)coord;
    (void)tint_r;
    (void)tint_g;
    (void)tint_b;
    (void)tint_a;
#endif
}

static float app_road_zoom_alpha_scale(RoadClass road_class, float zoom) {
    switch (road_class) {
        case ROAD_CLASS_SECONDARY:
            if (zoom <= 10.0f) return 0.68f;
            if (zoom <= 11.0f) return 0.80f;
            if (zoom <= 12.0f) return 0.92f;
            return 1.0f;
        case ROAD_CLASS_TERTIARY:
            if (zoom <= 10.0f) return 0.52f;
            if (zoom <= 11.0f) return 0.64f;
            if (zoom <= 12.0f) return 0.76f;
            if (zoom <= 13.0f) return 0.88f;
            return 1.0f;
        case ROAD_CLASS_RESIDENTIAL:
            if (zoom <= 10.0f) return 0.26f;
            if (zoom <= 11.0f) return 0.38f;
            if (zoom <= 12.0f) return 0.52f;
            if (zoom <= 13.0f) return 0.70f;
            return 1.0f;
        case ROAD_CLASS_SERVICE:
            if (zoom <= 10.0f) return 0.18f;
            if (zoom <= 11.0f) return 0.28f;
            if (zoom <= 12.0f) return 0.40f;
            if (zoom <= 13.0f) return 0.58f;
            return 1.0f;
        case ROAD_CLASS_FOOTWAY:
            if (zoom <= 10.0f) return 0.12f;
            if (zoom <= 11.0f) return 0.20f;
            if (zoom <= 12.0f) return 0.30f;
            if (zoom <= 13.0f) return 0.50f;
            return 0.80f;
        case ROAD_CLASS_PATH:
            if (zoom <= 10.0f) return 0.10f;
            if (zoom <= 11.0f) return 0.16f;
            if (zoom <= 12.0f) return 0.24f;
            if (zoom <= 13.0f) return 0.42f;
            return 0.70f;
        default:
            return 1.0f;
    }
}

static float app_road_zoom_luma_scale(RoadClass road_class, float zoom) {
    switch (road_class) {
        case ROAD_CLASS_SECONDARY:
            if (zoom <= 10.0f) return 0.84f;
            if (zoom <= 11.0f) return 0.90f;
            if (zoom <= 12.0f) return 0.96f;
            return 1.0f;
        case ROAD_CLASS_TERTIARY:
            if (zoom <= 10.0f) return 0.70f;
            if (zoom <= 11.0f) return 0.78f;
            if (zoom <= 12.0f) return 0.86f;
            if (zoom <= 13.0f) return 0.92f;
            return 1.0f;
        case ROAD_CLASS_RESIDENTIAL:
            if (zoom <= 10.0f) return 0.52f;
            if (zoom <= 11.0f) return 0.62f;
            if (zoom <= 12.0f) return 0.74f;
            if (zoom <= 13.0f) return 0.86f;
            return 1.0f;
        case ROAD_CLASS_SERVICE:
            if (zoom <= 10.0f) return 0.42f;
            if (zoom <= 11.0f) return 0.54f;
            if (zoom <= 12.0f) return 0.66f;
            if (zoom <= 13.0f) return 0.80f;
            return 1.0f;
        case ROAD_CLASS_FOOTWAY:
            if (zoom <= 10.0f) return 0.36f;
            if (zoom <= 11.0f) return 0.48f;
            if (zoom <= 12.0f) return 0.60f;
            if (zoom <= 13.0f) return 0.74f;
            return 0.88f;
        case ROAD_CLASS_PATH:
            if (zoom <= 10.0f) return 0.32f;
            if (zoom <= 11.0f) return 0.42f;
            if (zoom <= 12.0f) return 0.54f;
            if (zoom <= 13.0f) return 0.70f;
            return 0.84f;
        default:
            return 1.0f;
    }
}

static void app_draw_vk_tri_mesh_for_coord(AppState *app,
                                           const VkRendererTriMesh *mesh,
                                           TileCoord coord) {
#if defined(MAPFORGE_HAVE_VK)
    if (!app || !mesh || !app->renderer.vk) {
        return;
    }
    MapTileAffine affine;
    if (!map_tile_affine_from_camera(&app->view_state_bridge.camera, app->renderer.width, app->renderer.height, coord, &affine)) {
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

static uint32_t app_visible_ring_distance(const AppState *app, TileCoord coord) {
    if (!app || !app->tile_state_bridge.visible_valid || coord.z != app->tile_state_bridge.visible_zoom) {
        return UINT32_MAX / 4u;
    }

    TileCoord center = {
        app->tile_state_bridge.visible_zoom,
        (app->tile_state_bridge.visible_top_left.x + app->tile_state_bridge.visible_bottom_right.x) / 2u,
        (app->tile_state_bridge.visible_top_left.y + app->tile_state_bridge.visible_bottom_right.y) / 2u
    };
    uint32_t dx = (coord.x > center.x) ? (coord.x - center.x) : (center.x - coord.x);
    uint32_t dy = (coord.y > center.y) ? (coord.y - center.y) : (center.y - coord.y);
    return dx + dy;
}

static bool app_allow_immediate_building_fallback(const AppState *app, TileCoord coord) {
    // Keep Vulkan immediate fallback constrained to center/near-center tiles to
    // avoid wide-frame fallback spikes while preserving progressive building visibility.
    return app_visible_ring_distance(app, coord) <= 1u;
}

typedef struct BuildingTileDebugStats {
    uint32_t tiles;
    uint32_t polygons;
    uint32_t rings;
    uint32_t points_total;
    uint32_t points_min;
    uint32_t points_max;
    uint32_t rings_lt3;
    uint32_t rings_eq3;
    uint32_t rings_eq4;
} BuildingTileDebugStats;

static bool app_building_debug_enabled(void) {
    static int cached = -1;
    if (cached >= 0) {
        return cached != 0;
    }
    const char *value = getenv("MAPFORGE_BUILDING_DEBUG");
    cached = 0;
    if (value && value[0] != '\0' &&
        (strcmp(value, "1") == 0 || strcmp(value, "true") == 0 || strcmp(value, "TRUE") == 0 ||
         strcmp(value, "yes") == 0 || strcmp(value, "YES") == 0 || strcmp(value, "on") == 0 ||
         strcmp(value, "ON") == 0)) {
        cached = 1;
    }
    return cached != 0;
}

static void app_accumulate_building_debug_stats(const MftTile *tile, BuildingTileDebugStats *stats) {
    if (!tile || !stats || !tile->polygons || !tile->polygon_rings) {
        return;
    }
    stats->tiles += 1u;
    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const MftPolygon *polygon = &tile->polygons[i];
        stats->polygons += 1u;
        for (uint16_t r = 0; r < polygon->ring_count; ++r) {
            uint32_t ring_index = polygon->ring_offset + r;
            if (ring_index >= tile->polygon_ring_total) {
                continue;
            }
            uint32_t ring_points = tile->polygon_rings[ring_index];
            stats->rings += 1u;
            stats->points_total += ring_points;
            if (stats->points_min == 0u || ring_points < stats->points_min) {
                stats->points_min = ring_points;
            }
            if (ring_points > stats->points_max) {
                stats->points_max = ring_points;
            }
            if (ring_points < 3u) {
                stats->rings_lt3 += 1u;
            } else if (ring_points == 3u) {
                stats->rings_eq3 += 1u;
            } else if (ring_points == 4u) {
                stats->rings_eq4 += 1u;
            }
        }
    }
}

bool app_try_draw_vk_cached_tile(AppState *app, TileLayerKind kind, TileCoord coord, TileZoomBand band) {
    if (!app || renderer_get_backend(&app->renderer) != RENDERER_BACKEND_VULKAN || !app->tile_state_bridge.vk_assets_enabled) {
        return false;
    }
#if defined(MAPFORGE_HAVE_VK)
    const VkTileCacheEntry *asset = vk_tile_cache_peek(&app->tile_state_bridge.vk_tile_cache, kind, coord, band);
    if (!asset) {
        return false;
    }
    if (kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL) {
        bool drew_any = false;
        float effective_zoom = app_policy_zoom(app) - (app->view_state_bridge.zoom_logic_enabled ? app->view_state_bridge.road_zoom_bias : -1000.0f);
        float layer_opacity = app_layer_opacity_scale(app, kind);
        if (effective_zoom < 0.0f) {
            effective_zoom = 0.0f;
        }
        for (int rc = ROAD_CLASS_MOTORWAY; rc <= ROAD_CLASS_PATH; ++rc) {
            if (!asset->road_mesh_ready[rc]) {
                continue;
            }
            RoadClass road_class = (RoadClass)rc;
            float alpha_scale = app_road_zoom_alpha_scale(road_class, effective_zoom);
            float luma_scale = app_road_zoom_luma_scale(road_class, effective_zoom);
            float tint_alpha = alpha_scale * layer_opacity;
            if (tint_alpha <= 0.0f) {
                continue;
            }
            app_draw_vk_mesh_for_coord_tinted(app, &asset->road_mesh[rc], coord,
                                              luma_scale, luma_scale, luma_scale, tint_alpha);
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

bool app_try_draw_vk_cached_polygon_tile(AppState *app,
                                         TileLayerKind kind,
                                         TileCoord coord,
                                         TileZoomBand band,
                                         VkPolyFillBudget *budget,
                                         VkPolyAssetBuildBudget *asset_build_budget) {
    if (!app || renderer_get_backend(&app->renderer) != RENDERER_BACKEND_VULKAN || !app->tile_state_bridge.vk_assets_enabled) {
        return false;
    }
#if defined(MAPFORGE_HAVE_VK)
    const VkTileCacheEntry *asset = vk_tile_cache_peek(&app->tile_state_bridge.vk_tile_cache, kind, coord, band);
    (void)asset_build_budget;
    if (app_layer_opacity_scale(app, kind) < 0.999f) {
        return false;
    }
    if (!asset) {
        return false;
    }

    bool drew_any = false;
    bool allow_fill = !app->view_state_bridge.polygon_outline_only;
    if (kind == TILE_LAYER_POLY_BUILDING && !app->view_state_bridge.building_fill_enabled) {
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
                app->tile_state_bridge.vk_poly_fill_drawn += 1u;
                app->tile_state_bridge.vk_poly_fill_indices += need;
                if (budget && budget->enabled) {
                    budget->total_used += need;
                    if (kind < TILE_LAYER_COUNT) {
                        budget->layer_used[kind] += need;
                    }
                }
                drew_any = true;
            } else {
                app->tile_state_bridge.vk_poly_fill_skip += 1u;
            }
        } else {
            app->tile_state_bridge.vk_poly_fill_fail += 1u;
        }
    }

    if (kind == TILE_LAYER_POLY_WATER) {
        uint32_t lod = 2u;
        float policy_zoom = app_policy_zoom(app);
        if (policy_zoom < 13.8f) {
            lod = 0u;
        } else if (policy_zoom < 15.0f) {
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
    if (!app || !app->tile_state_bridge.visible_valid) {
        return 0;
    }

    uint32_t visible = 0;
    double now_sec = time_now_seconds();
    bool vk_backend = renderer_get_backend(&app->renderer) == RENDERER_BACKEND_VULKAN;
    bool allow_immediate_polygon_fallback = !(vk_backend && app->tile_state_bridge.vk_assets_enabled) || app_has_custom_layer_opacity(app);
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
    app->tile_state_bridge.vk_poly_fill_drawn = 0u;
    app->tile_state_bridge.vk_poly_fill_skip = 0u;
    app->tile_state_bridge.vk_poly_fill_fail = 0u;
    app->tile_state_bridge.vk_poly_fill_indices = 0u;
    app->tile_state_bridge.vk_road_band_fallback_draws = 0u;
    app->tile_state_bridge.draw_path_vk_count = 0u;
    app->tile_state_bridge.draw_path_fallback_count = 0u;
    app_tile_presenter_reset_frame_counters(app);
    BuildingTileDebugStats building_debug = {0};
    uint32_t expected = 0;
    uint32_t done = 0;
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        const LayerPolicy *policy = layer_policy_at(i);
        if (!policy || !app_layer_active_runtime(app, policy->kind)) {
            continue;
        }
        if (layer_policy_requires_full_ready(policy->kind)) {
            expected += app->tile_state_bridge.layer_expected[policy->kind];
            done += app->tile_state_bridge.layer_done[policy->kind];
        } else {
            expected += app->tile_state_bridge.layer_visible_expected[policy->kind];
            done += app->tile_state_bridge.layer_visible_loaded[policy->kind];
        }
    }

    for (uint32_t y = app->tile_state_bridge.visible_top_left.y; y <= app->tile_state_bridge.visible_bottom_right.y; ++y) {
        for (uint32_t x = app->tile_state_bridge.visible_top_left.x; x <= app->tile_state_bridge.visible_bottom_right.x; ++x) {
            TileCoord coord = {app->tile_state_bridge.visible_zoom, x, y};
            bool local_ready = !layer_policy_requires_full_ready(TILE_LAYER_ROAD_LOCAL) ||
                app->tile_state_bridge.layer_state[TILE_LAYER_ROAD_LOCAL] == LAYER_READINESS_READY;
            bool contour_ready = !layer_policy_requires_full_ready(TILE_LAYER_CONTOUR) ||
                app->tile_state_bridge.layer_state[TILE_LAYER_CONTOUR] == LAYER_READINESS_READY;
            TileZoomBand local_band = app->tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_LOCAL];
            TileZoomBand contour_band = app->tile_state_bridge.layer_target_band[TILE_LAYER_CONTOUR];
            TileZoomBand artery_band = app->tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_ARTERY];
            const MftTile *local = NULL;
            if (app_layer_active_runtime(app, TILE_LAYER_ROAD_LOCAL) && local_ready) {
                app_tile_presenter_resolve_tile_for_present(app, TILE_LAYER_ROAD_LOCAL, coord, now_sec, &local, &local_band);
            }
            const MftTile *contour = (app_layer_active_runtime(app, TILE_LAYER_CONTOUR) &&
                contour_ready)
                ? tile_manager_peek_tile(&app->tile_state_bridge.tile_managers[TILE_LAYER_CONTOUR], coord, contour_band) : NULL;
            const MftTile *artery = NULL;
            app_tile_presenter_resolve_tile_for_present(app, TILE_LAYER_ROAD_ARTERY, coord, now_sec, &artery, &artery_band);
            if (local && local_band != app->tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_LOCAL]) {
                app->tile_state_bridge.vk_road_band_fallback_draws += 1u;
            }
            if (artery && artery_band != app->tile_state_bridge.layer_target_band[TILE_LAYER_ROAD_ARTERY]) {
                app->tile_state_bridge.vk_road_band_fallback_draws += 1u;
            }
            if (local) {
                float road_zoom_bias = app->view_state_bridge.zoom_logic_enabled ? app->view_state_bridge.road_zoom_bias : -1000.0f;
                float road_opacity = app_layer_opacity_scale(app, TILE_LAYER_ROAD_LOCAL);
                (void)app_tile_presenter_draw_road_layer(app,
                                                         TILE_LAYER_ROAD_LOCAL,
                                                         coord,
                                                         local,
                                                         local_band,
                                                         app->single_line,
                                                         road_zoom_bias,
                                                         road_opacity,
                                                         now_sec,
                                                         &vk_asset_misses);
                visible += 1;
            }
            if (artery) {
                float road_zoom_bias = app->view_state_bridge.zoom_logic_enabled ? app->view_state_bridge.road_zoom_bias : -1000.0f;
                float road_opacity = app_layer_opacity_scale(app, TILE_LAYER_ROAD_ARTERY);
                (void)app_tile_presenter_draw_road_layer(app,
                                                         TILE_LAYER_ROAD_ARTERY,
                                                         coord,
                                                         artery,
                                                         artery_band,
                                                         app->single_line,
                                                         road_zoom_bias,
                                                         road_opacity,
                                                         now_sec,
                                                         &vk_asset_misses);
                visible += 1;
            }
            if (contour) {
                float contour_opacity = app_layer_opacity_scale(app, TILE_LAYER_CONTOUR);
                road_renderer_draw_tile(&app->renderer, &app->view_state_bridge.camera, contour, true, 0.0f, contour_opacity);
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

    for (uint32_t y = app->tile_state_bridge.visible_top_left.y; y <= app->tile_state_bridge.visible_bottom_right.y; ++y) {
        for (uint32_t x = app->tile_state_bridge.visible_top_left.x; x <= app->tile_state_bridge.visible_bottom_right.x; ++x) {
            TileCoord coord = {app->tile_state_bridge.visible_zoom, x, y};
            const MftTile *water = NULL;
            const MftTile *park = NULL;
            const MftTile *landuse = NULL;
            const MftTile *building = NULL;
            TileZoomBand water_band = app->tile_state_bridge.layer_target_band[TILE_LAYER_POLY_WATER];
            TileZoomBand park_band = app->tile_state_bridge.layer_target_band[TILE_LAYER_POLY_PARK];
            TileZoomBand landuse_band = app->tile_state_bridge.layer_target_band[TILE_LAYER_POLY_LANDUSE];
            TileZoomBand building_band = app->tile_state_bridge.layer_target_band[TILE_LAYER_POLY_BUILDING];
            if (app_layer_active_runtime(app, TILE_LAYER_POLY_WATER)) {
                app_tile_presenter_resolve_tile_for_present(app, TILE_LAYER_POLY_WATER, coord, now_sec, &water, &water_band);
            }
            if (app_layer_active_runtime(app, TILE_LAYER_POLY_PARK)) {
                app_tile_presenter_resolve_tile_for_present(app, TILE_LAYER_POLY_PARK, coord, now_sec, &park, &park_band);
            }
            if (app_layer_active_runtime(app, TILE_LAYER_POLY_LANDUSE)) {
                app_tile_presenter_resolve_tile_for_present(app, TILE_LAYER_POLY_LANDUSE, coord, now_sec, &landuse, &landuse_band);
            }
            if (app_layer_active_runtime(app, TILE_LAYER_POLY_BUILDING)) {
                app_tile_presenter_resolve_tile_for_present(app, TILE_LAYER_POLY_BUILDING, coord, now_sec, &building, &building_band);
            }
            if (water) {
                float building_zoom_bias = app->view_state_bridge.zoom_logic_enabled ? app->view_state_bridge.building_zoom_bias : -1000.0f;
                float layer_opacity = app_layer_opacity_scale(app, TILE_LAYER_POLY_WATER);
                (void)app_tile_presenter_draw_polygon_layer(app,
                                                            TILE_LAYER_POLY_WATER,
                                                            coord,
                                                            water,
                                                            water_band,
                                                            building_zoom_bias,
                                                            layer_opacity,
                                                            allow_immediate_polygon_fallback,
                                                            false,
                                                            &poly_fill_budget,
                                                            &poly_asset_build_budget,
                                                            now_sec,
                                                            &vk_asset_misses);
            }
            if (park) {
                float building_zoom_bias = app->view_state_bridge.zoom_logic_enabled ? app->view_state_bridge.building_zoom_bias : -1000.0f;
                float layer_opacity = app_layer_opacity_scale(app, TILE_LAYER_POLY_PARK);
                (void)app_tile_presenter_draw_polygon_layer(app,
                                                            TILE_LAYER_POLY_PARK,
                                                            coord,
                                                            park,
                                                            park_band,
                                                            building_zoom_bias,
                                                            layer_opacity,
                                                            allow_immediate_polygon_fallback,
                                                            false,
                                                            &poly_fill_budget,
                                                            &poly_asset_build_budget,
                                                            now_sec,
                                                            &vk_asset_misses);
            }
            if (landuse) {
                float building_zoom_bias = app->view_state_bridge.zoom_logic_enabled ? app->view_state_bridge.building_zoom_bias : -1000.0f;
                float layer_opacity = app_layer_opacity_scale(app, TILE_LAYER_POLY_LANDUSE);
                (void)app_tile_presenter_draw_polygon_layer(app,
                                                            TILE_LAYER_POLY_LANDUSE,
                                                            coord,
                                                            landuse,
                                                            landuse_band,
                                                            building_zoom_bias,
                                                            layer_opacity,
                                                            allow_immediate_polygon_fallback,
                                                            false,
                                                            &poly_fill_budget,
                                                            &poly_asset_build_budget,
                                                            now_sec,
                                                            &vk_asset_misses);
            }
            if (building) {
                if (app_building_debug_enabled()) {
                    app_accumulate_building_debug_stats(building, &building_debug);
                }
                float building_zoom_bias = app->view_state_bridge.zoom_logic_enabled ? app->view_state_bridge.building_zoom_bias : -1000.0f;
                float layer_opacity = app_layer_opacity_scale(app, TILE_LAYER_POLY_BUILDING);
                bool allow_building_fallback = app_allow_immediate_building_fallback(app, coord);
                (void)app_tile_presenter_draw_polygon_layer(app,
                                                            TILE_LAYER_POLY_BUILDING,
                                                            coord,
                                                            building,
                                                            building_band,
                                                            building_zoom_bias,
                                                            layer_opacity,
                                                            allow_immediate_polygon_fallback,
                                                            allow_building_fallback,
                                                            &poly_fill_budget,
                                                            &poly_asset_build_budget,
                                                            now_sec,
                                                            &vk_asset_misses);
            }
        }
    }

    if (vk_backend && total_line_budget > 0u) {
        app->renderer.vk_line_budget = total_line_budget;
    }

    app->tile_state_bridge.loading_expected = expected;
    app->tile_state_bridge.loading_done = done;
    app->tile_state_bridge.vk_asset_misses = vk_asset_misses;
    if (app_building_debug_enabled() && building_debug.tiles > 0u) {
        static uint64_t next_log_ms = 0u;
        uint64_t now_ms = SDL_GetTicks64();
        if (now_ms >= next_log_ms) {
            uint32_t avg_points = building_debug.rings > 0u ? (building_debug.points_total / building_debug.rings) : 0u;
            log_info("building_debug tiles=%u polygons=%u rings=%u points(avg=%u min=%u max=%u) "
                     "rings_lt3=%u rings_eq3=%u rings_eq4=%u",
                     building_debug.tiles,
                     building_debug.polygons,
                     building_debug.rings,
                     avg_points,
                     building_debug.points_min,
                     building_debug.points_max,
                     building_debug.rings_lt3,
                     building_debug.rings_eq3,
                     building_debug.rings_eq4);
            next_log_ms = now_ms + 1000u;
        }
    }

    if (app->tile_state_bridge.presenter_invariants_enabled) {
        (void)app_tile_presenter_validate_frame_invariants(app, visible, vk_asset_misses);
    }

    return visible;
}
