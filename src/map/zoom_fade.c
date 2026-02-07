#include "map/zoom_fade.h"
#include "map/layer_policy.h"

#include <math.h>

typedef struct ZoomTierInfo {
    float min_zoom;
    float max_zoom;
    bool has_lower;
    bool has_upper;
} ZoomTierInfo;

static ZoomTierInfo zoom_tier_info(ZoomTier tier) {
    switch (tier) {
        case ZOOM_TIER_FAR:
            return (ZoomTierInfo){0.0f, layer_policy_zoom_tier_far_max(), false, true};
        case ZOOM_TIER_MID:
            return (ZoomTierInfo){layer_policy_zoom_tier_far_max(), layer_policy_zoom_tier_mid_max(), true, true};
        case ZOOM_TIER_NEAR:
            return (ZoomTierInfo){layer_policy_zoom_tier_mid_max(), layer_policy_zoom_tier_near_max(), true, true};
        case ZOOM_TIER_CLOSE:
            return (ZoomTierInfo){layer_policy_zoom_tier_near_max(), layer_policy_zoom_tier_close_max(), true, true};
        case ZOOM_TIER_PATH:
        default:
            return (ZoomTierInfo){layer_policy_zoom_tier_close_max(), layer_policy_zoom_tier_path_max(), true, false};
    }
}

static float clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

ZoomTier zoom_tier_for(float zoom) {
    if (zoom <= layer_policy_zoom_tier_far_max()) {
        return ZOOM_TIER_FAR;
    }
    if (zoom <= layer_policy_zoom_tier_mid_max()) {
        return ZOOM_TIER_MID;
    }
    if (zoom <= layer_policy_zoom_tier_near_max()) {
        return ZOOM_TIER_NEAR;
    }
    if (zoom <= layer_policy_zoom_tier_close_max()) {
        return ZOOM_TIER_CLOSE;
    }
    return ZOOM_TIER_PATH;
}

const char *zoom_tier_label(ZoomTier tier) {
    switch (tier) {
        case ZOOM_TIER_FAR:
            return "far";
        case ZOOM_TIER_MID:
            return "mid";
        case ZOOM_TIER_NEAR:
            return "near";
        case ZOOM_TIER_CLOSE:
            return "close";
        case ZOOM_TIER_PATH:
        default:
            return "path";
    }
}

float zoom_tier_min_zoom(ZoomTier tier) {
    if (tier < ZOOM_TIER_FAR || tier > ZOOM_TIER_PATH) {
        return 0.0f;
    }
    return zoom_tier_info(tier).min_zoom;
}

float zoom_tier_fade_in_alpha(float zoom, ZoomTier tier) {
    if (tier < ZOOM_TIER_FAR || tier > ZOOM_TIER_PATH) {
        return 1.0f;
    }

    ZoomTierInfo info = zoom_tier_info(tier);
    if (zoom <= info.min_zoom) {
        return 0.0f;
    }

    float span = info.max_zoom - info.min_zoom;
    if (span <= 0.0001f) {
        return 1.0f;
    }
    float fade_span = span * 0.5f;
    if (fade_span <= 0.0001f) {
        return 1.0f;
    }

    float fade_start = info.min_zoom;
    float fade_end = fade_start + fade_span;

    if (zoom <= fade_start) {
        return 0.0f;
    }
    if (zoom >= fade_end) {
        return 1.0f;
    }

    float alpha = (zoom - fade_start) / fade_span;
    return clampf(alpha, 0.0f, 1.0f);
}
