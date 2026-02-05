#ifndef MAPFORGE_MAP_TILE_MATH_H
#define MAPFORGE_MAP_TILE_MATH_H

#include "map/mercator.h"

#include <stdint.h>

// Stores an integer slippy tile coordinate.
typedef struct TileCoord {
    uint16_t z;
    uint32_t x;
    uint32_t y;
} TileCoord;

// Returns number of tiles along one axis at the given zoom.
uint32_t tile_count(uint16_t z);

// Returns tile size in meters at the given zoom.
double tile_size_meters(uint16_t z);

// Returns the tile coordinate containing a Mercator position.
TileCoord tile_from_meters(uint16_t z, MercatorMeters value);

// Returns the Mercator origin (top-left) of the tile.
MercatorMeters tile_origin_meters(TileCoord tile);

#endif
