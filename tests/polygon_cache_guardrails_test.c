#include "map/polygon_cache.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void free_tile(MftTile *tile) {
    if (!tile) {
        return;
    }
    free(tile->polylines);
    free(tile->points);
    free(tile->polygons);
    free(tile->polygon_rings);
    free(tile->polygon_points);
    free(tile->polygon_tri_ring_offsets);
    free(tile->polygon_tri_ring_counts);
    free(tile->polygon_tri_indices);
    for (uint32_t i = 0u; i < 3u; ++i) {
        free(tile->water_variant_rings[i]);
        free(tile->water_variant_points[i]);
    }
    memset(tile, 0, sizeof(*tile));
}

static void test_ring_bounds_quarantine(void) {
    MftTile tile;
    memset(&tile, 0, sizeof(tile));
    tile.polygon_count = 1u;
    tile.polygons = (MftPolygon *)calloc(1u, sizeof(MftPolygon));
    tile.polygon_ring_total = 2u;
    tile.polygon_rings = (uint32_t *)calloc(2u, sizeof(uint32_t));
    tile.polygon_point_total = 5u;
    tile.polygon_points = (uint16_t *)calloc(tile.polygon_point_total * 2u, sizeof(uint16_t));
    assert(tile.polygons && tile.polygon_rings && tile.polygon_points);

    tile.polygons[0].ring_count = 2u;
    tile.polygons[0].ring_offset = 0u;
    tile.polygons[0].point_offset = 0u;
    tile.polygon_rings[0] = 4u;
    tile.polygon_rings[1] = 4u;

    PolygonCacheBuildStats stats;
    memset(&stats, 0, sizeof(stats));
    assert(polygon_cache_build_with_stats(&tile, &stats));
    assert(stats.ring_bounds_quarantined >= 1u);
    assert(stats.ring_quarantine_total >= 1u);
    assert(tile.polygon_tri_cached);

    free_tile(&tile);
}

static void test_ring_min_points_quarantine(void) {
    MftTile tile;
    memset(&tile, 0, sizeof(tile));
    tile.polygon_count = 1u;
    tile.polygons = (MftPolygon *)calloc(1u, sizeof(MftPolygon));
    tile.polygon_ring_total = 1u;
    tile.polygon_rings = (uint32_t *)calloc(1u, sizeof(uint32_t));
    tile.polygon_point_total = 2u;
    tile.polygon_points = (uint16_t *)calloc(tile.polygon_point_total * 2u, sizeof(uint16_t));
    assert(tile.polygons && tile.polygon_rings && tile.polygon_points);

    tile.polygons[0].ring_count = 1u;
    tile.polygons[0].ring_offset = 0u;
    tile.polygons[0].point_offset = 0u;
    tile.polygon_rings[0] = 2u;

    PolygonCacheBuildStats stats;
    memset(&stats, 0, sizeof(stats));
    assert(polygon_cache_build_with_stats(&tile, &stats));
    assert(stats.ring_min_points_quarantined == 1u);
    assert(stats.ring_quarantine_total == 1u);
    assert(tile.polygon_tri_index_total == 0u);

    free_tile(&tile);
}

static void test_winding_normalization(void) {
    MftTile tile;
    memset(&tile, 0, sizeof(tile));
    tile.polygon_count = 1u;
    tile.polygons = (MftPolygon *)calloc(1u, sizeof(MftPolygon));
    tile.polygon_ring_total = 1u;
    tile.polygon_rings = (uint32_t *)calloc(1u, sizeof(uint32_t));
    tile.polygon_point_total = 4u;
    tile.polygon_points = (uint16_t *)calloc(tile.polygon_point_total * 2u, sizeof(uint16_t));
    assert(tile.polygons && tile.polygon_rings && tile.polygon_points);

    tile.polygons[0].ring_count = 1u;
    tile.polygons[0].ring_offset = 0u;
    tile.polygons[0].point_offset = 0u;
    tile.polygon_rings[0] = 4u;

    // Clockwise outer ring (should be normalized to CCW).
    uint16_t points[8] = {
        0u, 0u,
        0u, 10u,
        10u, 10u,
        10u, 0u
    };
    memcpy(tile.polygon_points, points, sizeof(points));

    PolygonCacheBuildStats stats;
    memset(&stats, 0, sizeof(stats));
    assert(polygon_cache_build_with_stats(&tile, &stats));
    assert(stats.ring_winding_normalized == 1u);
    assert(tile.polygon_tri_index_total == 6u);
    assert(tile.polygon_tri_cache_max_points == POLYGON_CACHE_MAX_FILL_POINTS);
    assert(polygon_cache_build_with_limit(&tile, 8192, &stats));
    assert(tile.polygon_tri_cache_max_points == 8192 && tile.polygon_tri_index_total == 6);
    uint32_t *prepared = tile.polygon_tri_indices;
    assert(polygon_cache_build_with_stats(&tile, &stats));
    assert(tile.polygon_tri_indices == prepared); // Smaller requests retain the prepared mesh.

    free_tile(&tile);
}

static void test_polygon_quarantine_on_bad_header(void) {
    MftTile tile;
    memset(&tile, 0, sizeof(tile));
    tile.polygon_count = 1u;
    tile.polygons = (MftPolygon *)calloc(1u, sizeof(MftPolygon));
    tile.polygon_ring_total = 1u;
    tile.polygon_rings = (uint32_t *)calloc(1u, sizeof(uint32_t));
    tile.polygon_point_total = 4u;
    tile.polygon_points = (uint16_t *)calloc(tile.polygon_point_total * 2u, sizeof(uint16_t));
    assert(tile.polygons && tile.polygon_rings && tile.polygon_points);

    tile.polygons[0].ring_count = 1u;
    tile.polygons[0].ring_offset = 8u;
    tile.polygons[0].point_offset = 0u;
    tile.polygon_rings[0] = 4u;

    PolygonCacheBuildStats stats;
    memset(&stats, 0, sizeof(stats));
    assert(polygon_cache_build_with_stats(&tile, &stats));
    assert(stats.polygon_quarantined == 1u);
    assert(tile.polygon_tri_index_total == 0u);

    free_tile(&tile);
}

// Cancels deterministically inside the inner triangulation loop.
static bool cancel_after_checks(void *context) {
    unsigned *checks=context;
    return ++*checks > 8;
}

// Keeps exact large-ring indices in the worker-prepared cache without changing the default limit.
static void test_large_retained_ring(void) {
    MftTile tile={0};
    tile.polygon_count=tile.polygon_ring_total=1;
    tile.polygon_point_total=600;
    tile.polygons=calloc(1,sizeof(*tile.polygons));
    tile.polygon_rings=calloc(1,sizeof(*tile.polygon_rings));
    tile.polygon_points=calloc(1200,sizeof(*tile.polygon_points));
    assert(tile.polygons && tile.polygon_rings && tile.polygon_points);
    tile.polygons[0].ring_count=1; tile.polygon_rings[0]=600;
    for (unsigned i=0;i<600;i++) {
        double angle=i*6.283185307179586/600;
        tile.polygon_points[2*i]=(uint16_t)(32000+30000*cos(angle));
        tile.polygon_points[2*i+1]=(uint16_t)(32000+30000*sin(angle));
    }
    assert(polygon_cache_build(&tile));
    assert(tile.polygon_tri_index_total==0);
    unsigned checks=0;
    assert(!polygon_cache_build_cancelable(&tile,8192,NULL,cancel_after_checks,&checks));
    assert(!tile.polygon_tri_cached);
    assert(polygon_cache_build_with_limit(&tile,8192,NULL));
    assert(tile.polygon_tri_index_total==(600-2)*3);
    for (unsigned i=0;i<tile.polygon_tri_index_total;i++) assert(tile.polygon_tri_indices[i]<600);
    free_tile(&tile);
}

int main(void) {
    test_large_retained_ring();
    test_ring_bounds_quarantine();
    test_ring_min_points_quarantine();
    test_winding_normalization();
    test_polygon_quarantine_on_bad_header();
    printf("polygon_cache_guardrails_test: success\n");
    return 0;
}
