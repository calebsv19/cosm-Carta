#include "map/layer_policy.h"

// Centralized per-layer zoom starts.
#define LAYER_ZOOM_LOCAL_START 10.0f
#define LAYER_ZOOM_CONTOUR_START 12.2f
#define LAYER_ZOOM_WATER_START 12.8f
#define LAYER_ZOOM_PARK_START 13.0f
#define LAYER_ZOOM_LANDUSE_START 13.4f
#define LAYER_ZOOM_BUILDING_START 13.5f

// Centralized tier boundaries used by zoom fade logic.
#define ZOOM_TIER_FAR_MAX 9.5f
#define ZOOM_TIER_MID_MAX 11.0f
#define ZOOM_TIER_NEAR_MAX 12.0f
#define ZOOM_TIER_CLOSE_MAX 13.5f
#define ZOOM_TIER_PATH_MAX 15.0f

static const LayerPolicy kLayerPolicies[] = {
    {TILE_LAYER_ROAD_ARTERY, "artery", 0.0f, 0u, true, false, false},
    {TILE_LAYER_ROAD_LOCAL, "local", LAYER_ZOOM_LOCAL_START, 1u, true, false, true},
    {TILE_LAYER_CONTOUR, "contour", LAYER_ZOOM_CONTOUR_START, 2u, false, false, true},
    {TILE_LAYER_POLY_WATER, "water", LAYER_ZOOM_WATER_START, 3u, true, true, true},
    {TILE_LAYER_POLY_PARK, "park", LAYER_ZOOM_PARK_START, 4u, true, true, true},
    {TILE_LAYER_POLY_LANDUSE, "landuse", LAYER_ZOOM_LANDUSE_START, 5u, true, true, true},
    {TILE_LAYER_POLY_BUILDING, "building", LAYER_ZOOM_BUILDING_START, 6u, true, true, true}
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
    if (effective_zoom < 11.5f) {
        return road_class <= ROAD_CLASS_SECONDARY;
    }
    if (effective_zoom < 12.0f) {
        return road_class <= ROAD_CLASS_TERTIARY;
    }
    if (effective_zoom < 12.6f) {
        return road_class <= ROAD_CLASS_RESIDENTIAL;
    }
    if (effective_zoom < 13.2f) {
        return road_class <= ROAD_CLASS_SERVICE;
    }
    if (effective_zoom < 13.8f) {
        return road_class <= ROAD_CLASS_FOOTWAY;
    }
    return true;
}

uint32_t layer_policy_vk_sample_step(RoadClass road_class, float effective_zoom) {
    uint32_t step = 1u;
    if (effective_zoom < 12.3f) {
        step = 6u;
    } else if (effective_zoom < 12.9f) {
        step = 4u;
    } else if (effective_zoom < 13.5f) {
        step = 3u;
    } else if (effective_zoom < 14.5f) {
        step = 2u;
    }

    if (road_class == ROAD_CLASS_RESIDENTIAL && effective_zoom < 14.5f) {
        step += 1u;
    } else if ((road_class == ROAD_CLASS_SERVICE ||
                road_class == ROAD_CLASS_FOOTWAY ||
                road_class == ROAD_CLASS_PATH) &&
               effective_zoom < 15.0f) {
        step += 2u;
    }
    return step;
}

float layer_policy_vk_min_point_spacing_px(float effective_zoom) {
    if (effective_zoom < 13.0f) {
        return 1.2f;
    }
    if (effective_zoom < 14.0f) {
        return 0.8f;
    }
    if (effective_zoom < 15.0f) {
        return 0.6f;
    }
    return 0.4f;
}

float layer_policy_building_fade_start(float building_zoom_bias, bool vulkan_backend) {
    float start = layer_policy_zoom_tier_close_max() + 0.9f + building_zoom_bias;
    if (vulkan_backend) {
        start += 0.25f;
    }
    return start;
}

float layer_policy_building_fade_end(float building_zoom_bias, bool vulkan_backend) {
    return layer_policy_building_fade_start(building_zoom_bias, vulkan_backend) + 0.6f;
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
