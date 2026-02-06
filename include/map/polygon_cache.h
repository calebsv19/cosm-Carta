#ifndef MAPFORGE_MAP_POLYGON_CACHE_H
#define MAPFORGE_MAP_POLYGON_CACHE_H

#include "map/mft_loader.h"

#define POLYGON_CACHE_MAX_FILL_POINTS 512u

bool polygon_cache_build(MftTile *tile);

#endif
