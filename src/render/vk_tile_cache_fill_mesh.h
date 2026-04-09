#ifndef MAPFORGE_RENDER_VK_TILE_CACHE_FILL_MESH_H
#define MAPFORGE_RENDER_VK_TILE_CACHE_FILL_MESH_H

#include "render/vk_tile_cache.h"

bool vk_tile_cache_build_polygon_fill_mesh(VkTileCache *cache,
                                           void *vk_renderer,
                                           VkTileCacheEntry *entry,
                                           const MftTile *tile);

#endif
