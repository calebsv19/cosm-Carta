#include "map/polygon_cache.h"

#include "map/polygon_triangulator.h"

#include <stdlib.h>
#include <string.h>

bool polygon_cache_build(MftTile *tile) {
    if (!tile || tile->polygon_ring_total == 0) {
        return false;
    }
    if (tile->polygon_tri_cached) {
        return true;
    }

    tile->polygon_tri_ring_offsets = (uint32_t *)calloc(tile->polygon_ring_total, sizeof(uint32_t));
    tile->polygon_tri_ring_counts = (uint32_t *)calloc(tile->polygon_ring_total, sizeof(uint32_t));
    if (!tile->polygon_tri_ring_offsets || !tile->polygon_tri_ring_counts) {
        return false;
    }

    uint32_t indices_capacity = 0;
    uint32_t indices_count = 0;
    uint32_t *indices = NULL;

    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const MftPolygon *polygon = &tile->polygons[i];
        uint32_t point_offset = polygon->point_offset;
        for (uint16_t r = 0; r < polygon->ring_count; ++r) {
            uint32_t ring_count = tile->polygon_rings[polygon->ring_offset + r];
            uint32_t ring_idx = polygon->ring_offset + r;
            tile->polygon_tri_ring_offsets[ring_idx] = indices_count;
            tile->polygon_tri_ring_counts[ring_idx] = 0;

            if (ring_count >= 3 && ring_count <= POLYGON_CACHE_MAX_FILL_POINTS) {
                uint32_t max_indices = (ring_count - 2) * 3;
                int *ring_indices = (int *)malloc(sizeof(int) * max_indices);
                if (ring_indices) {
                    int ring_index_count = 0;
                    const uint16_t *ring_points = &tile->polygon_points[point_offset * 2];
                    if (polygon_triangulate(ring_points, ring_count, POLYGON_TRIANGULATION_EAR_CLIP,
                            ring_indices, &ring_index_count, (int)max_indices)) {
                        uint32_t needed = indices_count + (uint32_t)ring_index_count;
                        if (needed > indices_capacity) {
                            uint32_t next_capacity = indices_capacity == 0 ? needed : indices_capacity;
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

            point_offset += ring_count;
        }
    }

    tile->polygon_tri_indices = indices;
    tile->polygon_tri_index_total = indices_count;
    tile->polygon_tri_cached = true;
    return true;
}
