#include "map/road_renderer.h"

#include "map/layer_policy.h"
#include "map/map_space.h"
#include "map/zoom_fade.h"

#include <SDL.h>
#include <stdbool.h>

typedef struct RoadStyle {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    float width;
} RoadStyle;

#define ROAD_QUAD_MIN_ZOOM 14.5f
#define ROAD_QUAD_MIN_WIDTH 2.5f

static RoadRenderStats g_road_stats;

const char *road_renderer_zoom_tier_label(float zoom) {
    return zoom_tier_label(zoom_tier_for(zoom));
}

void road_renderer_stats_reset(void) {
    SDL_memset(&g_road_stats, 0, sizeof(g_road_stats));
}

void road_renderer_stats_get(RoadRenderStats *out_stats) {
    if (!out_stats) {
        return;
    }
    *out_stats = g_road_stats;
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

static float clamp_width_for_class(float width, RoadClass road_class) {
    float max_width = 4.8f;
    switch (road_class) {
        case ROAD_CLASS_MOTORWAY:
            max_width = 5.2f;
            break;
        case ROAD_CLASS_TRUNK:
        case ROAD_CLASS_PRIMARY:
            max_width = 4.8f;
            break;
        case ROAD_CLASS_SECONDARY:
        case ROAD_CLASS_TERTIARY:
            max_width = 4.2f;
            break;
        case ROAD_CLASS_RESIDENTIAL:
        case ROAD_CLASS_SERVICE:
            max_width = 3.2f;
            break;
        case ROAD_CLASS_FOOTWAY:
        case ROAD_CLASS_PATH:
            max_width = 2.2f;
            break;
        default:
            break;
    }
    if (width > max_width) {
        return max_width;
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

static float road_zoom_alpha_scale(RoadClass road_class, float zoom) {
    switch (road_class) {
        case ROAD_CLASS_SECONDARY:
            if (zoom <= 10.0f) return 0.68f;
            if (zoom <= 11.0f) return 0.80f;
            if (zoom <= 12.0f) return 0.92f;
            return 1.0f;
        case ROAD_CLASS_TERTIARY:
            if (zoom <= 10.0f) return 0.52f;
            if (zoom <= 11.0f) return 0.64f;
            if (zoom <= 12.0f) return 0.76f;
            if (zoom <= 13.0f) return 0.88f;
            return 1.0f;
        case ROAD_CLASS_RESIDENTIAL:
            if (zoom <= 10.0f) return 0.26f;
            if (zoom <= 11.0f) return 0.38f;
            if (zoom <= 12.0f) return 0.52f;
            if (zoom <= 13.0f) return 0.70f;
            return 1.0f;
        case ROAD_CLASS_SERVICE:
            if (zoom <= 10.0f) return 0.18f;
            if (zoom <= 11.0f) return 0.28f;
            if (zoom <= 12.0f) return 0.40f;
            if (zoom <= 13.0f) return 0.58f;
            return 1.0f;
        case ROAD_CLASS_FOOTWAY:
            if (zoom <= 10.0f) return 0.12f;
            if (zoom <= 11.0f) return 0.20f;
            if (zoom <= 12.0f) return 0.30f;
            if (zoom <= 13.0f) return 0.50f;
            return 0.80f;
        case ROAD_CLASS_PATH:
            if (zoom <= 10.0f) return 0.10f;
            if (zoom <= 11.0f) return 0.16f;
            if (zoom <= 12.0f) return 0.24f;
            if (zoom <= 13.0f) return 0.42f;
            return 0.70f;
        default:
            return 1.0f;
    }
}

static float road_zoom_luma_scale(RoadClass road_class, float zoom) {
    switch (road_class) {
        case ROAD_CLASS_SECONDARY:
            if (zoom <= 10.0f) return 0.84f;
            if (zoom <= 11.0f) return 0.90f;
            if (zoom <= 12.0f) return 0.96f;
            return 1.0f;
        case ROAD_CLASS_TERTIARY:
            if (zoom <= 10.0f) return 0.70f;
            if (zoom <= 11.0f) return 0.78f;
            if (zoom <= 12.0f) return 0.86f;
            if (zoom <= 13.0f) return 0.92f;
            return 1.0f;
        case ROAD_CLASS_RESIDENTIAL:
            if (zoom <= 10.0f) return 0.52f;
            if (zoom <= 11.0f) return 0.62f;
            if (zoom <= 12.0f) return 0.74f;
            if (zoom <= 13.0f) return 0.86f;
            return 1.0f;
        case ROAD_CLASS_SERVICE:
            if (zoom <= 10.0f) return 0.42f;
            if (zoom <= 11.0f) return 0.54f;
            if (zoom <= 12.0f) return 0.66f;
            if (zoom <= 13.0f) return 0.80f;
            return 1.0f;
        case ROAD_CLASS_FOOTWAY:
            if (zoom <= 10.0f) return 0.36f;
            if (zoom <= 11.0f) return 0.48f;
            if (zoom <= 12.0f) return 0.60f;
            if (zoom <= 13.0f) return 0.74f;
            return 0.88f;
        case ROAD_CLASS_PATH:
            if (zoom <= 10.0f) return 0.32f;
            if (zoom <= 11.0f) return 0.42f;
            if (zoom <= 12.0f) return 0.54f;
            if (zoom <= 13.0f) return 0.70f;
            return 0.84f;
        default:
            return 1.0f;
    }
}

static RoadStyle road_style_for_class(RoadClass road_class, float zoom, ZoomTier tier) {
    float scale = zoom_width_scale(zoom);
    float major_min = (zoom <= 12.0f) ? 1.2f : 0.0f;

    switch (road_class) {
        case ROAD_CLASS_MOTORWAY:
            return (RoadStyle){230, 160, 50, 255, clamp_width_for_class(SDL_max(clamp_width_for_tier(3.0f * scale, tier), major_min), road_class)};
        case ROAD_CLASS_TRUNK:
            return (RoadStyle){220, 140, 60, 255, clamp_width_for_class(SDL_max(clamp_width_for_tier(2.6f * scale, tier), major_min), road_class)};
        case ROAD_CLASS_PRIMARY:
            return (RoadStyle){210, 120, 70, 255, clamp_width_for_class(SDL_max(clamp_width_for_tier(2.2f * scale, tier), major_min), road_class)};
        case ROAD_CLASS_SECONDARY:
            return (RoadStyle){200, 200, 200, 255, clamp_width_for_class(clamp_width_for_tier(1.8f * scale, tier), road_class)};
        case ROAD_CLASS_TERTIARY:
            return (RoadStyle){180, 180, 180, 255, clamp_width_for_class(clamp_width_for_tier(1.4f * scale, tier), road_class)};
        case ROAD_CLASS_RESIDENTIAL:
            return (RoadStyle){160, 160, 160, 255, clamp_width_for_class(clamp_width_for_tier(1.1f * scale, tier), road_class)};
        case ROAD_CLASS_SERVICE:
            return (RoadStyle){130, 130, 130, 255, clamp_width_for_class(clamp_width_for_tier(0.9f * scale, tier), road_class)};
        case ROAD_CLASS_FOOTWAY:
            return (RoadStyle){110, 110, 110, 200, clamp_width_for_class(clamp_width_for_tier(0.75f * scale, tier), road_class)};
        case ROAD_CLASS_PATH:
            return (RoadStyle){90, 90, 90, 180, clamp_width_for_class(clamp_width_for_tier(0.55f * scale, tier), road_class)};
        default:
            return (RoadStyle){130, 130, 130, 255, clamp_width_for_class(clamp_width_for_tier(0.9f * scale, tier), road_class)};
    }
}

static void draw_polyline(Renderer *renderer, const SDL_FPoint *points, int count, int width, bool single_line) {
    if (count < 2) {
        return;
    }

    renderer_draw_lines(renderer, points, count);

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

        renderer_draw_lines(renderer, shifted, count);

        for (int i = 0; i < count; ++i) {
            shifted[i] = points[i];
            shifted[i].x -= (float)offset;
            shifted[i].y -= (float)offset;
        }

        renderer_draw_lines(renderer, shifted, count);
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

static void draw_quad_strip(Renderer *renderer, const SDL_FPoint *points, int count, float width, SDL_Color color) {
    if (!renderer || count < 2) {
        return;
    }

    int vertex_count = 0;
    SDL_Vertex *verts = build_strip_vertices(points, count, width, color, &vertex_count);
    if (!verts || vertex_count == 0) {
        SDL_free(verts);
        return;
    }

    renderer_draw_geometry(renderer, verts, vertex_count, NULL, 0);
    SDL_free(verts);
}

static bool road_use_quad_strip(const Renderer *renderer, RoadClass road_class, float width, float zoom, bool single_line) {
    if (single_line) {
        return false;
    }
    if (renderer && renderer->backend == RENDERER_BACKEND_VULKAN) {
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

static bool road_class_allowed_under_pressure(const Renderer *renderer, RoadClass road_class) {
    if (!renderer || renderer->backend != RENDERER_BACKEND_VULKAN || renderer->vk_line_budget == 0u) {
        return true;
    }
    if (road_class <= ROAD_CLASS_SERVICE) {
        return true;
    }

    float usage = (float)renderer->vk_lines_drawn / (float)renderer->vk_line_budget;
    if (road_class == ROAD_CLASS_FOOTWAY) {
        return usage < 0.97f;
    }
    if (road_class == ROAD_CLASS_PATH) {
        return usage < 0.90f;
    }
    return usage < 0.95f;
}

static int road_class_index(RoadClass road_class) {
    if (road_class < ROAD_CLASS_MOTORWAY || road_class > ROAD_CLASS_PATH) {
        return -1;
    }
    return (int)road_class;
}

static int road_pass_for_class(RoadClass road_class) {
    switch (road_class) {
        case ROAD_CLASS_MOTORWAY:
            return 0;
        case ROAD_CLASS_TRUNK:
            return 1;
        case ROAD_CLASS_PRIMARY:
            return 2;
        case ROAD_CLASS_SECONDARY:
            return 3;
        case ROAD_CLASS_TERTIARY:
            return 4;
        case ROAD_CLASS_RESIDENTIAL:
            return 5;
        case ROAD_CLASS_SERVICE:
            return 6;
        case ROAD_CLASS_FOOTWAY:
            return 7;
        case ROAD_CLASS_PATH:
        default:
            return 8;
    }
}

void road_renderer_draw_tile(Renderer *renderer,
                             const Camera *camera,
                             const MftTile *tile,
                             bool single_line,
                             float zoom_bias,
                             float opacity_scale) {
    if (!renderer || !camera || !tile || tile->polyline_count == 0) {
        return;
    }
    if (opacity_scale <= 0.0f) {
        return;
    }
    if (opacity_scale > 1.0f) {
        opacity_scale = 1.0f;
    }

    float effective_zoom = camera->zoom - zoom_bias;
    if (effective_zoom < 0.0f) {
        effective_zoom = 0.0f;
    }
    ZoomTier tier = zoom_tier_for(effective_zoom);
    MapTileTransform transform;
    map_tile_transform_init(tile->coord, &transform);

    int pass_count = (renderer->backend == RENDERER_BACKEND_VULKAN) ? 9 : 1;
    for (int pass = 0; pass < pass_count; ++pass) {
        for (uint32_t i = 0; i < tile->polyline_count; ++i) {
            const MftPolyline *polyline = &tile->polylines[i];
            if (polyline->point_count < 2) {
                continue;
            }
            if (renderer->backend == RENDERER_BACKEND_VULKAN &&
                road_pass_for_class(polyline->road_class) != pass) {
                continue;
            }
            if (renderer->backend == RENDERER_BACKEND_VULKAN &&
                !road_class_allowed_under_pressure(renderer, polyline->road_class)) {
                int cls = road_class_index(polyline->road_class);
                if (cls >= 0) {
                    g_road_stats.filtered_by_class[cls] += 1;
                }
                continue;
            }
            uint32_t sample_step = 1;
            if (renderer->backend == RENDERER_BACKEND_VULKAN) {
                sample_step = layer_policy_vk_sample_step(polyline->road_class, effective_zoom);
                if (effective_zoom >= 14.0f && polyline->road_class <= ROAD_CLASS_SERVICE) {
                    sample_step = 1;
                }
            }

            RoadStyle style = road_style_for_class(polyline->road_class, effective_zoom, tier);
            float alpha_scale = road_zoom_alpha_scale(polyline->road_class, effective_zoom);
            float luma_scale = road_zoom_luma_scale(polyline->road_class, effective_zoom);
            float alpha = (float)style.a * alpha_scale * opacity_scale;
            if (alpha > 255.0f) {
                alpha = 255.0f;
            }
            int alpha_i = (int)lroundf(alpha);
            if (alpha_i <= 0) {
                continue;
            }
            int r_i = (int)lroundf((float)style.r * luma_scale);
            int g_i = (int)lroundf((float)style.g * luma_scale);
            int b_i = (int)lroundf((float)style.b * luma_scale);
            if (r_i < 0) r_i = 0;
            if (g_i < 0) g_i = 0;
            if (b_i < 0) b_i = 0;
            if (r_i > 255) r_i = 255;
            if (g_i > 255) g_i = 255;
            if (b_i > 255) b_i = 255;
            style.r = (uint8_t)r_i;
            style.g = (uint8_t)g_i;
            style.b = (uint8_t)b_i;
            style.a = (uint8_t)alpha_i;
            renderer_set_draw_color(renderer, style.r, style.g, style.b, style.a);

            uint32_t sampled_count = 1 + ((polyline->point_count - 1) / sample_step);
            if (((polyline->point_count - 1) % sample_step) != 0) {
                sampled_count += 1;
            }
            if (sampled_count < 2) {
                sampled_count = 2;
            }

            SDL_FPoint stack_points[256];
            SDL_FPoint *points = stack_points;
            bool heap_points = false;

            if (sampled_count > 256) {
                points = (SDL_FPoint *)SDL_malloc(sizeof(SDL_FPoint) * (size_t)sampled_count);
                if (!points) {
                    continue;
                }
                heap_points = true;
            }

            uint32_t out_count = 0;
            float min_point_px = 0.0f;
            if (renderer->backend == RENDERER_BACKEND_VULKAN) {
                min_point_px = layer_policy_vk_min_point_spacing_px(effective_zoom);
                if (effective_zoom >= 14.0f && polyline->road_class <= ROAD_CLASS_SERVICE) {
                    min_point_px = 0.0f;
                }
            }
            float min_point_px2 = min_point_px * min_point_px;
            for (uint32_t p = 0; p < polyline->point_count; p += sample_step) {
                uint32_t idx = (polyline->point_offset + p) * 2;
                float qx = (float)tile->points[idx + 0];
                float qy = (float)tile->points[idx + 1];

                float sx = 0.0f;
                float sy = 0.0f;
                map_tile_local_to_screen(&transform, camera, renderer->width, renderer->height, qx, qy, &sx, &sy);

                bool is_last_sample = (p + sample_step >= polyline->point_count);
                if (out_count > 0 && !is_last_sample && min_point_px2 > 0.0f) {
                    float dx = sx - points[out_count - 1].x;
                    float dy = sy - points[out_count - 1].y;
                    if ((dx * dx + dy * dy) < min_point_px2) {
                        continue;
                    }
                }
                points[out_count].x = sx;
                points[out_count].y = sy;
                out_count += 1;
            }

            uint32_t last_index = polyline->point_count - 1;
            if (last_index % sample_step != 0 && out_count < sampled_count) {
                uint32_t idx = (polyline->point_offset + last_index) * 2;
                float qx = (float)tile->points[idx + 0];
                float qy = (float)tile->points[idx + 1];
                float sx = 0.0f;
                float sy = 0.0f;
                map_tile_local_to_screen(&transform, camera, renderer->width, renderer->height, qx, qy, &sx, &sy);
                points[out_count].x = sx;
                points[out_count].y = sy;
                out_count += 1;
            }
            if (out_count < 2) {
                out_count = 2;
            }

            if (road_use_quad_strip(renderer, polyline->road_class, style.width, camera->zoom, single_line)) {
                SDL_Color color = {style.r, style.g, style.b, style.a};
                draw_quad_strip(renderer, points, (int)out_count, style.width, color);
            } else {
                int width = (int)(style.width + 0.5f);
                if (renderer->backend == RENDERER_BACKEND_VULKAN) {
                    width = 1;
                }
                if (width < 1) {
                    width = 1;
                }
                draw_polyline(renderer, points, (int)out_count, width, single_line);
            }
            int cls = road_class_index(polyline->road_class);
            if (cls >= 0) {
                g_road_stats.drawn_by_class[cls] += 1;
            }

            if (heap_points) {
                SDL_free(points);
            }
        }
    }
}
