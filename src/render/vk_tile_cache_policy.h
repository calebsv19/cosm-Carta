#ifndef MAPFORGE_VK_TILE_CACHE_POLICY_H
#define MAPFORGE_VK_TILE_CACHE_POLICY_H

#include "render/vk_tile_cache.h"

VkTileCacheEntry *vk_tile_cache_find_entry(VkTileCache *cache,
                                           TileLayerKind kind,
                                           TileCoord coord,
                                           TileZoomBand band);
VkTileCacheEntry *vk_tile_cache_pick_slot(VkTileCache *cache, TileLayerKind incoming_kind);

#endif
