#include "render/vk_tile_cache.h"
#include "map/polygon_triangulator.h"

#include <stdlib.h>
#include <string.h>

static bool coord_equals(TileCoord a, TileCoord b) {
    return a.z == b.z && a.x == b.x && a.y == b.y;
}

static bool kind_has_floor(TileLayerKind kind) {
    return kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL;
}

static bool kind_is_road(TileLayerKind kind) {
    return kind == TILE_LAYER_ROAD_ARTERY || kind == TILE_LAYER_ROAD_LOCAL;
}

static bool kind_is_polygon(TileLayerKind kind) {
    return kind == TILE_LAYER_POLY_WATER ||
           kind == TILE_LAYER_POLY_PARK ||
           kind == TILE_LAYER_POLY_LANDUSE ||
           kind == TILE_LAYER_POLY_BUILDING;
}

static VkTileCacheEntry *find_entry(VkTileCache *cache, TileLayerKind kind, TileCoord coord, TileZoomBand band) {
    if (!cache || !cache->entries) {
        return NULL;
    }

    for (uint32_t i = 0; i < cache->capacity; ++i) {
        VkTileCacheEntry *entry = &cache->entries[i];
        if (!entry->occupied) {
            continue;
        }
        if (entry->kind == kind && entry->band == band && coord_equals(entry->coord, coord)) {
            return entry;
        }
    }
    return NULL;
}

static bool entry_is_protected(const VkTileCache *cache, TileLayerKind incoming_kind, const VkTileCacheEntry *entry) {
    if (!cache || !entry || !entry->occupied) {
        return false;
    }
    TileLayerKind resident_kind = entry->kind;
    if (!kind_has_floor(resident_kind)) {
        return false;
    }
    if (incoming_kind == resident_kind) {
        return false;
    }
    uint32_t resident = cache->resident_by_kind[resident_kind];
    uint32_t floor = cache->min_resident_by_kind[resident_kind];
    return resident <= floor;
}

static VkTileCacheEntry *pick_slot(VkTileCache *cache, TileLayerKind incoming_kind) {
    if (!cache || !cache->entries) {
        return NULL;
    }

    VkTileCacheEntry *empty = NULL;
    VkTileCacheEntry *oldest = NULL;
    VkTileCacheEntry *fallback_oldest = NULL;
    for (uint32_t i = 0; i < cache->capacity; ++i) {
        VkTileCacheEntry *entry = &cache->entries[i];
        if (!entry->occupied) {
            empty = entry;
            break;
        }
        if (!fallback_oldest || entry->last_used < fallback_oldest->last_used) {
            fallback_oldest = entry;
        }
        if (entry_is_protected(cache, incoming_kind, entry)) {
            continue;
        }
        if (!oldest || entry->last_used < oldest->last_used) {
            oldest = entry;
        }
    }
    if (empty) {
        return empty;
    }
    if (oldest) {
        return oldest;
    }
    return fallback_oldest;
}

#if defined(MAPFORGE_HAVE_VK)
static void road_class_color(RoadClass road_class, float *r, float *g, float *b, float *a) {
    if (!r || !g || !b || !a) {
        return;
    }
    switch (road_class) {
        case ROAD_CLASS_MOTORWAY:
            *r = 230.0f / 255.0f; *g = 160.0f / 255.0f; *b = 50.0f / 255.0f; *a = 1.0f;
            break;
        case ROAD_CLASS_TRUNK:
            *r = 220.0f / 255.0f; *g = 140.0f / 255.0f; *b = 60.0f / 255.0f; *a = 1.0f;
            break;
        case ROAD_CLASS_PRIMARY:
            *r = 210.0f / 255.0f; *g = 120.0f / 255.0f; *b = 70.0f / 255.0f; *a = 1.0f;
            break;
        case ROAD_CLASS_SECONDARY:
            *r = 200.0f / 255.0f; *g = 200.0f / 255.0f; *b = 200.0f / 255.0f; *a = 1.0f;
            break;
        case ROAD_CLASS_TERTIARY:
            *r = 180.0f / 255.0f; *g = 180.0f / 255.0f; *b = 180.0f / 255.0f; *a = 1.0f;
            break;
        case ROAD_CLASS_RESIDENTIAL:
            *r = 160.0f / 255.0f; *g = 160.0f / 255.0f; *b = 160.0f / 255.0f; *a = 1.0f;
            break;
        case ROAD_CLASS_SERVICE:
            *r = 130.0f / 255.0f; *g = 130.0f / 255.0f; *b = 130.0f / 255.0f; *a = 1.0f;
            break;
        case ROAD_CLASS_FOOTWAY:
            *r = 110.0f / 255.0f; *g = 110.0f / 255.0f; *b = 110.0f / 255.0f; *a = 200.0f / 255.0f;
            break;
        case ROAD_CLASS_PATH:
        default:
            *r = 90.0f / 255.0f; *g = 90.0f / 255.0f; *b = 90.0f / 255.0f; *a = 180.0f / 255.0f;
            break;
    }
}

static void mesh_outline_color_for_kind(TileLayerKind kind, float *r, float *g, float *b, float *a) {
    if (!r || !g || !b || !a) {
        return;
    }

    switch (kind) {
        case TILE_LAYER_POLY_WATER:
            *r = 0.28f; *g = 0.42f; *b = 0.62f; *a = 0.55f;
            break;
        case TILE_LAYER_POLY_PARK:
            *r = 0.33f; *g = 0.52f; *b = 0.34f; *a = 0.50f;
            break;
        case TILE_LAYER_POLY_LANDUSE:
            *r = 0.50f; *g = 0.53f; *b = 0.47f; *a = 0.42f;
            break;
        case TILE_LAYER_POLY_BUILDING:
            *r = 0.42f; *g = 0.42f; *b = 0.42f; *a = 0.48f;
            break;
        case TILE_LAYER_ROAD_ARTERY:
        case TILE_LAYER_ROAD_LOCAL:
        default:
            *r = 0.92f; *g = 0.92f; *b = 0.92f; *a = 1.0f;
            break;
    }
}

static void mesh_fill_color_for_kind(TileLayerKind kind, float *r, float *g, float *b, float *a) {
    if (!r || !g || !b || !a) {
        return;
    }
    switch (kind) {
        case TILE_LAYER_POLY_WATER:
            *r = 0.30f; *g = 0.42f; *b = 0.58f; *a = 0.14f;
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

static uint32_t polygon_outline_base_step(uint32_t ring_count) {
    if (ring_count > 8192u) {
        return 12u;
    }
    if (ring_count > 4096u) {
        return 8u;
    }
    if (ring_count > 2048u) {
        return 4u;
    }
    if (ring_count > 1024u) {
        return 2u;
    }
    return 1u;
}

static uint32_t polygon_outline_segment_count(uint32_t ring_count, uint32_t step_scale) {
    if (ring_count < 2u) {
        return 0u;
    }
    uint32_t step = polygon_outline_base_step(ring_count) * (step_scale == 0u ? 1u : step_scale);
    if (step == 0u) {
        step = 1u;
    }
    return (ring_count + step - 1u) / step;
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

static void destroy_entry_meshes(VkTileCache *cache, void *vk_renderer, VkTileCacheEntry *entry) {
    if (!cache || !entry) {
        return;
    }
    for (uint32_t i = 0u; i < 3u; ++i) {
        if (!entry->water_lod_mesh_ready[i]) {
            continue;
        }
        cache->mesh_vertices -= entry->water_lod_mesh[i].vertex_count;
        cache->mesh_bytes -= entry->water_lod_mesh[i].vertex_buffer.size;
        if (vk_renderer) {
            vk_renderer_destroy_line_mesh((VkRenderer *)vk_renderer, &entry->water_lod_mesh[i]);
        }
        entry->water_lod_mesh_ready[i] = false;
    }
    if (entry->mesh_ready) {
        cache->mesh_vertices -= entry->mesh.vertex_count;
        cache->mesh_bytes -= entry->mesh.vertex_buffer.size;
        if (vk_renderer) {
            vk_renderer_destroy_line_mesh((VkRenderer *)vk_renderer, &entry->mesh);
        }
        entry->mesh_ready = false;
    }
    if (entry->fill_mesh_ready) {
        cache->mesh_bytes -= entry->fill_mesh.vertex_buffer.size;
        cache->mesh_bytes -= entry->fill_mesh.index_buffer.size;
        if (vk_renderer) {
            vk_renderer_destroy_tri_mesh((VkRenderer *)vk_renderer, &entry->fill_mesh);
        }
        entry->fill_mesh_ready = false;
    }
    for (int rc = 0; rc <= ROAD_CLASS_PATH; ++rc) {
        if (!entry->road_mesh_ready[rc]) {
            continue;
        }
        cache->mesh_vertices -= entry->road_mesh[rc].vertex_count;
        cache->mesh_bytes -= entry->road_mesh[rc].vertex_buffer.size;
        if (vk_renderer) {
            vk_renderer_destroy_line_mesh((VkRenderer *)vk_renderer, &entry->road_mesh[rc]);
        }
        entry->road_mesh_ready[rc] = false;
    }
}

static bool build_polygon_outline_mesh_points(VkTileCache *cache,
                                              void *vk_renderer,
                                              TileLayerKind kind,
                                              const MftTile *tile,
                                              const uint32_t *polygon_rings,
                                              const uint16_t *polygon_points,
                                              VkRendererLineMesh *out_mesh,
                                              uint32_t *out_vertex_count,
                                              uint64_t *out_buffer_bytes) {
    if (!cache || !vk_renderer || !tile || !polygon_rings || !polygon_points || !out_mesh) {
        return false;
    }

    uint32_t *ring_point_offsets = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)tile->polygon_ring_total);
    if (!ring_point_offsets) {
        return false;
    }
    {
        uint32_t running_offset = 0u;
        for (uint32_t ring = 0u; ring < tile->polygon_ring_total; ++ring) {
            ring_point_offsets[ring] = running_offset;
            running_offset += polygon_rings[ring];
        }
    }

    uint32_t segment_count = 0u;
    uint32_t polygon_step_scale = 1u;
    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const MftPolygon *polygon = &tile->polygons[i];
        for (uint16_t r = 0; r < polygon->ring_count; ++r) {
            uint32_t ring_index = polygon->ring_offset + r;
            if (ring_index >= tile->polygon_ring_total) {
                cache->mesh_build_failures += 1u;
                free(ring_point_offsets);
                return false;
            }
            uint32_t ring_count = polygon_rings[ring_index];
            if (ring_count >= 2u) {
                segment_count += polygon_outline_segment_count(ring_count, 1u);
            }
        }
    }

    {
        const uint32_t kPolygonOutlineSegmentCap = 24000u;
        if (segment_count > kPolygonOutlineSegmentCap) {
            polygon_step_scale = (segment_count + kPolygonOutlineSegmentCap - 1u) / kPolygonOutlineSegmentCap;
            if (polygon_step_scale > 32u) {
                polygon_step_scale = 32u;
            }
            segment_count = 0u;
            for (uint32_t i = 0; i < tile->polygon_count; ++i) {
                const MftPolygon *polygon = &tile->polygons[i];
                    for (uint16_t r = 0; r < polygon->ring_count; ++r) {
                        uint32_t ring_index = polygon->ring_offset + r;
                        if (ring_index >= tile->polygon_ring_total) {
                            cache->mesh_build_failures += 1u;
                            free(ring_point_offsets);
                            return false;
                        }
                        uint32_t ring_count = polygon_rings[ring_index];
                        if (ring_count >= 2u) {
                            segment_count += polygon_outline_segment_count(ring_count, polygon_step_scale);
                    }
                }
            }
        }
    }

    if (segment_count == 0u) {
        free(ring_point_offsets);
        return false;
    }
    SDL_FPoint *verts = (SDL_FPoint *)malloc(sizeof(SDL_FPoint) * (size_t)(segment_count * 2u));
    if (!verts) {
        free(ring_point_offsets);
        return false;
    }

    uint32_t v = 0u;
    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const MftPolygon *polygon = &tile->polygons[i];
        for (uint16_t r = 0; r < polygon->ring_count; ++r) {
            uint32_t ring_index = polygon->ring_offset + r;
            uint32_t ring_count = polygon_rings[ring_index];
            if (ring_count < 2u) {
                continue;
            }
            uint32_t point_offset = ring_point_offsets[ring_index];
            uint32_t step = polygon_outline_base_step(ring_count) * polygon_step_scale;
            if (step == 0u) {
                step = 1u;
            }
            for (uint32_t p = 0; p < ring_count; p += step) {
                uint32_t p0 = point_offset + p;
                uint32_t next = p + step;
                if (next >= ring_count) {
                    next = 0u;
                }
                uint32_t p1 = point_offset + next;
                uint32_t idx0 = p0 * 2u;
                uint32_t idx1 = p1 * 2u;
                verts[v++] = (SDL_FPoint){(float)polygon_points[idx0], (float)polygon_points[idx0 + 1u]};
                verts[v++] = (SDL_FPoint){(float)polygon_points[idx1], (float)polygon_points[idx1 + 1u]};
            }
        }
    }

    float r = 0.92f;
    float g = 0.92f;
    float b = 0.92f;
    float a = 1.0f;
    mesh_outline_color_for_kind(kind, &r, &g, &b, &a);
    VkResult result = vk_renderer_create_line_list_mesh((VkRenderer *)vk_renderer, verts, v, r, g, b, a, out_mesh);
    free(verts);
    free(ring_point_offsets);
    if (result != VK_SUCCESS) {
        cache->mesh_build_failures += 1u;
        return false;
    }
    if (out_vertex_count) {
        *out_vertex_count = out_mesh->vertex_count;
    }
    if (out_buffer_bytes) {
        *out_buffer_bytes = out_mesh->vertex_buffer.size;
    }
    return true;
}

static bool build_entry_mesh(VkTileCache *cache, void *vk_renderer, VkTileCacheEntry *entry, const MftTile *tile) {
    if (!vk_renderer || !entry || !tile) {
        return false;
    }

    uint32_t segment_count = 0u;
    uint32_t polygon_step_scale = 1u;
    if (kind_is_road(entry->kind)) {
        uint32_t class_segment_counts[ROAD_CLASS_PATH + 1] = {0};
        if (tile->polyline_count == 0u) {
            return false;
        }
        for (uint32_t i = 0; i < tile->polyline_count; ++i) {
            const MftPolyline *polyline = &tile->polylines[i];
            if (polyline->point_count >= 2u) {
                if (polyline->point_offset >= tile->point_total ||
                    polyline->point_offset + polyline->point_count > tile->point_total) {
                    if (cache) {
                        cache->mesh_build_failures += 1u;
                    }
                    return false;
                }
                uint32_t segs = (polyline->point_count - 1u);
                segment_count += segs;
                if (polyline->road_class <= ROAD_CLASS_PATH) {
                    class_segment_counts[polyline->road_class] += segs;
                }
            }
        }
        if (segment_count == 0u) {
            return false;
        }
        for (int rc = ROAD_CLASS_MOTORWAY; rc <= ROAD_CLASS_PATH; ++rc) {
            uint32_t class_segments = class_segment_counts[rc];
            if (class_segments == 0u) {
                continue;
            }
            SDL_FPoint *verts = (SDL_FPoint *)malloc(sizeof(SDL_FPoint) * (size_t)(class_segments * 2u));
            if (!verts) {
                if (cache) {
                    cache->mesh_build_failures += 1u;
                }
                return false;
            }
            uint32_t v = 0u;
            for (uint32_t i = 0; i < tile->polyline_count; ++i) {
                const MftPolyline *polyline = &tile->polylines[i];
                if (polyline->road_class != (RoadClass)rc || polyline->point_count < 2u) {
                    continue;
                }
                for (uint32_t p = 0; p + 1u < polyline->point_count; ++p) {
                    uint32_t idx0 = (polyline->point_offset + p) * 2u;
                    uint32_t idx1 = (polyline->point_offset + p + 1u) * 2u;
                    if (idx1 + 1u >= tile->point_total * 2u) {
                        free(verts);
                        if (cache) {
                            cache->mesh_build_failures += 1u;
                        }
                        return false;
                    }
                    verts[v++] = (SDL_FPoint){(float)tile->points[idx0], (float)tile->points[idx0 + 1u]};
                    verts[v++] = (SDL_FPoint){(float)tile->points[idx1], (float)tile->points[idx1 + 1u]};
                }
            }
            float r = 0.92f;
            float g = 0.92f;
            float b = 0.92f;
            float a = 1.0f;
            road_class_color((RoadClass)rc, &r, &g, &b, &a);
            VkResult result = vk_renderer_create_line_list_mesh((VkRenderer *)vk_renderer,
                                                                verts,
                                                                v,
                                                                r, g, b, a,
                                                                &entry->road_mesh[rc]);
            free(verts);
            if (result != VK_SUCCESS) {
                if (cache) {
                    cache->mesh_build_failures += 1u;
                }
                return false;
            }
            entry->road_mesh_ready[rc] = true;
            if (cache) {
                cache->mesh_vertices += entry->road_mesh[rc].vertex_count;
                cache->mesh_bytes += entry->road_mesh[rc].vertex_buffer.size;
            }
        }
        return true;
    } else if (kind_is_polygon(entry->kind)) {
        if (entry->kind == TILE_LAYER_POLY_WATER && tile->water_variants_available) {
            for (uint32_t lod = 0u; lod < 3u; ++lod) {
                if (!tile->water_variant_rings[lod] || !tile->water_variant_points[lod]) {
                    continue;
                }
                uint32_t verts = 0u;
                uint64_t bytes = 0u;
                if (build_polygon_outline_mesh_points(cache,
                                                      vk_renderer,
                                                      entry->kind,
                                                      tile,
                                                      tile->water_variant_rings[lod],
                                                      tile->water_variant_points[lod],
                                                      &entry->water_lod_mesh[lod],
                                                      &verts,
                                                      &bytes)) {
                    entry->water_lod_mesh_ready[lod] = true;
                    cache->mesh_vertices += verts;
                    cache->mesh_bytes += bytes;
                }
            }
            if (entry->water_lod_mesh_ready[0] ||
                entry->water_lod_mesh_ready[1] ||
                entry->water_lod_mesh_ready[2]) {
                return true;
            }
        }
        if (tile->polygon_count == 0u) {
            return false;
        }
        for (uint32_t i = 0; i < tile->polygon_count; ++i) {
            const MftPolygon *polygon = &tile->polygons[i];
            if (polygon->ring_offset >= tile->polygon_ring_total &&
                polygon->ring_count > 0u) {
                if (cache) {
                    cache->mesh_build_failures += 1u;
                }
                return false;
            }
            uint32_t point_offset = polygon->point_offset;
            for (uint16_t r = 0; r < polygon->ring_count; ++r) {
                uint32_t ring_index = polygon->ring_offset + r;
                if (ring_index >= tile->polygon_ring_total) {
                    if (cache) {
                        cache->mesh_build_failures += 1u;
                    }
                    return false;
                }
                uint32_t ring_count = tile->polygon_rings[ring_index];
                (void)point_offset;
                if (ring_count >= 2u) {
                    if (point_offset >= tile->polygon_point_total ||
                        point_offset + ring_count > tile->polygon_point_total) {
                        if (cache) {
                            cache->mesh_build_failures += 1u;
                        }
                        return false;
                    }
                    segment_count += polygon_outline_segment_count(ring_count, 1u);
                }
                point_offset += ring_count;
            }
        }
        {
            const uint32_t kPolygonOutlineSegmentCap = 24000u;
            if (segment_count > kPolygonOutlineSegmentCap) {
                polygon_step_scale = (segment_count + kPolygonOutlineSegmentCap - 1u) / kPolygonOutlineSegmentCap;
                if (polygon_step_scale > 32u) {
                    polygon_step_scale = 32u;
                }
                segment_count = 0u;
                for (uint32_t i = 0; i < tile->polygon_count; ++i) {
                    const MftPolygon *polygon = &tile->polygons[i];
                    uint32_t point_offset = polygon->point_offset;
                    for (uint16_t r = 0; r < polygon->ring_count; ++r) {
                        uint32_t ring_index = polygon->ring_offset + r;
                        if (ring_index >= tile->polygon_ring_total) {
                            if (cache) {
                                cache->mesh_build_failures += 1u;
                            }
                            return false;
                        }
                        uint32_t ring_count = tile->polygon_rings[ring_index];
                        if (ring_count >= 2u) {
                            if (point_offset >= tile->polygon_point_total ||
                                point_offset + ring_count > tile->polygon_point_total) {
                                if (cache) {
                                    cache->mesh_build_failures += 1u;
                                }
                                return false;
                            }
                            segment_count += polygon_outline_segment_count(ring_count, polygon_step_scale);
                        }
                        point_offset += ring_count;
                    }
                }
            }
        }
    } else {
        return false;
    }
    if (segment_count == 0u) {
        return false;
    }

    SDL_FPoint *verts = (SDL_FPoint *)malloc(sizeof(SDL_FPoint) * (size_t)(segment_count * 2u));
    if (!verts) {
        return false;
    }

    uint32_t v = 0u;
    {
        for (uint32_t i = 0; i < tile->polygon_count; ++i) {
            const MftPolygon *polygon = &tile->polygons[i];
            uint32_t point_offset = polygon->point_offset;
            for (uint16_t r = 0; r < polygon->ring_count; ++r) {
                uint32_t ring_index = polygon->ring_offset + r;
                if (ring_index >= tile->polygon_ring_total) {
                    free(verts);
                    if (cache) {
                        cache->mesh_build_failures += 1u;
                    }
                    return false;
                }
                uint32_t ring_count = tile->polygon_rings[ring_index];
                if (ring_count < 2u) {
                    point_offset += ring_count;
                    continue;
                }
                if (point_offset >= tile->polygon_point_total ||
                    point_offset + ring_count > tile->polygon_point_total) {
                    free(verts);
                    if (cache) {
                        cache->mesh_build_failures += 1u;
                    }
                    return false;
                }
                uint32_t step = polygon_outline_base_step(ring_count) * polygon_step_scale;
                if (step == 0u) {
                    step = 1u;
                }
                for (uint32_t p = 0; p < ring_count; p += step) {
                    uint32_t p0 = point_offset + p;
                    uint32_t next = p + step;
                    if (next >= ring_count) {
                        next = 0u;
                    }
                    uint32_t p1 = point_offset + next;
                    uint32_t idx0 = p0 * 2u;
                    uint32_t idx1 = p1 * 2u;
                    if (idx1 + 1u >= tile->polygon_point_total * 2u) {
                        free(verts);
                        if (cache) {
                            cache->mesh_build_failures += 1u;
                        }
                        return false;
                    }
                    verts[v++] = (SDL_FPoint){(float)tile->polygon_points[idx0], (float)tile->polygon_points[idx0 + 1u]};
                    verts[v++] = (SDL_FPoint){(float)tile->polygon_points[idx1], (float)tile->polygon_points[idx1 + 1u]};
                }
                point_offset += ring_count;
            }
        }
    }

    float r = 0.92f;
    float g = 0.92f;
    float b = 0.92f;
    float a = 1.0f;
    mesh_outline_color_for_kind(entry->kind, &r, &g, &b, &a);

    VkResult result = vk_renderer_create_line_list_mesh((VkRenderer *)vk_renderer,
                                                        verts,
                                                        v,
                                                        r,
                                                        g,
                                                        b,
                                                        a,
                                                        &entry->mesh);
    free(verts);
    if (result != VK_SUCCESS) {
        if (cache) {
            cache->mesh_build_failures += 1u;
        }
        return false;
    }
    entry->mesh_ready = true;
    if (cache) {
        cache->mesh_vertices += entry->mesh.vertex_count;
        cache->mesh_bytes += entry->mesh.vertex_buffer.size;
    }
    return true;
}

static bool build_polygon_fill_mesh(VkTileCache *cache,
                                    void *vk_renderer,
                                    VkTileCacheEntry *entry,
                                    const MftTile *tile) {
    if (!cache || !vk_renderer || !entry || !tile || !kind_is_polygon(entry->kind)) {
        return false;
    }
    if (tile->polygon_count == 0u || !tile->polygons || !tile->polygon_rings || !tile->polygon_points) {
        return false;
    }
    if (entry->kind == TILE_LAYER_POLY_WATER) {
        // Skip retained water fills to avoid long triangulation stalls on dense coastal tiles.
        return false;
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
                const uint32_t *cached_indices = NULL;
                uint32_t cached_count = 0u;
                uint32_t *fallback_indices = NULL;
                uint32_t fallback_count = 0u;
                bool has_indices = polygon_get_cached_indices(tile, ring_index, &cached_indices, &cached_count);
                if (!has_indices) {
                    const uint16_t *ring_points = &tile->polygon_points[point_offset * 2u];
                    has_indices = polygon_build_fallback_indices(ring_points, ring_count,
                        &fallback_indices, &fallback_count);
                    cached_indices = fallback_indices;
                    cached_count = fallback_count;
                }
                if (has_indices && cached_count >= 3u) {
                    // Guard against pathological rings in dense regions.
                    if (ring_count <= 8192u && cached_count <= 24576u) {
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
            const uint32_t *ring_indices = NULL;
            uint32_t ring_index_count = 0u;
            uint32_t *fallback_indices = NULL;
            uint32_t fallback_count = 0u;
            bool has_indices = polygon_get_cached_indices(tile, ring_index, &ring_indices, &ring_index_count);
            if (!has_indices) {
                const uint16_t *ring_points = &tile->polygon_points[point_offset * 2u];
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
            if (ring_count > 8192u || ring_index_count > 24576u) {
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
#endif

static void fill_entry_from_tile(VkTileCacheEntry *entry,
                                 TileLayerKind kind,
                                 TileCoord coord,
                                 TileZoomBand band,
                                 const MftTile *tile,
                                 uint64_t stamp) {
    memset(entry, 0, sizeof(*entry));
    entry->occupied = true;
    entry->kind = kind;
    entry->coord = coord;
    entry->band = band;
    entry->last_used = stamp;

    if (!tile) {
        return;
    }

    entry->polyline_count = tile->polyline_count;
    if (kind_is_road(kind)) {
        for (uint32_t i = 0; i < tile->polyline_count; ++i) {
            const MftPolyline *polyline = &tile->polylines[i];
            if (polyline->point_count >= 2u) {
                entry->segment_count += (polyline->point_count - 1u);
            }
            if (polyline->road_class <= ROAD_CLASS_PATH) {
                entry->class_counts[polyline->road_class] += 1u;
            }
        }
    } else if (kind_is_polygon(kind)) {
        entry->polygon_count = tile->polygon_count;
        for (uint32_t i = 0; i < tile->polygon_count; ++i) {
            const MftPolygon *polygon = &tile->polygons[i];
            entry->polygon_ring_count += polygon->ring_count;
            uint32_t point_offset = polygon->point_offset;
            for (uint16_t r = 0; r < polygon->ring_count; ++r) {
                uint32_t ring_count = tile->polygon_rings[polygon->ring_offset + r];
                (void)point_offset;
                if (ring_count >= 2u) {
                    entry->segment_count += ring_count;
                }
                point_offset += ring_count;
            }
        }
    }
}

bool vk_tile_cache_init(VkTileCache *cache, uint32_t capacity) {
    if (!cache || capacity == 0u) {
        return false;
    }

    memset(cache, 0, sizeof(*cache));
    cache->entries = (VkTileCacheEntry *)calloc(capacity, sizeof(VkTileCacheEntry));
    if (!cache->entries) {
        return false;
    }

    cache->capacity = capacity;
    cache->tick = 1u;
    cache->min_resident_by_kind[TILE_LAYER_ROAD_ARTERY] = 96u;
    cache->min_resident_by_kind[TILE_LAYER_ROAD_LOCAL] = 64u;
    return true;
}

void vk_tile_cache_shutdown(VkTileCache *cache) {
    if (!cache) {
        return;
    }
#if defined(MAPFORGE_HAVE_VK)
    for (uint32_t i = 0; i < cache->capacity; ++i) {
        VkTileCacheEntry *entry = &cache->entries[i];
        if (!entry->occupied) {
            continue;
        }
        // Mesh destruction requires renderer; entries are expected to be cleared by owner first.
        entry->mesh_ready = false;
        memset(&entry->mesh, 0, sizeof(entry->mesh));
        entry->fill_mesh_ready = false;
        memset(&entry->fill_mesh, 0, sizeof(entry->fill_mesh));
        memset(entry->road_mesh_ready, 0, sizeof(entry->road_mesh_ready));
        memset(entry->road_mesh, 0, sizeof(entry->road_mesh));
    }
#endif
    free(cache->entries);
    memset(cache, 0, sizeof(*cache));
}

void vk_tile_cache_clear(VkTileCache *cache) {
    if (!cache || !cache->entries) {
        return;
    }
    memset(cache->entries, 0, sizeof(VkTileCacheEntry) * (size_t)cache->capacity);
    cache->count = 0u;
    memset(cache->resident_by_kind, 0, sizeof(cache->resident_by_kind));
    cache->mesh_vertices = 0u;
    cache->mesh_bytes = 0u;
    cache->tick = 1u;
}

void vk_tile_cache_clear_with_renderer(VkTileCache *cache, void *vk_renderer) {
    if (!cache || !cache->entries) {
        return;
    }
#if defined(MAPFORGE_HAVE_VK)
    for (uint32_t i = 0; i < cache->capacity; ++i) {
        VkTileCacheEntry *entry = &cache->entries[i];
        if (!entry->occupied) {
            continue;
        }
        destroy_entry_meshes(cache, vk_renderer, entry);
    }
#else
    (void)vk_renderer;
#endif
    vk_tile_cache_clear(cache);
}

bool vk_tile_cache_on_tile_loaded(VkTileCache *cache,
                                  void *vk_renderer,
                                  TileLayerKind kind,
                                  TileCoord coord,
                                  TileZoomBand band,
                                  const MftTile *tile) {
    if (!cache || !cache->entries || !tile) {
        return false;
    }

    VkTileCacheEntry *entry = find_entry(cache, kind, coord, band);
    if (entry) {
#if defined(MAPFORGE_HAVE_VK)
        destroy_entry_meshes(cache, vk_renderer, entry);
#endif
        fill_entry_from_tile(entry, kind, coord, band, tile, cache->tick++);
#if defined(MAPFORGE_HAVE_VK)
        if (kind_is_road(kind) || kind_is_polygon(kind)) {
            build_entry_mesh(cache, vk_renderer, entry, tile);
            if (kind_is_polygon(kind)) {
                build_polygon_fill_mesh(cache, vk_renderer, entry, tile);
            }
        }
#endif
        cache->builds += 1u;
        return true;
    }

    entry = pick_slot(cache, kind);
    if (!entry) {
        return false;
    }
    if (entry->occupied) {
#if defined(MAPFORGE_HAVE_VK)
        destroy_entry_meshes(cache, vk_renderer, entry);
#endif
        if (entry->kind < TILE_LAYER_COUNT && cache->resident_by_kind[entry->kind] > 0u) {
            cache->resident_by_kind[entry->kind] -= 1u;
        }
        cache->evictions += 1u;
    } else {
        cache->count += 1u;
    }

    fill_entry_from_tile(entry, kind, coord, band, tile, cache->tick++);
    if (kind < TILE_LAYER_COUNT) {
        cache->resident_by_kind[kind] += 1u;
    }
#if defined(MAPFORGE_HAVE_VK)
    if (kind_is_road(kind) || kind_is_polygon(kind)) {
        build_entry_mesh(cache, vk_renderer, entry, tile);
        if (kind_is_polygon(kind)) {
            build_polygon_fill_mesh(cache, vk_renderer, entry, tile);
        }
    }
#endif
    cache->builds += 1u;
    return true;
}

const VkTileCacheEntry *vk_tile_cache_peek(VkTileCache *cache, TileLayerKind kind, TileCoord coord, TileZoomBand band) {
    VkTileCacheEntry *entry = find_entry(cache, kind, coord, band);
    if (!entry) {
        return NULL;
    }
    entry->last_used = cache->tick++;
    return entry;
}

void vk_tile_cache_get_stats(const VkTileCache *cache, VkTileCacheStats *out_stats) {
    if (!out_stats) {
        return;
    }
    memset(out_stats, 0, sizeof(*out_stats));
    if (!cache) {
        return;
    }
    out_stats->capacity = cache->capacity;
    out_stats->count = cache->count;
    out_stats->builds = cache->builds;
    out_stats->evictions = cache->evictions;
    out_stats->resident_artery = cache->resident_by_kind[TILE_LAYER_ROAD_ARTERY];
    out_stats->resident_local = cache->resident_by_kind[TILE_LAYER_ROAD_LOCAL];
    out_stats->resident_water = cache->resident_by_kind[TILE_LAYER_POLY_WATER];
    out_stats->resident_park = cache->resident_by_kind[TILE_LAYER_POLY_PARK];
    out_stats->resident_landuse = cache->resident_by_kind[TILE_LAYER_POLY_LANDUSE];
    out_stats->resident_building = cache->resident_by_kind[TILE_LAYER_POLY_BUILDING];
    for (uint32_t i = 0; i < cache->capacity; ++i) {
        const VkTileCacheEntry *entry = &cache->entries[i];
        if (!entry->occupied || !entry->fill_mesh_ready) {
            continue;
        }
        switch (entry->kind) {
            case TILE_LAYER_POLY_WATER:
                out_stats->resident_fill_water += 1u;
                break;
            case TILE_LAYER_POLY_PARK:
                out_stats->resident_fill_park += 1u;
                break;
            case TILE_LAYER_POLY_LANDUSE:
                out_stats->resident_fill_landuse += 1u;
                break;
            case TILE_LAYER_POLY_BUILDING:
                out_stats->resident_fill_building += 1u;
                break;
            default:
                break;
        }
    }
    out_stats->mesh_vertices = cache->mesh_vertices;
    out_stats->mesh_bytes = cache->mesh_bytes;
    out_stats->mesh_build_failures = cache->mesh_build_failures;
    out_stats->fill_mesh_build_failures = cache->fill_mesh_build_failures;
}
