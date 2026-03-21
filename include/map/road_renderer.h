#ifndef MAPFORGE_MAP_ROAD_RENDERER_H
#define MAPFORGE_MAP_ROAD_RENDERER_H

#include "camera/camera.h"
#include "map/mft_loader.h"
#include "render/renderer.h"

#include <stdbool.h>
#include <stdint.h>

// Stores per-frame road draw/filter counters by road class.
typedef struct RoadRenderStats {
    uint32_t drawn_by_class[9];
    uint32_t filtered_by_class[9];
} RoadRenderStats;

// Renders road polylines from a decoded MFT tile.
void road_renderer_draw_tile(Renderer *renderer,
                             const Camera *camera,
                             const MftTile *tile,
                             bool single_line,
                             float zoom_bias,
                             float opacity_scale);

// Returns a short label for the current zoom tier.
const char *road_renderer_zoom_tier_label(float zoom);

// Clears per-frame road renderer stats.
void road_renderer_stats_reset(void);

// Copies current per-frame road renderer stats.
void road_renderer_stats_get(RoadRenderStats *out_stats);

#endif
