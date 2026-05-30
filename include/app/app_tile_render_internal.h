#ifndef MAPFORGE_APP_TILE_RENDER_INTERNAL_H
#define MAPFORGE_APP_TILE_RENDER_INTERNAL_H

#include "app/app_internal.h"

void app_draw_visible_tiles(AppState *app, AppVisibleTileRenderStats *out_stats);
void app_draw_region_bounds(AppState *app);
void app_tile_presenter_reset_frame_counters(AppState *app);
float app_tile_presenter_band_blend_mix(const AppState *app, TileLayerKind kind, double now_sec);
bool app_tile_presenter_peek_tile_for_band(const AppState *app,
                                           TileLayerKind kind,
                                           TileCoord coord,
                                           TileZoomBand band,
                                           const MftTile **out_tile);
bool app_tile_presenter_pick_tile_with_fallback(const AppState *app,
                                                TileLayerKind kind,
                                                TileCoord coord,
                                                const MftTile **out_tile,
                                                TileZoomBand *out_band);
bool app_tile_presenter_resolve_tile_for_present(AppState *app,
                                                 TileLayerKind kind,
                                                 TileCoord coord,
                                                 double now_sec,
                                                 const MftTile **out_tile,
                                                 TileZoomBand *out_band);
void app_tile_presenter_present_hold_remember(AppState *app,
                                              TileLayerKind kind,
                                              TileCoord coord,
                                              TileZoomBand band,
                                              double now_sec);
bool app_tile_presenter_present_hold_lookup(AppState *app,
                                            TileLayerKind kind,
                                            TileCoord coord,
                                            double now_sec,
                                            TileZoomBand *out_band);
bool app_tile_presenter_draw_polygon_band_blend(AppState *app,
                                                TileLayerKind kind,
                                                TileCoord coord,
                                                float building_zoom_bias,
                                                float layer_opacity,
                                                double now_sec);
bool app_tile_presenter_draw_road_band_blend(AppState *app,
                                             TileLayerKind kind,
                                             TileCoord coord,
                                             bool single_line,
                                             float road_zoom_bias,
                                             float road_opacity,
                                             double now_sec);
bool app_tile_presenter_draw_road_layer(AppState *app,
                                        TileLayerKind kind,
                                        TileCoord coord,
                                        const MftTile *tile,
                                        TileZoomBand band,
                                        bool single_line,
                                        float road_zoom_bias,
                                        float road_opacity,
                                        double now_sec,
                                        uint32_t *io_vk_asset_misses);
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
                                           uint32_t *io_vk_asset_misses);
bool app_tile_presenter_validate_frame_invariants(AppState *app,
                                                  uint32_t visible_tiles,
                                                  uint32_t vk_asset_misses);

#endif
