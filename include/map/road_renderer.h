#ifndef MAPFORGE_MAP_ROAD_RENDERER_H
#define MAPFORGE_MAP_ROAD_RENDERER_H

#include "camera/camera.h"
#include "map/mft_loader.h"
#include "render/renderer.h"

#include <stdbool.h>

// Renders road polylines from a decoded MFT tile.
void road_renderer_draw_tile(Renderer *renderer, const Camera *camera, const MftTile *tile, bool single_line);

// Returns a short label for the current zoom tier.
const char *road_renderer_zoom_tier_label(float zoom);

#endif
