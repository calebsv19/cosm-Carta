#ifndef MAPFORGE_APP_TILE_PIPELINE_HELPERS_H
#define MAPFORGE_APP_TILE_PIPELINE_HELPERS_H

#include "app/app_internal.h"

bool app_kind_is_polygon(TileLayerKind kind);
bool app_tile_request_in_region_coverage(const AppState *app,
                                         TileLayerKind kind,
                                         TileZoomBand band,
                                         TileCoord coord);
uint32_t app_tile_ring_distance_from_bounds(TileCoord coord,
                                            TileCoord top_left,
                                            TileCoord bottom_right);
TileZoomBand app_layer_target_band(const AppState *app, TileLayerKind kind);
void app_apply_layer_residency_budgets(AppState *app,
                                       TileCoord queue_top_left,
                                       TileCoord queue_bottom_right);
void app_refresh_visible_layer_coverage(AppState *app);
bool app_has_visible_tile_with_fallback(const AppState *app,
                                        TileLayerKind kind,
                                        TileCoord coord,
                                        TileZoomBand preferred_band);
float app_visible_layer_ideal_coverage_for_band(const AppState *app,
                                                TileLayerKind kind,
                                                TileZoomBand band);

// Bounds submission demand to the contiguous requests serviceable under existing lane caps.
uint32_t app_tile_queue_serviceable_count(const TileQueue *queue, uint32_t desired,
                                          const uint32_t lane_caps[TILE_QUEUE_LANE_COUNT]);

// Gives missing visible requests bounded priority over refinement and speculative prefetch.
uint32_t app_tile_visible_missing_budget(const TileQueue *queue, uint32_t base, uint32_t visible_cap);

// Keeps cheap submission throughput stable across slow frames, bounded to four nominal frames.
uint32_t app_tile_submission_frame_scale(float frame_ms);

#endif
