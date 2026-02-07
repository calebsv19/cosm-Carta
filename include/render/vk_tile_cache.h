#ifndef MAPFORGE_RENDER_VK_TILE_CACHE_H
#define MAPFORGE_RENDER_VK_TILE_CACHE_H

#include "map/mft_loader.h"
#include "map/tile_layers.h"

#include <stdbool.h>
#include <stdint.h>

#if defined(MAPFORGE_HAVE_VK)
#include <vk_renderer.h>
#endif

// Stores retained per-tile render metadata used by Vulkan path migration.
typedef struct VkTileCacheEntry {
    TileLayerKind kind;
    TileCoord coord;
    bool occupied;
    uint64_t last_used;
    uint32_t polyline_count;
    uint32_t segment_count;
    uint32_t polygon_count;
    uint32_t polygon_ring_count;
    uint32_t class_counts[ROAD_CLASS_PATH + 1];
#if defined(MAPFORGE_HAVE_VK)
    bool water_lod_mesh_ready[3];
    VkRendererLineMesh water_lod_mesh[3];
    bool mesh_ready;
    VkRendererLineMesh mesh;
    bool fill_mesh_ready;
    VkRendererTriMesh fill_mesh;
    bool road_mesh_ready[ROAD_CLASS_PATH + 1];
    VkRendererLineMesh road_mesh[ROAD_CLASS_PATH + 1];
#endif
} VkTileCacheEntry;

// Holds retained tile render metadata with simple LRU eviction.
typedef struct VkTileCache {
    VkTileCacheEntry *entries;
    uint32_t capacity;
    uint32_t count;
    uint32_t resident_by_kind[TILE_LAYER_COUNT];
    uint32_t min_resident_by_kind[TILE_LAYER_COUNT];
    uint64_t tick;
    uint32_t builds;
    uint32_t evictions;
    uint64_t mesh_vertices;
    uint64_t mesh_bytes;
    uint32_t mesh_build_failures;
    uint32_t fill_mesh_build_failures;
} VkTileCache;

// Summarizes cache health for debug/perf overlays.
typedef struct VkTileCacheStats {
    uint32_t capacity;
    uint32_t count;
    uint32_t builds;
    uint32_t evictions;
    uint32_t resident_artery;
    uint32_t resident_local;
    uint32_t resident_water;
    uint32_t resident_park;
    uint32_t resident_landuse;
    uint32_t resident_building;
    uint32_t resident_fill_water;
    uint32_t resident_fill_park;
    uint32_t resident_fill_landuse;
    uint32_t resident_fill_building;
    uint64_t mesh_vertices;
    uint64_t mesh_bytes;
    uint32_t mesh_build_failures;
    uint32_t fill_mesh_build_failures;
} VkTileCacheStats;

// Initializes cache storage.
bool vk_tile_cache_init(VkTileCache *cache, uint32_t capacity);

// Releases all cache memory.
void vk_tile_cache_shutdown(VkTileCache *cache);

// Clears entries while retaining allocated storage.
void vk_tile_cache_clear(VkTileCache *cache);

// Clears entries and destroys retained GPU meshes when Vulkan is enabled.
void vk_tile_cache_clear_with_renderer(VkTileCache *cache, void *vk_renderer);

// Builds/updates retained tile metadata from a decoded tile.
bool vk_tile_cache_on_tile_loaded(VkTileCache *cache,
                                  void *vk_renderer,
                                  TileLayerKind kind,
                                  TileCoord coord,
                                  const MftTile *tile);

// Returns cached retained metadata for a tile, if available.
const VkTileCacheEntry *vk_tile_cache_peek(VkTileCache *cache, TileLayerKind kind, TileCoord coord);

// Returns cache counters for instrumentation.
void vk_tile_cache_get_stats(const VkTileCache *cache, VkTileCacheStats *out_stats);

#endif
