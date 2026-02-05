#ifndef MAPFORGE_MAP_ZOOM_FADE_H
#define MAPFORGE_MAP_ZOOM_FADE_H

#include <stdbool.h>

// Groups zoom ranges for visibility and fade calculations.
typedef enum ZoomTier {
    ZOOM_TIER_FAR = 0,
    ZOOM_TIER_MID = 1,
    ZOOM_TIER_NEAR = 2,
    ZOOM_TIER_CLOSE = 3,
    ZOOM_TIER_PATH = 4
} ZoomTier;

// Returns the active zoom tier for a given zoom level.
ZoomTier zoom_tier_for(float zoom);

// Returns a short label for a zoom tier.
const char *zoom_tier_label(ZoomTier tier);

// Returns the minimum zoom for a tier.
float zoom_tier_min_zoom(ZoomTier tier);

// Returns the alpha (0..1) for fading in at the start of a tier.
float zoom_tier_fade_in_alpha(float zoom, ZoomTier tier);

#endif
