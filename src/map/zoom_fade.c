#include "map/zoom_fade.h"

#include <math.h>

typedef struct ZoomTierInfo {
    float min_zoom;
    float max_zoom;
    bool has_lower;
    bool has_upper;
} ZoomTierInfo;

#define ZOOM_TIER_FAR_MAX 10.5f
#define ZOOM_TIER_MID_MAX 12.0f
#define ZOOM_TIER_NEAR_MAX 13.0f
#define ZOOM_TIER_CLOSE_MAX 14.0f
#define ZOOM_TIER_PATH_MAX 16.0f

static const ZoomTierInfo kZoomTiers[] = {
    [ZOOM_TIER_FAR] = {0.0f, ZOOM_TIER_FAR_MAX, false, true},
    [ZOOM_TIER_MID] = {ZOOM_TIER_FAR_MAX, ZOOM_TIER_MID_MAX, true, true},
    [ZOOM_TIER_NEAR] = {ZOOM_TIER_MID_MAX, ZOOM_TIER_NEAR_MAX, true, true},
    [ZOOM_TIER_CLOSE] = {ZOOM_TIER_NEAR_MAX, ZOOM_TIER_CLOSE_MAX, true, true},
    [ZOOM_TIER_PATH] = {ZOOM_TIER_CLOSE_MAX, ZOOM_TIER_PATH_MAX, true, false},
};

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
    if (zoom <= ZOOM_TIER_FAR_MAX) {
        return ZOOM_TIER_FAR;
    }
    if (zoom <= ZOOM_TIER_MID_MAX) {
        return ZOOM_TIER_MID;
    }
    if (zoom <= ZOOM_TIER_NEAR_MAX) {
        return ZOOM_TIER_NEAR;
    }
    if (zoom <= ZOOM_TIER_CLOSE_MAX) {
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
    return kZoomTiers[tier].min_zoom;
}

float zoom_tier_fade_in_alpha(float zoom, ZoomTier tier) {
    if (tier < ZOOM_TIER_FAR || tier > ZOOM_TIER_PATH) {
        return 1.0f;
    }

    ZoomTierInfo info = kZoomTiers[tier];
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
