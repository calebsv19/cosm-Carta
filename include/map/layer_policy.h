#ifndef MAPFORGE_MAP_LAYER_POLICY_H
#define MAPFORGE_MAP_LAYER_POLICY_H

#include "map/mft_loader.h"
#include "map/tile_layers.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Stores centralized layer visibility and loading policy values.
typedef struct LayerPolicy {
    TileLayerKind kind;
    const char *label;
    float zoom_start;
    uint32_t load_priority;
    bool enabled;
    bool is_polygon;
    bool requires_full_layer_ready;
} LayerPolicy;

// Describes current per-layer readiness used by load-aware rendering.
typedef enum LayerReadinessState {
    LAYER_READINESS_HIDDEN = 0,
    LAYER_READINESS_LOADING = 1,
    LAYER_READINESS_READY = 2
} LayerReadinessState;

// Returns the number of configured layers in load/render order.
size_t layer_policy_count(void);

// Returns policy entry by order index, or NULL for invalid index.
const LayerPolicy *layer_policy_at(size_t index);

// Returns policy entry by layer kind, or NULL when unknown.
const LayerPolicy *layer_policy_for(TileLayerKind kind);

// Returns stable human-readable layer label.
const char *layer_policy_label(TileLayerKind kind);

// Returns effective zoom start for a layer.
float layer_policy_zoom_start(TileLayerKind kind, float building_zoom_bias);

// Returns true if the layer should be considered active at this zoom.
bool layer_policy_layer_active(TileLayerKind kind, float zoom, float building_zoom_bias);

// Returns true when this layer should only render after full load completion.
bool layer_policy_requires_full_ready(TileLayerKind kind);

// Returns far/mid/near/close/path max tier boundaries for zoom fade logic.
float layer_policy_zoom_tier_far_max(void);
float layer_policy_zoom_tier_mid_max(void);
float layer_policy_zoom_tier_near_max(void);
float layer_policy_zoom_tier_close_max(void);
float layer_policy_zoom_tier_path_max(void);

// Returns Vulkan road class visibility gate for a given effective zoom.
bool layer_policy_vk_road_class_allowed(RoadClass road_class, float effective_zoom);

// Returns Vulkan road polyline simplification step for a class/zoom pair.
uint32_t layer_policy_vk_sample_step(RoadClass road_class, float effective_zoom);

// Returns Vulkan screen-space point merge threshold in pixels.
float layer_policy_vk_min_point_spacing_px(float effective_zoom);

// Returns building fade window bounds.
float layer_policy_building_fade_start(float building_zoom_bias, bool vulkan_backend);
float layer_policy_building_fade_end(float building_zoom_bias, bool vulkan_backend);

// Returns label used in debug output for layer readiness states.
const char *layer_policy_readiness_label(LayerReadinessState state);

// Returns zoom band label used by tile pyramid paths.
const char *layer_policy_band_label(TileZoomBand band);

// Returns target tile pyramid band for a layer at the current zoom.
TileZoomBand layer_policy_band_for_zoom(TileLayerKind kind, float zoom, float region_bias);

// Returns source tile z for a requested band from a base z and range.
uint16_t layer_policy_source_tile_z_for_band(uint16_t min_zoom,
                                             uint16_t max_zoom,
                                             uint16_t base_zoom,
                                             TileZoomBand band);

// Returns per-frame Vulkan line budget tuned by zoom and visible tile count.
uint32_t layer_policy_vk_line_budget(float zoom, uint32_t visible_tiles);

// Returns per-frame polygon tile draw budget tuned by visible tile count.
uint32_t layer_policy_vk_polygon_tile_budget(uint32_t visible_tiles);

// Returns reserved line budget kept for non-map primitives and HUD.
uint32_t layer_policy_vk_reserved_line_budget(uint32_t total_line_budget);

// Returns road line budget from total/reserved budget and active layer state.
uint32_t layer_policy_vk_road_line_budget(uint32_t total_line_budget,
                                          uint32_t reserved_line_budget,
                                          bool building_layer_enabled);

// Returns capped polygon line budget for the current frame.
uint32_t layer_policy_vk_polygon_line_budget_cap(uint32_t lines_drawn,
                                                 uint32_t total_line_budget,
                                                 uint32_t reserved_line_budget,
                                                 uint32_t road_line_budget);

// Returns per-frame polygon fill index budget for Vulkan retained fill meshes.
uint32_t layer_policy_vk_polygon_fill_index_budget(float zoom, uint32_t visible_tiles);

// Returns per-layer share of the polygon fill index budget.
uint32_t layer_policy_vk_polygon_fill_layer_budget(TileLayerKind kind, uint32_t total_index_budget);

#endif
