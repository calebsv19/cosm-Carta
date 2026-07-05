#include "vk_tile_cache_fill_mesh.h"

#include "vk_polygon_fill_geometry_guard.h"

#include "map/polygon_triangulator.h"

#include <stdlib.h>

static bool kind_is_polygon(TileLayerKind kind) {
    return kind == TILE_LAYER_POLY_WATER ||
           kind == TILE_LAYER_POLY_PARK ||
           kind == TILE_LAYER_POLY_LANDUSE ||
           kind == TILE_LAYER_POLY_BUILDING;
}

static void mesh_fill_color_for_kind(TileLayerKind kind, float *r, float *g, float *b, float *a) {
    if (!r || !g || !b || !a) {
        return;
    }
    switch (kind) {
        case TILE_LAYER_POLY_WATER:
            *r = 0.30f; *g = 0.42f; *b = 0.58f; *a = 0.24f;
            break;
        case TILE_LAYER_POLY_PARK:
            *r = 0.35f; *g = 0.53f; *b = 0.36f; *a = 0.12f;
            break;
        case TILE_LAYER_POLY_LANDUSE:
            *r = 0.52f; *g = 0.54f; *b = 0.49f; *a = 0.08f;
            break;
        case TILE_LAYER_POLY_BUILDING:
            *r = 0.45f; *g = 0.45f; *b = 0.45f; *a = 0.10f;
            break;
        default:
            *r = 0.92f; *g = 0.92f; *b = 0.92f; *a = 1.0f;
            break;
    }
}

static bool polygon_ring_is_convex(const uint16_t *points, uint32_t count) {
    if (!points || count < 4u) {
        return true;
    }
    int sign = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t i0 = i;
        uint32_t i1 = (i + 1u) % count;
        uint32_t i2 = (i + 2u) % count;
        float x0 = (float)points[i0 * 2u];
        float y0 = (float)points[i0 * 2u + 1u];
        float x1 = (float)points[i1 * 2u];
        float y1 = (float)points[i1 * 2u + 1u];
        float x2 = (float)points[i2 * 2u];
        float y2 = (float)points[i2 * 2u + 1u];
        float dx1 = x1 - x0;
        float dy1 = y1 - y0;
        float dx2 = x2 - x1;
        float dy2 = y2 - y1;
        float cross = dx1 * dy2 - dy1 * dx2;
        if (cross > -0.001f && cross < 0.001f) {
            continue;
        }
        int curr = cross > 0.0f ? 1 : -1;
        if (sign == 0) {
            sign = curr;
        } else if (sign != curr) {
            return false;
        }
    }
    return true;
}

static bool polygon_get_cached_indices(const MftTile *tile,
                                       uint32_t ring_index,
                                       const uint32_t **out_indices,
                                       uint32_t *out_count) {
    if (!tile || !tile->polygon_tri_cached || !out_indices || !out_count) {
        return false;
    }
    if (ring_index >= tile->polygon_ring_total || !tile->polygon_tri_ring_offsets ||
        !tile->polygon_tri_ring_counts || !tile->polygon_tri_indices) {
        return false;
    }
    uint32_t offset = tile->polygon_tri_ring_offsets[ring_index];
    uint32_t count = tile->polygon_tri_ring_counts[ring_index];
    if (count == 0u || offset + count > tile->polygon_tri_index_total) {
        return false;
    }
    *out_indices = tile->polygon_tri_indices + offset;
    *out_count = count;
    return true;
}

static bool polygon_build_fallback_indices(const uint16_t *ring_points,
                                           uint32_t ring_count,
                                           uint32_t **out_indices,
                                           uint32_t *out_count) {
    if (!ring_points || ring_count < 3u || !out_indices || !out_count) {
        return false;
    }
    uint32_t max_indices = (ring_count - 2u) * 3u;
    int *tmp = (int *)malloc(sizeof(int) * (size_t)max_indices);
    if (!tmp) {
        return false;
    }
    int built = 0;
    PolygonTriangulationMode mode = polygon_ring_is_convex(ring_points, ring_count)
        ? POLYGON_TRIANGULATION_FAN
        : POLYGON_TRIANGULATION_EAR_CLIP;
    bool ok = polygon_triangulate(ring_points, ring_count, mode, tmp, &built, (int)max_indices);
    if (!ok || built < 3 || (built % 3) != 0) {
        free(tmp);
        return false;
    }
    uint32_t *indices = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)built);
    if (!indices) {
        free(tmp);
        return false;
    }
    for (int i = 0; i < built; ++i) {
        indices[i] = (uint32_t)tmp[i];
    }
    free(tmp);
    *out_indices = indices;
    *out_count = (uint32_t)built;
    return true;
}

bool vk_tile_cache_build_polygon_fill_mesh(VkTileCache *cache,
                                           void *vk_renderer,
                                           VkTileCacheEntry *entry,
                                           const MftTile *tile) {
    if (!cache || !vk_renderer || !entry || !tile || !kind_is_polygon(entry->kind)) {
        return false;
    }
    if (tile->polygon_count == 0u || !tile->polygons || !tile->polygon_rings || !tile->polygon_points) {
        return false;
    }
    uint32_t max_ring_points = 8192u;
    uint32_t max_ring_indices = 24576u;
    if (entry->kind == TILE_LAYER_POLY_WATER) {
        max_ring_points = 32768u;
        max_ring_indices = 98304u;
    }

    uint32_t total_vertices = 0u;
    uint32_t total_indices = 0u;
    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const MftPolygon *polygon = &tile->polygons[i];
        uint32_t point_offset = polygon->point_offset;
        for (uint16_t r = 0; r < polygon->ring_count; ++r) {
            uint32_t ring_index = polygon->ring_offset + r;
            if (ring_index >= tile->polygon_ring_total) {
                cache->mesh_build_failures += 1u;
                cache->fill_mesh_build_failures += 1u;
                return false;
            }
            uint32_t ring_count = tile->polygon_rings[ring_index];
            if (ring_count >= 3u) {
                if (point_offset >= tile->polygon_point_total ||
                    point_offset + ring_count > tile->polygon_point_total) {
                    cache->mesh_build_failures += 1u;
                    cache->fill_mesh_build_failures += 1u;
                    return false;
                }
                const uint16_t *ring_points = &tile->polygon_points[point_offset * 2u];
                if (!vk_polygon_fill_ring_allowed_for_retained_mesh(entry->kind, ring_points, ring_count)) {
                    point_offset += ring_count;
                    continue;
                }
                const uint32_t *cached_indices = NULL;
                uint32_t cached_count = 0u;
                uint32_t *fallback_indices = NULL;
                uint32_t fallback_count = 0u;
                bool has_indices = polygon_get_cached_indices(tile, ring_index, &cached_indices, &cached_count);
                if (!has_indices) {
                    has_indices = polygon_build_fallback_indices(ring_points, ring_count,
                                                                 &fallback_indices, &fallback_count);
                    cached_indices = fallback_indices;
                    cached_count = fallback_count;
                }
                if (has_indices && cached_count >= 3u) {
                    if (ring_count <= max_ring_points && cached_count <= max_ring_indices) {
                        total_vertices += ring_count;
                        total_indices += cached_count;
                    }
                } else {
                    cache->mesh_build_failures += 1u;
                    cache->fill_mesh_build_failures += 1u;
                }
                free(fallback_indices);
            }
            point_offset += ring_count;
        }
    }

    if (total_vertices < 3u || total_indices < 3u) {
        return false;
    }

    SDL_FPoint *verts = (SDL_FPoint *)malloc(sizeof(SDL_FPoint) * (size_t)total_vertices);
    uint32_t *indices = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)total_indices);
    if (!verts || !indices) {
        free(verts);
        free(indices);
        cache->mesh_build_failures += 1u;
        cache->fill_mesh_build_failures += 1u;
        return false;
    }

    uint32_t v_write = 0u;
    uint32_t i_write = 0u;
    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const MftPolygon *polygon = &tile->polygons[i];
        uint32_t point_offset = polygon->point_offset;
        for (uint16_t r = 0; r < polygon->ring_count; ++r) {
            uint32_t ring_index = polygon->ring_offset + r;
            uint32_t ring_count = tile->polygon_rings[ring_index];
            if (ring_count < 3u) {
                point_offset += ring_count;
                continue;
            }
            const uint16_t *ring_points = &tile->polygon_points[point_offset * 2u];
            if (!vk_polygon_fill_ring_allowed_for_retained_mesh(entry->kind, ring_points, ring_count)) {
                point_offset += ring_count;
                continue;
            }
            const uint32_t *ring_indices = NULL;
            uint32_t ring_index_count = 0u;
            uint32_t *fallback_indices = NULL;
            uint32_t fallback_count = 0u;
            bool has_indices = polygon_get_cached_indices(tile, ring_index, &ring_indices, &ring_index_count);
            if (!has_indices) {
                has_indices = polygon_build_fallback_indices(ring_points, ring_count,
                                                             &fallback_indices, &fallback_count);
                ring_indices = fallback_indices;
                ring_index_count = fallback_count;
            }
            if (!has_indices || ring_index_count < 3u) {
                free(fallback_indices);
                point_offset += ring_count;
                continue;
            }
            if (ring_count > max_ring_points || ring_index_count > max_ring_indices) {
                free(fallback_indices);
                point_offset += ring_count;
                continue;
            }

            uint32_t base = v_write;
            for (uint32_t p = 0; p < ring_count; ++p) {
                uint32_t idx = (point_offset + p) * 2u;
                verts[v_write++] = (SDL_FPoint){
                    (float)tile->polygon_points[idx],
                    (float)tile->polygon_points[idx + 1u]
                };
            }
            for (uint32_t k = 0; k < ring_index_count; ++k) {
                uint32_t local = ring_indices[k];
                if (local >= ring_count) {
                    continue;
                }
                indices[i_write++] = base + local;
            }
            free(fallback_indices);
            point_offset += ring_count;
        }
    }

    float r = 0.92f;
    float g = 0.92f;
    float b = 0.92f;
    float a = 1.0f;
    mesh_fill_color_for_kind(entry->kind, &r, &g, &b, &a);
    VkResult result = vk_renderer_create_tri_mesh((VkRenderer *)vk_renderer,
                                                  verts,
                                                  v_write,
                                                  indices,
                                                  i_write,
                                                  r, g, b, a,
                                                  &entry->fill_mesh);
    free(verts);
    free(indices);
    if (result != VK_SUCCESS) {
        cache->mesh_build_failures += 1u;
        cache->fill_mesh_build_failures += 1u;
        return false;
    }
    entry->fill_mesh_ready = true;
    cache->mesh_bytes += entry->fill_mesh.vertex_buffer.size;
    cache->mesh_bytes += entry->fill_mesh.index_buffer.size;
    return true;
}
