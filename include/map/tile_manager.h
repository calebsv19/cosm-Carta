#ifndef MAPFORGE_MAP_TILE_MANAGER_H
#define MAPFORGE_MAP_TILE_MANAGER_H

#include "map/mft_loader.h"
#include "map/tile_layers.h"

#include <stdbool.h>
#include <stdint.h>

// Stores a cached tile entry for the LRU manager.
typedef struct TileEntry {
    TileCoord coord;
    TileZoomBand band;
    MftTile tile;
    bool occupied;
    uint64_t last_used;
} TileEntry;

// Manages MFT tile loading and LRU eviction.
typedef struct TileManager {
    TileEntry *entries;
    uint32_t capacity;
    uint32_t count;
    uint64_t tick;
    char base_dir[256];
} TileManager;

// Initializes the tile manager with a cache capacity and base directory.
bool tile_manager_init(TileManager *manager, uint32_t capacity, const char *base_dir);

// Releases memory owned by the tile manager.
void tile_manager_shutdown(TileManager *manager);

// Fetches a tile by coordinate, loading it if necessary.
const MftTile *tile_manager_get_tile(TileManager *manager, TileCoord coord, TileZoomBand band);

// Returns a cached tile without loading from disk.
const MftTile *tile_manager_peek_tile(const TileManager *manager, TileCoord coord, TileZoomBand band);

// Inserts a loaded tile into the cache (takes ownership of *tile).
bool tile_manager_put_tile(TileManager *manager, TileCoord coord, TileZoomBand band, MftTile *tile);

// Returns number of cached tiles.
uint32_t tile_manager_count(const TileManager *manager);

// Returns cache capacity.
uint32_t tile_manager_capacity(const TileManager *manager);

// Ensures the cache can hold at least the requested capacity.
bool tile_manager_ensure_capacity(TileManager *manager, uint32_t capacity);

#endif
