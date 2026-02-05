#include "map/road_renderer.h"

#include "map/tile_math.h"
#include "map/zoom_fade.h"

#include <SDL.h>
#include <stdbool.h>

#define TILE_EXTENT 4096.0f

typedef struct RoadStyle {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    float width;
} RoadStyle;

#define ROAD_QUAD_MIN_ZOOM 14.5f
#define ROAD_QUAD_MIN_WIDTH 2.5f

const char *road_renderer_zoom_tier_label(float zoom) {
    return zoom_tier_label(zoom_tier_for(zoom));
}

static ZoomTier road_class_min_tier(RoadClass road_class) {
    switch (road_class) {
        case ROAD_CLASS_MOTORWAY:
        case ROAD_CLASS_TRUNK:
            return ZOOM_TIER_FAR;
        case ROAD_CLASS_PRIMARY:
        case ROAD_CLASS_SECONDARY:
        case ROAD_CLASS_TERTIARY:
            return ZOOM_TIER_MID;
        case ROAD_CLASS_RESIDENTIAL:
            return ZOOM_TIER_NEAR;
        case ROAD_CLASS_SERVICE:
        case ROAD_CLASS_FOOTWAY:
            return ZOOM_TIER_CLOSE;
        case ROAD_CLASS_PATH:
        default:
            return ZOOM_TIER_PATH;
    }
}

static float clamp_width_for_tier(float width, ZoomTier tier) {
    if (tier == ZOOM_TIER_FAR && width > 2.0f) {
        return 2.0f;
    }
    if (tier == ZOOM_TIER_MID && width > 3.0f) {
        return 3.0f;
    }
    return width;
}

static float zoom_width_scale(float zoom) {
    if (zoom <= 10.0f) {
        return 0.35f;
    }
    if (zoom <= 12.0f) {
        return 0.6f;
    }
    if (zoom <= 14.0f) {
        return 1.0f;
    }
    if (zoom <= 16.0f) {
        return 1.4f;
    }
    return 1.8f;
}

static RoadStyle road_style_for_class(RoadClass road_class, float zoom, ZoomTier tier) {
    float scale = zoom_width_scale(zoom);

    switch (road_class) {
        case ROAD_CLASS_MOTORWAY:
            return (RoadStyle){230, 160, 50, 255, clamp_width_for_tier(3.0f * scale, tier)};
        case ROAD_CLASS_TRUNK:
            return (RoadStyle){220, 140, 60, 255, clamp_width_for_tier(2.6f * scale, tier)};
        case ROAD_CLASS_PRIMARY:
            return (RoadStyle){210, 120, 70, 255, clamp_width_for_tier(2.2f * scale, tier)};
        case ROAD_CLASS_SECONDARY:
            return (RoadStyle){200, 200, 200, 255, clamp_width_for_tier(1.8f * scale, tier)};
        case ROAD_CLASS_TERTIARY:
            return (RoadStyle){180, 180, 180, 255, clamp_width_for_tier(1.4f * scale, tier)};
        case ROAD_CLASS_RESIDENTIAL:
            return (RoadStyle){160, 160, 160, 255, clamp_width_for_tier(1.1f * scale, tier)};
        case ROAD_CLASS_SERVICE:
            return (RoadStyle){130, 130, 130, 255, clamp_width_for_tier(0.9f * scale, tier)};
        case ROAD_CLASS_FOOTWAY:
            return (RoadStyle){110, 110, 110, 200, clamp_width_for_tier(0.75f * scale, tier)};
        case ROAD_CLASS_PATH:
            return (RoadStyle){90, 90, 90, 180, clamp_width_for_tier(0.55f * scale, tier)};
        default:
            return (RoadStyle){130, 130, 130, 255, clamp_width_for_tier(0.9f * scale, tier)};
    }
}

static void draw_polyline(SDL_Renderer *sdl, const SDL_FPoint *points, int count, int width, bool single_line) {
    if (count < 2) {
        return;
    }

    SDL_RenderDrawLinesF(sdl, points, count);

    if (width <= 1 || single_line) {
        return;
    }

    for (int offset = 1; offset <= width / 2; ++offset) {
        SDL_FPoint *shifted = (SDL_FPoint *)SDL_malloc(sizeof(SDL_FPoint) * (size_t)count);
        if (!shifted) {
            return;
        }

        for (int i = 0; i < count; ++i) {
            shifted[i] = points[i];
            shifted[i].x += (float)offset;
            shifted[i].y += (float)offset;
        }

        SDL_RenderDrawLinesF(sdl, shifted, count);

        for (int i = 0; i < count; ++i) {
            shifted[i] = points[i];
            shifted[i].x -= (float)offset;
            shifted[i].y -= (float)offset;
        }

        SDL_RenderDrawLinesF(sdl, shifted, count);
        SDL_free(shifted);
    }
}

static SDL_Vertex *build_strip_vertices(const SDL_FPoint *points, int count, float width, SDL_Color color, int *out_vertex_count) {
    if (!points || count < 2 || width <= 0.0f || !out_vertex_count) {
        return NULL;
    }

    int quad_count = count - 1;
    int vertex_count = quad_count * 6;
    SDL_Vertex *verts = (SDL_Vertex *)SDL_malloc(sizeof(SDL_Vertex) * (size_t)vertex_count);
    if (!verts) {
        return NULL;
    }

    float half = width * 0.5f;
    int v = 0;
    for (int i = 0; i < quad_count; ++i) {
        SDL_FPoint a = points[i];
        SDL_FPoint b = points[i + 1];
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float len = SDL_sqrtf(dx * dx + dy * dy);
        if (len <= 0.0001f) {
            continue;
        }

        float nx = -dy / len;
        float ny = dx / len;

        SDL_FPoint a0 = {a.x + nx * half, a.y + ny * half};
        SDL_FPoint a1 = {a.x - nx * half, a.y - ny * half};
        SDL_FPoint b0 = {b.x + nx * half, b.y + ny * half};
        SDL_FPoint b1 = {b.x - nx * half, b.y - ny * half};

        verts[v++] = (SDL_Vertex){a0, color, {0.0f, 0.0f}};
        verts[v++] = (SDL_Vertex){b0, color, {0.0f, 0.0f}};
        verts[v++] = (SDL_Vertex){b1, color, {0.0f, 0.0f}};
        verts[v++] = (SDL_Vertex){a0, color, {0.0f, 0.0f}};
        verts[v++] = (SDL_Vertex){b1, color, {0.0f, 0.0f}};
        verts[v++] = (SDL_Vertex){a1, color, {0.0f, 0.0f}};
    }

    *out_vertex_count = v;
    return verts;
}

static void draw_quad_strip(SDL_Renderer *sdl, const SDL_FPoint *points, int count, float width, SDL_Color color) {
    if (!sdl || count < 2) {
        return;
    }

    int vertex_count = 0;
    SDL_Vertex *verts = build_strip_vertices(points, count, width, color, &vertex_count);
    if (!verts || vertex_count == 0) {
        SDL_free(verts);
        return;
    }

    SDL_RenderGeometry(sdl, NULL, verts, vertex_count, NULL, 0);
    SDL_free(verts);
}

static bool road_use_quad_strip(RoadClass road_class, float width, float zoom, bool single_line) {
    if (single_line) {
        return false;
    }
    if (road_class == ROAD_CLASS_FOOTWAY || road_class == ROAD_CLASS_PATH) {
        return false;
    }
    if (zoom < ROAD_QUAD_MIN_ZOOM) {
        return false;
    }
    return width >= ROAD_QUAD_MIN_WIDTH;
}

void road_renderer_draw_tile(Renderer *renderer, const Camera *camera, const MftTile *tile, bool single_line) {
    if (!renderer || !renderer->sdl || !camera || !tile || tile->polyline_count == 0) {
        return;
    }

    ZoomTier tier = zoom_tier_for(camera->zoom);
    double tile_size = tile_size_meters(tile->coord.z);
    MercatorMeters origin = tile_origin_meters(tile->coord);

    for (uint32_t i = 0; i < tile->polyline_count; ++i) {
        const MftPolyline *polyline = &tile->polylines[i];
        if (polyline->point_count < 2) {
            continue;
        }

        ZoomTier min_tier = road_class_min_tier(polyline->road_class);
        RoadStyle style = road_style_for_class(polyline->road_class, camera->zoom, tier);
        float fade = zoom_tier_fade_in_alpha(camera->zoom, min_tier);
        float alpha = (float)style.a * fade;
        if (alpha > 255.0f) {
            alpha = 255.0f;
        }
        int alpha_i = (int)lroundf(alpha);
        if (alpha_i <= 0) {
            continue;
        }
        style.a = (uint8_t)alpha_i;
        SDL_SetRenderDrawColor(renderer->sdl, style.r, style.g, style.b, style.a);

        SDL_FPoint stack_points[256];
        SDL_FPoint *points = stack_points;
        bool heap_points = false;

        if (polyline->point_count > 256) {
            points = (SDL_FPoint *)SDL_malloc(sizeof(SDL_FPoint) * (size_t)polyline->point_count);
            if (!points) {
                continue;
            }
            heap_points = true;
        }

        for (uint32_t p = 0; p < polyline->point_count; ++p) {
            uint32_t idx = (polyline->point_offset + p) * 2;
            float qx = (float)tile->points[idx + 0];
            float qy = (float)tile->points[idx + 1];

            float ux = qx / TILE_EXTENT;
            float uy = qy / TILE_EXTENT;

            float world_x = (float)(origin.x + ux * tile_size);
            float world_y = (float)(origin.y - uy * tile_size);

            float sx = 0.0f;
            float sy = 0.0f;
            camera_world_to_screen(camera, world_x, world_y, renderer->width, renderer->height, &sx, &sy);

            points[p].x = sx;
            points[p].y = sy;
        }

        if (road_use_quad_strip(polyline->road_class, style.width, camera->zoom, single_line)) {
            SDL_Color color = {style.r, style.g, style.b, style.a};
            draw_quad_strip(renderer->sdl, points, (int)polyline->point_count, style.width, color);
        } else {
            int width = (int)(style.width + 0.5f);
            if (width < 1) {
                width = 1;
            }
            draw_polyline(renderer->sdl, points, (int)polyline->point_count, width, single_line);
        }

        if (heap_points) {
            SDL_free(points);
        }
    }
}
