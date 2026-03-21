#ifndef MAPFORGE_MAP_POLYGON_RENDERER_H
#define MAPFORGE_MAP_POLYGON_RENDERER_H

#include "camera/camera.h"
#include "map/mft_loader.h"
#include "render/renderer.h"

// Renders polygon layers from a decoded MFT tile.
void polygon_renderer_draw_tile(Renderer *renderer,
                                const Camera *camera,
                                MftTile *tile,
                                bool show_landuse,
                                float building_zoom_bias,
                                bool building_fill_enabled,
                                bool polygon_outline_only,
                                float opacity_scale);

#endif
