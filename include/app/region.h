#ifndef MAPFORGE_APP_REGION_H
#define MAPFORGE_APP_REGION_H

#include <stdbool.h>

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
    bool has_center;
    bool has_bounds;
} RegionInfo;

// Returns the number of configured regions.
int region_count(void);

// Returns the region info for a given index.
const RegionInfo *region_get(int index);

#endif
