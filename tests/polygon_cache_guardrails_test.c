#include "map/polygon_cache.h"

#include <assert.h>
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

int main(void) {
    test_ring_bounds_quarantine();
    test_ring_min_points_quarantine();
    test_winding_normalization();
    test_polygon_quarantine_on_bad_header();
    printf("polygon_cache_guardrails_test: success\n");
    return 0;
}
