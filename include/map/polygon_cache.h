#ifndef MAPFORGE_MAP_POLYGON_CACHE_H
#define MAPFORGE_MAP_POLYGON_CACHE_H

#include "map/mft_loader.h"
#include "map/polygon_triangulator.h"

#define POLYGON_CACHE_MAX_FILL_POINTS 512u

typedef struct PolygonCacheBuildStats {
    uint32_t polygon_quarantined;
    uint32_t ring_quarantine_total;
    uint32_t ring_bounds_quarantined;
    uint32_t ring_min_points_quarantined;
    uint32_t ring_degenerate_quarantined;
    uint32_t ring_winding_normalized;
} PolygonCacheBuildStats;

// Builds exact cache geometry with cancellation checks inside expensive rings.
bool polygon_cache_build_cancelable(MftTile *tile, uint32_t max_points, PolygonCacheBuildStats *stats,
    PolygonTriangulationCancelFn canceled, void *context);

// Prepares exact retained-mesh indices off-thread up to the existing renderer ring limit.
bool polygon_cache_build_with_limit(MftTile *tile, uint32_t max_points, PolygonCacheBuildStats *out_stats);

bool polygon_cache_build_with_stats(MftTile *tile, PolygonCacheBuildStats *out_stats);
bool polygon_cache_build(MftTile *tile);

#endif
