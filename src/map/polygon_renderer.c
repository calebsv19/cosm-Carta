#include "map/polygon_renderer.h"

#include "map/polygon_triangulator.h"
#include "map/tile_math.h"
#include "map/zoom_fade.h"

#include <SDL.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>

#define TILE_EXTENT 4096.0f
#define POLYGON_MAX_FILL_POINTS 512u
#define POLYGON_MAX_OUTLINE_POINTS 2048u

typedef struct PolygonStyle {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    bool outline;
} PolygonStyle;

static ZoomTier polygon_class_min_tier(PolygonClass polygon_class) {
    switch (polygon_class) {
        case POLYGON_CLASS_WATER:
            return ZOOM_TIER_MID;
        case POLYGON_CLASS_PARK:
        case POLYGON_CLASS_LANDUSE:
            return ZOOM_TIER_NEAR;
        case POLYGON_CLASS_BUILDING:
        default:
            return ZOOM_TIER_PATH;
    }
}

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
            return (PolygonStyle){170, 170, 170, 210, true};
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

static void draw_polygon_outline(SDL_Renderer *sdl, const SDL_FPoint *points, int count) {
    if (!sdl || !points || count < 2) {
        return;
    }

    SDL_RenderDrawLinesF(sdl, points, count);
    SDL_RenderDrawLineF(sdl, points[count - 1].x, points[count - 1].y, points[0].x, points[0].y);
}

static bool polygon_cache_init(MftTile *tile) {
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

            if (ring_count >= 3 && ring_count <= POLYGON_MAX_FILL_POINTS) {
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

static bool draw_polygon_fill_cached(SDL_Renderer *sdl,
                                     const SDL_FPoint *points,
                                     int count,
                                     SDL_Color color,
                                     const uint32_t *indices,
                                     uint32_t index_count) {
    if (!sdl || !points || count < 3 || !indices || index_count < 3) {
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

    SDL_RenderGeometry(sdl, NULL, verts, count, (const int *)indices, (int)index_count);
    SDL_free(verts);
    return true;
}
void polygon_renderer_draw_tile(Renderer *renderer,
                                const Camera *camera,
                                MftTile *tile,
                                bool show_landuse,
                                float building_zoom_bias,
                                bool building_fill_enabled,
                                bool polygon_outline_only) {
    if (!renderer || !renderer->sdl || !camera || !tile || tile->polygon_count == 0) {
        return;
    }

    double tile_size = tile_size_meters(tile->coord.z);
    MercatorMeters origin = tile_origin_meters(tile->coord);

    if (!tile->polygon_tri_cached) {
        polygon_cache_init(tile);
    }

    for (uint32_t i = 0; i < tile->polygon_count; ++i) {
        const MftPolygon *polygon = &tile->polygons[i];
        if (polygon->ring_count == 0) {
            continue;
        }

        ZoomTier min_tier = polygon_class_min_tier(polygon->polygon_class);
        float fade = zoom_tier_fade_in_alpha(camera->zoom, min_tier);
        if (polygon->polygon_class == POLYGON_CLASS_BUILDING) {
            float start = zoom_tier_min_zoom(ZOOM_TIER_PATH) + 0.8f + building_zoom_bias;
            float end = start + 0.6f;
            float building_fade = (camera->zoom - start) / (end - start);
            if (building_fade < 0.0f) {
                building_fade = 0.0f;
            } else if (building_fade > 1.0f) {
                building_fade = 1.0f;
            }
            fade *= building_fade;
        }
        if (fade <= 0.0f) {
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
        int alpha = (int)SDL_roundf((float)style.a * fade);
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

                float ux = qx / TILE_EXTENT;
                float uy = qy / TILE_EXTENT;

                float world_x = (float)(origin.x + ux * tile_size);
                float world_y = (float)(origin.y - uy * tile_size);

                float sx = 0.0f;
                float sy = 0.0f;
                camera_world_to_screen(camera, world_x, world_y, renderer->width, renderer->height, &sx, &sy);

                points[p] = (SDL_FPoint){sx, sy};
            }

        bool draw_fill = convex || polygon->polygon_class != POLYGON_CLASS_LANDUSE;
        if (polygon_outline_only) {
            draw_fill = false;
        }
        if (polygon->polygon_class == POLYGON_CLASS_BUILDING && !building_fill_enabled) {
            draw_fill = false;
        }
            if (ring_count > POLYGON_MAX_FILL_POINTS) {
                draw_fill = false;
            }
            if (draw_fill) {
                const uint32_t *indices = NULL;
                uint32_t index_count = 0;
                if (!polygon_get_cached_indices(tile, ring_index, &indices, &index_count) ||
                    !draw_polygon_fill_cached(renderer->sdl, points, (int)ring_count, fill, indices, index_count)) {
                    draw_fill = false;
                }
            }

            if (style.outline) {
                SDL_Color stroke = {style.r, style.g, style.b, (uint8_t)SDL_max(alpha - 40, 40)};
                SDL_SetRenderDrawColor(renderer->sdl, stroke.r, stroke.g, stroke.b, stroke.a);
                draw_polygon_outline(renderer->sdl, points, (int)ring_count);
            } else if (!draw_fill) {
                SDL_Color stroke = {style.r, style.g, style.b, (uint8_t)SDL_max(alpha - 40, 40)};
                SDL_SetRenderDrawColor(renderer->sdl, stroke.r, stroke.g, stroke.b, stroke.a);
                draw_polygon_outline(renderer->sdl, points, (int)ring_count);
            }

            SDL_free(points);
            point_offset += ring_count;
        }
    }
}
