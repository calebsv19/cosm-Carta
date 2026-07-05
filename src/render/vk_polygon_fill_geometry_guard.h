#ifndef MAPFORGE_RENDER_VK_POLYGON_FILL_GEOMETRY_GUARD_H
#define MAPFORGE_RENDER_VK_POLYGON_FILL_GEOMETRY_GUARD_H

#include "map/tile_layers.h"

#include <stdbool.h>
#include <stdint.h>

bool vk_polygon_fill_kind_allowed_for_retained_mesh(TileLayerKind kind);

bool vk_polygon_fill_ring_allowed_for_retained_mesh(TileLayerKind kind,
                                                    const uint16_t *ring_points,
                                                    uint32_t ring_count);

#endif
