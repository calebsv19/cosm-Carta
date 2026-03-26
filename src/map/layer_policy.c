#include "map/layer_policy.h"

// Centralized per-layer zoom starts.
#define LAYER_ZOOM_LOCAL_START 10.0f
#define LAYER_ZOOM_CONTOUR_START 12.2f
#define LAYER_ZOOM_WATER_START 12.8f
#define LAYER_ZOOM_PARK_START 13.0f
#define LAYER_ZOOM_LANDUSE_START 13.4f
#define LAYER_ZOOM_BUILDING_START 12.8f

// Centralized tier boundaries used by zoom fade logic.
#define ZOOM_TIER_FAR_MAX 9.5f
#define ZOOM_TIER_MID_MAX 11.0f
#define ZOOM_TIER_NEAR_MAX 12.0f
#define ZOOM_TIER_CLOSE_MAX 13.5f
#define ZOOM_TIER_PATH_MAX 15.0f

// Centralized zoom thresholds for tile pyramid band selection.
#define BAND_COARSE_MAX_ZOOM 12.6f
#define BAND_MID_MAX_ZOOM 14.6f

static const LayerPolicy kLayerPolicies[] = {
    {TILE_LAYER_ROAD_ARTERY, "artery", 0.0f, 0u, true, false, false},
    {TILE_LAYER_ROAD_LOCAL, "local", LAYER_ZOOM_LOCAL_START, 1u, true, false, true},
    // Kept for future terrain/elevation support; hidden/disabled in current runtime.
    {TILE_LAYER_CONTOUR, "contour", LAYER_ZOOM_CONTOUR_START, 2u, false, false, false},
    {TILE_LAYER_POLY_WATER, "water", LAYER_ZOOM_WATER_START, 3u, true, true, false},
    {TILE_LAYER_POLY_PARK, "park", LAYER_ZOOM_PARK_START, 4u, true, true, false},
    {TILE_LAYER_POLY_LANDUSE, "landuse", LAYER_ZOOM_LANDUSE_START, 5u, true, true, false},
    {TILE_LAYER_POLY_BUILDING, "building", LAYER_ZOOM_BUILDING_START, 6u, true, true, false}
};

size_t layer_policy_count(void) {
    return sizeof(kLayerPolicies) / sizeof(kLayerPolicies[0]);
}

const LayerPolicy *layer_policy_at(size_t index) {
    if (index >= layer_policy_count()) {
        return NULL;
    }
    return &kLayerPolicies[index];
}

const LayerPolicy *layer_policy_for(TileLayerKind kind) {
    for (size_t i = 0; i < layer_policy_count(); ++i) {
        if (kLayerPolicies[i].kind == kind) {
            return &kLayerPolicies[i];
        }
    }
    return NULL;
}

const char *layer_policy_label(TileLayerKind kind) {
    const LayerPolicy *policy = layer_policy_for(kind);
    return policy ? policy->label : "layer";
}

float layer_policy_zoom_start(TileLayerKind kind, float building_zoom_bias) {
    const LayerPolicy *policy = layer_policy_for(kind);
    if (!policy) {
        return 0.0f;
    }
    if (kind == TILE_LAYER_POLY_BUILDING) {
        return policy->zoom_start + building_zoom_bias;
    }
    return policy->zoom_start;
}

bool layer_policy_layer_active(TileLayerKind kind, float zoom, float building_zoom_bias) {
    const LayerPolicy *policy = layer_policy_for(kind);
    if (!policy || !policy->enabled) {
        return false;
    }
    return zoom >= layer_policy_zoom_start(kind, building_zoom_bias);
}

bool layer_policy_requires_full_ready(TileLayerKind kind) {
    const LayerPolicy *policy = layer_policy_for(kind);
    return policy ? policy->requires_full_layer_ready : false;
}

float layer_policy_zoom_tier_far_max(void) {
    return ZOOM_TIER_FAR_MAX;
}

float layer_policy_zoom_tier_mid_max(void) {
    return ZOOM_TIER_MID_MAX;
}

float layer_policy_zoom_tier_near_max(void) {
    return ZOOM_TIER_NEAR_MAX;
}

float layer_policy_zoom_tier_close_max(void) {
    return ZOOM_TIER_CLOSE_MAX;
}

float layer_policy_zoom_tier_path_max(void) {
    return ZOOM_TIER_PATH_MAX;
}

bool layer_policy_vk_road_class_allowed(RoadClass road_class, float effective_zoom) {
    if (effective_zoom < 12.2f) {
        return road_class == ROAD_CLASS_MOTORWAY ||
               road_class == ROAD_CLASS_TRUNK ||
               road_class == ROAD_CLASS_PRIMARY ||
               road_class == ROAD_CLASS_SECONDARY ||
               road_class == ROAD_CLASS_TERTIARY;
    }
    if (effective_zoom < 13.6f) {
        return road_class != ROAD_CLASS_FOOTWAY &&
               road_class != ROAD_CLASS_PATH;
    }
    if (effective_zoom < 14.4f) {
        return road_class != ROAD_CLASS_PATH;
    }
    return true;
}

uint32_t layer_policy_vk_sample_step(RoadClass road_class, float effective_zoom) {
    uint32_t step = 1u;
    if (effective_zoom < 12.0f) {
        step = 3u;
    } else if (effective_zoom < 13.3f) {
        step = 2u;
    }
    if ((road_class == ROAD_CLASS_FOOTWAY || road_class == ROAD_CLASS_PATH) && effective_zoom < 13.8f) {
        step += 1u;
    }
    return step;
}

float layer_policy_vk_min_point_spacing_px(float effective_zoom) {
    if (effective_zoom < 12.0f) {
        return 1.4f;
    }
    if (effective_zoom < 13.2f) {
        return 0.8f;
    }
    return 0.0f;
}

float layer_policy_building_fade_start(float building_zoom_bias, bool vulkan_backend) {
    float start = layer_policy_zoom_start(TILE_LAYER_POLY_BUILDING, building_zoom_bias) + 0.2f;
    if (vulkan_backend) {
        start += 0.15f;
    }
    return start;
}

float layer_policy_building_fade_end(float building_zoom_bias, bool vulkan_backend) {
    return layer_policy_building_fade_start(building_zoom_bias, vulkan_backend) + 0.7f;
}

const char *layer_policy_readiness_label(LayerReadinessState state) {
    switch (state) {
        case LAYER_READINESS_HIDDEN:
            return "hidden";
        case LAYER_READINESS_LOADING:
            return "loading";
        case LAYER_READINESS_READY:
            return "ready";
        default:
            return "unknown";
    }
}

const char *layer_policy_band_label(TileZoomBand band) {
    switch (band) {
        case TILE_BAND_COARSE:
            return "coarse";
        case TILE_BAND_MID:
            return "mid";
        case TILE_BAND_FINE:
            return "fine";
        case TILE_BAND_DEFAULT:
        default:
            return "default";
    }
}

TileZoomBand layer_policy_band_for_zoom(TileLayerKind kind, float zoom, float region_bias) {
    (void)region_bias;
    if (kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL) {
        if (zoom < BAND_COARSE_MAX_ZOOM) {
            return TILE_BAND_COARSE;
        }
        if (zoom < BAND_MID_MAX_ZOOM) {
            return TILE_BAND_MID;
        }
        return TILE_BAND_FINE;
    }

    if (kind == TILE_LAYER_POLY_BUILDING) {
        if (zoom < 12.9f) {
            return TILE_BAND_COARSE;
        }
        if (zoom < 14.0f) {
            return TILE_BAND_MID;
        }
        return TILE_BAND_FINE;
    }

    // Phase 7D 4B: non-building polygon overlays participate in pyramid bands.
    if (kind == TILE_LAYER_POLY_WATER || kind == TILE_LAYER_POLY_PARK ||
        kind == TILE_LAYER_POLY_LANDUSE) {
        if (zoom < 13.1f) {
            return TILE_BAND_COARSE;
        }
        if (zoom < 15.0f) {
            return TILE_BAND_MID;
        }
        return TILE_BAND_FINE;
    }

    return TILE_BAND_DEFAULT;
}

uint16_t layer_policy_source_tile_z_for_band(uint16_t min_zoom,
                                             uint16_t max_zoom,
                                             uint16_t base_zoom,
                                             TileZoomBand band) {
    int source = (int)base_zoom;
    if (band == TILE_BAND_COARSE) {
        source -= 2;
    } else if (band == TILE_BAND_MID) {
        source -= 1;
    } else if (band == TILE_BAND_FINE) {
        source += 1;
    }
    if (source < (int)min_zoom) {
        source = (int)min_zoom;
    }
    if (source > (int)max_zoom) {
        source = (int)max_zoom;
    }
    return (uint16_t)source;
}

uint32_t layer_policy_vk_line_budget(float zoom, uint32_t visible_tiles) {
    uint32_t budget = 76000u;
    if (zoom >= 15.5f) {
        budget = 70000u;
    } else if (zoom >= 15.0f) {
        budget = 74000u;
    } else if (zoom >= 14.0f) {
        budget = 78000u;
    } else if (zoom >= 13.0f) {
        budget = 82000u;
    } else if (zoom < 12.0f) {
        budget = 92000u;
    }

    if (visible_tiles <= 2u) {
        budget = 100000u;
    } else if (visible_tiles <= 4u) {
        budget = 92000u;
    } else if (visible_tiles <= 8u) {
        budget = 84000u;
    }

    if (visible_tiles > 80u) {
        budget = (uint32_t)((float)budget * 0.55f);
    } else if (visible_tiles > 48u) {
        budget = (uint32_t)((float)budget * 0.70f);
    } else if (visible_tiles > 24u) {
        budget = (uint32_t)((float)budget * 0.82f);
    }

    if (budget < 32000u) {
        budget = 32000u;
    }
    return budget;
}

uint32_t layer_policy_vk_polygon_tile_budget(uint32_t visible_tiles) {
    if (visible_tiles > 80u) {
        return 4u;
    }
    if (visible_tiles > 40u) {
        return 8u;
    }
    return 16u;
}

uint32_t layer_policy_vk_reserved_line_budget(uint32_t total_line_budget) {
    if (total_line_budget == 0u) {
        return 0u;
    }

    uint32_t reserve = total_line_budget / 20u;
    if (reserve < 1500u) {
        reserve = 1500u;
    }
    if (reserve > total_line_budget / 5u) {
        reserve = total_line_budget / 5u;
    }
    return reserve;
}

uint32_t layer_policy_vk_road_line_budget(uint32_t total_line_budget,
                                          uint32_t reserved_line_budget,
                                          bool building_layer_enabled) {
    if (total_line_budget == 0u) {
        return 0u;
    }

    uint32_t drawable_budget = total_line_budget > reserved_line_budget
        ? (total_line_budget - reserved_line_budget)
        : total_line_budget;
    float road_share = building_layer_enabled ? 0.82f : 0.95f;
    uint32_t road_budget = (uint32_t)((float)drawable_budget * road_share);
    if (road_budget < drawable_budget / 2u) {
        road_budget = drawable_budget / 2u;
    }
    return road_budget;
}

uint32_t layer_policy_vk_polygon_line_budget_cap(uint32_t lines_drawn,
                                                 uint32_t total_line_budget,
                                                 uint32_t reserved_line_budget,
                                                 uint32_t road_line_budget) {
    if (total_line_budget == 0u) {
        return 0u;
    }

    uint32_t drawable_budget = total_line_budget > reserved_line_budget
        ? (total_line_budget - reserved_line_budget)
        : total_line_budget;
    uint32_t polygon_budget = drawable_budget > road_line_budget
        ? (drawable_budget - road_line_budget)
        : 0u;
    uint32_t polygon_cap = lines_drawn + polygon_budget;
    uint32_t max_cap = total_line_budget > reserved_line_budget
        ? (total_line_budget - reserved_line_budget)
        : total_line_budget;
    if (polygon_cap > max_cap) {
        polygon_cap = max_cap;
    }
    if (polygon_cap < lines_drawn) {
        polygon_cap = lines_drawn;
    }
    return polygon_cap;
}

uint32_t layer_policy_vk_polygon_fill_index_budget(float zoom, uint32_t visible_tiles) {
    uint32_t budget = 180000u;
    if (zoom >= 16.0f) {
        budget = 260000u;
    } else if (zoom >= 15.0f) {
        budget = 230000u;
    } else if (zoom >= 14.0f) {
        budget = 210000u;
    } else if (zoom < 13.0f) {
        budget = 150000u;
    }

    if (visible_tiles <= 2u) {
        budget = (uint32_t)((float)budget * 1.35f);
    } else if (visible_tiles <= 4u) {
        budget = (uint32_t)((float)budget * 1.20f);
    } else if (visible_tiles > 48u) {
        budget = (uint32_t)((float)budget * 0.55f);
    } else if (visible_tiles > 24u) {
        budget = (uint32_t)((float)budget * 0.70f);
    }

    if (budget < 40000u) {
        budget = 40000u;
    }
    return budget;
}

uint32_t layer_policy_vk_polygon_fill_layer_budget(TileLayerKind kind, uint32_t total_index_budget) {
    if (total_index_budget == 0u) {
        return 0u;
    }

    float share = 0.0f;
    switch (kind) {
        case TILE_LAYER_POLY_WATER:
            share = 0.38f;
            break;
        case TILE_LAYER_POLY_PARK:
            share = 0.24f;
            break;
        case TILE_LAYER_POLY_LANDUSE:
            share = 0.14f;
            break;
        case TILE_LAYER_POLY_BUILDING:
            share = 0.24f;
            break;
        default:
            share = 0.0f;
            break;
    }
    uint32_t layer_budget = (uint32_t)((float)total_index_budget * share);
    if (layer_budget == 0u && share > 0.0f) {
        layer_budget = 1u;
    }
    return layer_budget;
}
