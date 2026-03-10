#include "app/region.h"

#include <string.h>

static const RegionInfo kRegions[] = {
    {"sample", "data/regions/sample/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false},
    {"seattle_small", "data/regions/seattle_small/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false},
    {"seattle_medium", "data/regions/seattle_medium/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false},
    {"seattle_large", "data/regions/seattle_large/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false},
    {"new_york", "data/regions/new_york/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false},
    {"rome", "data/regions/rome/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false},
    {"poulsbo", "data/regions/poulsbo/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false},
    {"puyallup", "data/regions/puyallup/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false},
    {"puget_sound", "data/regions/puget_sound/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false},
    {"planet_122_416_47_494_122_214_47_83", "data/regions/planet_122_416_47_494_122_214_47_83/tiles", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 10, 18, 4096, false, false, false, false}
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
