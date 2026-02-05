#ifndef MAPFORGE_APP_REGION_LOADER_H
#define MAPFORGE_APP_REGION_LOADER_H

#include "app/region.h"

#include <stdbool.h>

// Loads region metadata from meta.json, if present.
bool region_load_meta(const RegionInfo *info, RegionInfo *out_info);

#endif
