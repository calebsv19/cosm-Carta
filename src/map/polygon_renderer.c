#include "map/polygon_renderer.h"

#include "map/polygon_cache.h"
#include "map/map_space.h"

#include <SDL.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>

#define POLYGON_MAX_OUTLINE_POINTS 2048u
#define POLYGON_TILE_EXTENT_Q 4096u

typedef struct PolygonStyle {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    bool outline;
} PolygonStyle;

static PolygonStyle polygon_style_for_class(PolygonClass polygon_class) {
    switch (polygon_class) {
        case POLYGON_CLASS_WATER:
            return (PolygonStyle){50, 80, 120, 210, true};
        case POLYGON_CLASS_PARK:
            return (PolygonStyle){70, 100, 70, 200, false};
        case POLYGON_CLASS_LANDUSE:
            return (PolygonStyle){170, 180, 150, 160, false};
        case POLYGON_CLASS_BUILDING:
        default:
            return (PolygonStyle){138, 138, 138, 190, true};
    }
}

// Returns true if the polygon ring is convex (in tile-local coordinates).
static bool polygon_ring_is_convex(const uint16_t *points, uint32_t count) {
    if (!points || count < 4) {
        return true;
    }

    int sign = 0;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t i0 = i;
        uint32_t i1 = (i + 1) % count;
        uint32_t i2 = (i + 2) % count;

        float x0 = (float)points[i0 * 2];
        float y0 = (float)points[i0 * 2 + 1];
        float x1 = (float)points[i1 * 2];
        float y1 = (float)points[i1 * 2 + 1];
        float x2 = (float)points[i2 * 2];
        float y2 = (float)points[i2 * 2 + 1];

        float dx1 = x1 - x0;
        float dy1 = y1 - y0;
        float dx2 = x2 - x1;
        float dy2 = y2 - y1;
        float cross = dx1 * dy2 - dy1 * dx2;
        if (fabsf(cross) < 0.001f) {
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

static void draw_polygon_outline(Renderer *renderer, const SDL_FPoint *points, int count) {
    if (!renderer || !points || count < 2) {
        return;
    }

    renderer_draw_lines(renderer, points, count);
    renderer_draw_line(renderer, points[count - 1].x, points[count - 1].y, points[0].x, points[0].y);
}

static bool polygon_edge_on_tile_boundary(const uint16_t *ring_points, uint32_t count, uint32_t index) {
    if (!ring_points || count < 2u) {
        return false;
    }
    uint32_t next = (index + 1u) % count;
    uint16_t x0 = ring_points[index * 2u];
    uint16_t y0 = ring_points[index * 2u + 1u];
    uint16_t x1 = ring_points[next * 2u];
    uint16_t y1 = ring_points[next * 2u + 1u];
    return (x0 == 0u && x1 == 0u) ||
           (x0 == POLYGON_TILE_EXTENT_Q && x1 == POLYGON_TILE_EXTENT_Q) ||
           (y0 == 0u && y1 == 0u) ||
           (y0 == POLYGON_TILE_EXTENT_Q && y1 == POLYGON_TILE_EXTENT_Q);
}

static void draw_polygon_outline_filtered(Renderer *renderer,
                                          const SDL_FPoint *points,
                                          const uint16_t *ring_points,
                                          int count,
                                          bool suppress_tile_boundary_edges) {
    if (!renderer || !points || count < 2) {
        return;
    }
    if (!suppress_tile_boundary_edges || !ring_points) {
        draw_polygon_outline(renderer, points, count);
        return;
    }

    for (int i = 0; i < count; ++i) {
        int next = (i + 1) % count;
        if (polygon_edge_on_tile_boundary(ring_points, (uint32_t)count, (uint32_t)i)) {
            continue;
        }
        renderer_draw_line(renderer, points[i].x, points[i].y, points[next].x, points[next].y);
    }
}

static bool polygon_allow_under_vk_pressure(const Renderer *renderer, PolygonClass polygon_class, float zoom) {
    if (!renderer || renderer->backend != RENDERER_BACKEND_VULKAN || renderer->vk_line_budget == 0u) {
        return true;
    }

    float usage = (float)renderer->vk_lines_drawn / (float)renderer->vk_line_budget;
    if (polygon_class == POLYGON_CLASS_BUILDING) {
        if (zoom < 13.8f) {
            return usage < 0.55f;
        }
        return usage < 0.75f;
    }
    if (polygon_class == POLYGON_CLASS_LANDUSE) {
        return usage < 0.82f;
    }
    if (polygon_class == POLYGON_CLASS_PARK) {
        return usage < 0.90f;
    }
    if (polygon_class == POLYGON_CLASS_WATER) {
        return usage < 0.94f;
    }
    return true;
}

static int polygon_compact_outline_points(SDL_FPoint *points, int count, int step) {
    if (!points || count < 3 || step <= 1) {
        return count;
    }

    int write = 1;
    for (int i = step; i < count - 1; i += step) {
        points[write++] = points[i];
    }
    points[write++] = points[count - 1];
    if (write < 3) {
        // Guarantee a valid minimal ring from existing vertices.
        points[1] = points[count / 2];
        points[2] = points[count - 1];
        return 3;
    }
    return write;
}

static int polygon_vk_outline_step(PolygonClass polygon_class, float zoom) {
    if (polygon_class == POLYGON_CLASS_BUILDING) {
        (void)zoom;
        // Preserve building footprint fidelity in fallback polygon path.
        return 1;
    }
    if (polygon_class == POLYGON_CLASS_LANDUSE) {
        return zoom < 14.5f ? 4 : 3;
    }
    if (polygon_class == POLYGON_CLASS_PARK) {
        return zoom < 14.0f ? 3 : 2;
    }
    if (polygon_class == POLYGON_CLASS_WATER) {
        return zoom < 13.5f ? 3 : 2;
    }
    return 1;
}

static bool polygon_get_cached_indices(const MftTile *tile,
                                       uint32_t ring_index,
                                       const uint32_t **out_indices,
                                       uint32_t *out_count) {
    if (!tile || !tile->polygon_tri_cached || !out_indices || !out_count) {
        return false;
    }
    if (ring_index >= tile->polygon_ring_total) {
        return false;
    }
    uint32_t offset = tile->polygon_tri_ring_offsets[ring_index];
    uint32_t count = tile->polygon_tri_ring_counts[ring_index];
    if (count == 0 || !tile->polygon_tri_indices) {
        return false;
    }
    *out_indices = tile->polygon_tri_indices + offset;
    *out_count = count;
    return true;
}

static bool draw_polygon_fill_cached(Renderer *renderer,
                                     const SDL_FPoint *points,
                                     int count,
                                     SDL_Color color,
                                     const uint32_t *indices,
                                     uint32_t index_count) {
    if (!renderer || !points || count < 3 || !indices || index_count < 3) {
        return false;
    }

    SDL_Vertex *verts = (SDL_Vertex *)SDL_malloc(sizeof(SDL_Vertex) * (size_t)count);
    if (!verts) {
        return false;
    }

    for (int i = 0; i < count; ++i) {
        verts[i].position = points[i];
        verts[i].color = color;
        verts[i].tex_coord = (SDL_FPoint){0.0f, 0.0f};
    }

    renderer_draw_geometry(renderer, verts, count, (const int *)indices, (int)index_count);
    SDL_free(verts);
    return true;
}
void polygon_renderer_draw_tile(Renderer *renderer,
                                const Camera *camera,
                                MftTile *tile,
                                bool show_landuse,
                                float building_zoom_bias,
                                bool building_fill_enabled,
                                bool polygon_outline_only,
                                float opacity_scale) {
    if (!renderer || !camera || !tile || tile->polygon_count == 0) {
        return;
    }
    if (opacity_scale <= 0.0f) {
        return;
    }
    if (opacity_scale > 1.0f) {
        opacity_scale = 1.0f;
    }
    (void)building_zoom_bias;

    MapTileTransform transform;
    map_tile_transform_init(tile->coord, &transform);

    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const MftPolygon *polygon = &tile->polygons[i];
        if (polygon->ring_count == 0) {
            continue;
        }

        if (!polygon_allow_under_vk_pressure(renderer, polygon->polygon_class, camera->zoom)) {
            continue;
        }

        if (polygon->polygon_class == POLYGON_CLASS_LANDUSE && !show_landuse) {
            continue;
        }

        PolygonStyle style = polygon_style_for_class(polygon->polygon_class);
        if (polygon_outline_only) {
            style.outline = true;
        }
        if (polygon->polygon_class == POLYGON_CLASS_LANDUSE) {
            style.outline = true;
        }
        int alpha = (int)SDL_roundf((float)style.a * opacity_scale);
        if (alpha <= 0) {
            continue;
        }
        if (alpha > 255) {
            alpha = 255;
        }

        SDL_Color fill = {style.r, style.g, style.b, (uint8_t)alpha};

        uint32_t point_offset = polygon->point_offset;
        for (uint16_t r = 0; r < polygon->ring_count; ++r) {
            uint32_t ring_index = polygon->ring_offset + r;
            uint32_t ring_count = tile->polygon_rings[polygon->ring_offset + r];
            if (ring_count < 3) {
                point_offset += ring_count;
                continue;
            }

            if (ring_count > POLYGON_MAX_OUTLINE_POINTS) {
                point_offset += ring_count;
                continue;
            }

            SDL_FPoint *points = (SDL_FPoint *)SDL_malloc(sizeof(SDL_FPoint) * (size_t)ring_count);
            if (!points) {
                break;
            }

            const uint16_t *ring_points = &tile->polygon_points[point_offset * 2];
            bool convex = polygon_ring_is_convex(ring_points, ring_count);
            for (uint32_t p = 0; p < ring_count; ++p) {
                float qx = (float)ring_points[p * 2];
                float qy = (float)ring_points[p * 2 + 1];

                float sx = 0.0f;
                float sy = 0.0f;
                map_tile_local_to_screen(&transform, camera, renderer->width, renderer->height, qx, qy, &sx, &sy);

                points[p] = (SDL_FPoint){sx, sy};
            }

        bool draw_fill = convex || polygon->polygon_class != POLYGON_CLASS_LANDUSE;
        if (renderer->backend == RENDERER_BACKEND_VULKAN) {
            // Vulkan backend currently keeps polygon rendering in outline mode to avoid
            // high triangle throughput stalls on dense regions.
            draw_fill = false;
        }
        if (polygon_outline_only) {
            draw_fill = false;
        }
        if (polygon->polygon_class == POLYGON_CLASS_BUILDING && !building_fill_enabled) {
            draw_fill = false;
        }
            if (ring_count > POLYGON_CACHE_MAX_FILL_POINTS) {
                draw_fill = false;
            }
            if (draw_fill) {
                const uint32_t *indices = NULL;
                uint32_t index_count = 0;
                if (!polygon_get_cached_indices(tile, ring_index, &indices, &index_count) ||
                    !draw_polygon_fill_cached(renderer, points, (int)ring_count, fill, indices, index_count)) {
                    draw_fill = false;
                }
            }

            int outline_count = (int)ring_count;
            if (renderer->backend == RENDERER_BACKEND_VULKAN) {
                int outline_step = polygon_vk_outline_step(polygon->polygon_class, camera->zoom);
                outline_count = polygon_compact_outline_points(points, outline_count, outline_step);
            }

            bool suppress_tile_edges = true;
            if (style.outline) {
                SDL_Color stroke = {style.r, style.g, style.b, (uint8_t)SDL_max(alpha - 40, 40)};
                renderer_set_draw_color(renderer, stroke.r, stroke.g, stroke.b, stroke.a);
                draw_polygon_outline_filtered(renderer, points, ring_points, outline_count, suppress_tile_edges);
            } else if (!draw_fill) {
                SDL_Color stroke = {style.r, style.g, style.b, (uint8_t)SDL_max(alpha - 40, 40)};
                renderer_set_draw_color(renderer, stroke.r, stroke.g, stroke.b, stroke.a);
                draw_polygon_outline_filtered(renderer, points, ring_points, outline_count, suppress_tile_edges);
            }

            SDL_free(points);
            point_offset += ring_count;
        }
    }
}
