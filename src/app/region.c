#include "app/region.h"

#include <string.h>

static const RegionInfo kRegions[] = {
    {"seattle_small", "data/regions/seattle_small/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false},
    {"seattle_medium", "data/regions/seattle_medium/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false},
    {"seattle_large", "data/regions/seattle_large/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false},
    {"amsterdam", "data/regions/amsterdam/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false},
    {"los_angeles", "data/regions/los_angeles/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false}
};

int region_count(void) {
    return (int)(sizeof(kRegions) / sizeof(kRegions[0]));
}

const RegionInfo *region_get(int index) {
    if (index < 0 || index >= region_count()) {
        return NULL;
    }

    return &kRegions[index];
}
