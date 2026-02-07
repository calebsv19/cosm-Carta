#ifndef MAPFORGE_APP_REGION_H
#define MAPFORGE_APP_REGION_H

#include <stdbool.h>
#include <stdint.h>

// Stores region pack metadata for runtime selection.
typedef struct RegionInfo {
    const char *name;
    const char *tiles_dir;
    double center_lat;
    double center_lon;
    double min_lat;
    double max_lat;
    double min_lon;
    double max_lon;
    uint16_t tile_min_zoom;
    uint16_t tile_max_zoom;
    uint32_t tile_extent;
    bool has_center;
    bool has_bounds;
    bool has_tile_range;
} RegionInfo;

// Returns the number of configured regions.
int region_count(void);

// Returns the region info for a given index.
const RegionInfo *region_get(int index);

#endif
