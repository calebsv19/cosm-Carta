#ifndef MAPFORGE_MAP_MFT_LOADER_H
#define MAPFORGE_MAP_MFT_LOADER_H

#include "map/tile_math.h"

#include <stdbool.h>
#include <stdint.h>

// Enumerates road classes stored in MFT tiles.
typedef enum RoadClass {
    ROAD_CLASS_MOTORWAY = 0,
    ROAD_CLASS_TRUNK = 1,
    ROAD_CLASS_PRIMARY = 2,
    ROAD_CLASS_SECONDARY = 3,
    ROAD_CLASS_TERTIARY = 4,
    ROAD_CLASS_RESIDENTIAL = 5,
    ROAD_CLASS_SERVICE = 6,
    ROAD_CLASS_FOOTWAY = 7,
    ROAD_CLASS_PATH = 8
} RoadClass;

// Enumerates polygon classes stored in MFT tiles.
typedef enum PolygonClass {
    POLYGON_CLASS_BUILDING = 0,
    POLYGON_CLASS_WATER = 1,
    POLYGON_CLASS_PARK = 2,
    POLYGON_CLASS_LANDUSE = 3
} PolygonClass;

// Stores one decoded polyline header within a tile.
typedef struct MftPolyline {
    RoadClass road_class;
    uint32_t point_count;
    uint32_t point_offset;
} MftPolyline;

// Stores one decoded polygon header within a tile.
typedef struct MftPolygon {
    PolygonClass polygon_class;
    uint16_t ring_count;
    uint32_t ring_offset;
    uint32_t point_offset;
} MftPolygon;

// Stores decoded polyline and polygon data from an MFT tile.
typedef struct MftTile {
    TileCoord coord;
    uint32_t polyline_count;
    MftPolyline *polylines;
    uint32_t point_total;
    uint16_t *points;
    uint32_t polygon_count;
    MftPolygon *polygons;
    uint32_t polygon_ring_total;
    uint32_t *polygon_rings;
    uint32_t polygon_point_total;
    uint16_t *polygon_points;
    bool polygon_tri_cached;
    uint32_t polygon_tri_index_total;
    uint32_t *polygon_tri_ring_offsets;
    uint32_t *polygon_tri_ring_counts;
    uint32_t *polygon_tri_indices;
} MftTile;

// Loads an MFT tile from disk.
bool mft_load_tile(const char *path, MftTile *out_tile);

// Releases memory allocated for a loaded tile.
void mft_free_tile(MftTile *tile);

#endif
