#include "map/polygon_cache.h"

#include "map/polygon_triangulator.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static double polygon_ring_signed_area(const uint16_t *points, uint32_t count) {
    if (!points || count < 3u) {
        return 0.0;
    }

    double area = 0.0;
    for (uint32_t i = 0u; i < count; ++i) {
        uint32_t j = (i + 1u) % count;
        double xi = (double)points[(size_t)i * 2u];
        double yi = (double)points[(size_t)i * 2u + 1u];
        double xj = (double)points[(size_t)j * 2u];
        double yj = (double)points[(size_t)j * 2u + 1u];
        area += xi * yj - xj * yi;
    }
    return area * 0.5;
}

static void polygon_ring_reverse(uint16_t *points, uint32_t count) {
    if (!points || count < 2u) {
        return;
    }
    for (uint32_t i = 0u, j = count - 1u; i < j; ++i, --j) {
        uint16_t ax = points[(size_t)i * 2u];
        uint16_t ay = points[(size_t)i * 2u + 1u];
        points[(size_t)i * 2u] = points[(size_t)j * 2u];
        points[(size_t)i * 2u + 1u] = points[(size_t)j * 2u + 1u];
        points[(size_t)j * 2u] = ax;
        points[(size_t)j * 2u + 1u] = ay;
    }
}

bool polygon_cache_build_with_stats(MftTile *tile, PolygonCacheBuildStats *out_stats) {
    PolygonCacheBuildStats stats = {0};
    if (!tile || tile->polygon_ring_total == 0u) {
        if (out_stats) {
            *out_stats = stats;
        }
        return false;
    }
    if (tile->polygon_tri_cached) {
        if (out_stats) {
            *out_stats = stats;
        }
        return true;
    }
    if ((tile->polygon_count > 0u && (!tile->polygons || !tile->polygon_rings)) ||
        (tile->polygon_point_total > 0u && !tile->polygon_points)) {
        stats.polygon_quarantined = tile->polygon_count;
        if (out_stats) {
            *out_stats = stats;
        }
        return false;
    }

    tile->polygon_tri_ring_offsets = (uint32_t *)calloc(tile->polygon_ring_total, sizeof(uint32_t));
    tile->polygon_tri_ring_counts = (uint32_t *)calloc(tile->polygon_ring_total, sizeof(uint32_t));
    if (!tile->polygon_tri_ring_offsets || !tile->polygon_tri_ring_counts) {
        if (out_stats) {
            *out_stats = stats;
        }
        return false;
    }

    uint32_t indices_capacity = 0u;
    uint32_t indices_count = 0u;
    uint32_t *indices = NULL;

    for (uint32_t i = 0u; i < tile->polygon_count; ++i) {
        const MftPolygon *polygon = &tile->polygons[i];
        uint64_t ring_end = (uint64_t)polygon->ring_offset + (uint64_t)polygon->ring_count;
        if (polygon->ring_offset >= tile->polygon_ring_total ||
            ring_end > tile->polygon_ring_total ||
            polygon->point_offset > tile->polygon_point_total) {
            stats.polygon_quarantined += 1u;
            continue;
        }

        uint32_t point_offset = polygon->point_offset;
        for (uint16_t r = 0u; r < polygon->ring_count; ++r) {
            uint32_t ring_idx = polygon->ring_offset + r;
            uint32_t ring_count = tile->polygon_rings[ring_idx];
            tile->polygon_tri_ring_offsets[ring_idx] = indices_count;
            tile->polygon_tri_ring_counts[ring_idx] = 0u;

            if (point_offset > tile->polygon_point_total) {
                stats.ring_bounds_quarantined += 1u;
                continue;
            }

            uint32_t remaining = tile->polygon_point_total - point_offset;
            bool ring_out_of_bounds = ring_count > remaining;
            uint32_t safe_ring_count = ring_out_of_bounds ? remaining : ring_count;

            if (ring_out_of_bounds) {
                stats.ring_bounds_quarantined += 1u;
                point_offset += safe_ring_count;
                continue;
            }

            if (safe_ring_count < 3u) {
                stats.ring_min_points_quarantined += 1u;
                point_offset += safe_ring_count;
                continue;
            }

            uint16_t *ring_points = &tile->polygon_points[(size_t)point_offset * 2u];
            double signed_area = polygon_ring_signed_area(ring_points, safe_ring_count);
            if (fabs(signed_area) < 1e-3) {
                stats.ring_degenerate_quarantined += 1u;
                point_offset += safe_ring_count;
                continue;
            }

            bool want_ccw = (r == 0u);
            bool have_ccw = signed_area > 0.0;
            if (want_ccw != have_ccw) {
                polygon_ring_reverse(ring_points, safe_ring_count);
                stats.ring_winding_normalized += 1u;
            }

            if (safe_ring_count <= POLYGON_CACHE_MAX_FILL_POINTS) {
                uint32_t max_indices = (safe_ring_count - 2u) * 3u;
                int *ring_indices = (int *)malloc(sizeof(int) * max_indices);
                if (ring_indices) {
                    int ring_index_count = 0;
                    if (polygon_triangulate(ring_points, safe_ring_count, POLYGON_TRIANGULATION_EAR_CLIP,
                            ring_indices, &ring_index_count, (int)max_indices)) {
                        uint32_t needed = indices_count + (uint32_t)ring_index_count;
                        if (needed > indices_capacity) {
                            uint32_t next_capacity = indices_capacity == 0u ? needed : indices_capacity;
                            while (next_capacity < needed) {
                                next_capacity = next_capacity * 2u;
                            }
                            uint32_t *next = (uint32_t *)realloc(indices, next_capacity * sizeof(uint32_t));
                            if (next) {
                                indices = next;
                                indices_capacity = next_capacity;
                            }
                        }

                        if (indices && needed <= indices_capacity) {
                            for (int k = 0; k < ring_index_count; ++k) {
                                indices[indices_count++] = (uint32_t)ring_indices[k];
                            }
                            tile->polygon_tri_ring_counts[ring_idx] = (uint32_t)ring_index_count;
                        }
                    }
                    free(ring_indices);
                }
            }

            point_offset += safe_ring_count;
        }
    }

    stats.ring_quarantine_total =
        stats.ring_bounds_quarantined +
        stats.ring_min_points_quarantined +
        stats.ring_degenerate_quarantined;

    tile->polygon_tri_indices = indices;
    tile->polygon_tri_index_total = indices_count;
    tile->polygon_tri_cached = true;
    if (out_stats) {
        *out_stats = stats;
    }
    return true;
}

bool polygon_cache_build(MftTile *tile) {
    return polygon_cache_build_with_stats(tile, NULL);
}
