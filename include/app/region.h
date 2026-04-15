#ifndef MAPFORGE_APP_REGION_H
#define MAPFORGE_APP_REGION_H

#include "map/tile_source.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAPFORGE_REGION_PATH_CAPACITY 512u

enum {
    REGION_ROLLUP_BAND_DEFAULT = 0,
    REGION_ROLLUP_BAND_COARSE = 1,
    REGION_ROLLUP_BAND_MID = 2,
    REGION_ROLLUP_BAND_FINE = 3,
    REGION_ROLLUP_BAND_COUNT = 4
};

enum {
    REGION_ROLLUP_LAYER_ARTERY = 0,
    REGION_ROLLUP_LAYER_LOCAL = 1,
    REGION_ROLLUP_LAYER_WATER = 2,
    REGION_ROLLUP_LAYER_PARK = 3,
    REGION_ROLLUP_LAYER_LANDUSE = 4,
    REGION_ROLLUP_LAYER_BUILDING = 5,
    REGION_ROLLUP_LAYER_CONTOUR = 6,
    REGION_ROLLUP_LAYER_COUNT = 7
};

// Stores region pack metadata for runtime selection.
typedef struct RegionInfo {
    const char *name;
    char region_dir[MAPFORGE_REGION_PATH_CAPACITY];
    char tiles_dir[MAPFORGE_REGION_PATH_CAPACITY];
    char tile_archive_path[MAPFORGE_REGION_PATH_CAPACITY];
    TileSourceConfig tile_source;
    double center_lat;
    double center_lon;
    double min_lat;
    double max_lat;
    double min_lon;
    double max_lon;
    uint16_t tile_min_zoom;
    uint16_t tile_max_zoom;
    uint32_t tile_extent;
    bool has_tile_pyramid_roads;
    bool has_tile_pyramid_buildings;
    bool has_tile_archive;
    bool has_archive_rollups;
    bool has_center;
    bool has_bounds;
    bool has_tile_range;
    uint64_t archive_rollup_rows[REGION_ROLLUP_BAND_COUNT][REGION_ROLLUP_LAYER_COUNT];
    uint64_t archive_rollup_bytes[REGION_ROLLUP_BAND_COUNT][REGION_ROLLUP_LAYER_COUNT];
    uint64_t archive_rollup_total_rows;
    uint64_t archive_rollup_total_bytes;
} RegionInfo;

// Returns the number of configured regions.
int region_count(void);

// Returns the region info for a given index.
const RegionInfo *region_get(int index);

// Resolves the configured region data root path.
const char *region_data_root(void);

// Fills region_dir and tiles_dir based on the configured region root and name.
bool region_resolve_paths(RegionInfo *info);

// Builds a path to the region metadata file.
bool region_meta_path(const RegionInfo *info, char *out_path, size_t out_size);

// Builds a path to the region graph file.
bool region_graph_path(const RegionInfo *info, char *out_path, size_t out_size);
// Returns true when the region has a readable graph/graph.bin payload.
bool region_has_graph(const RegionInfo *info);

#endif
