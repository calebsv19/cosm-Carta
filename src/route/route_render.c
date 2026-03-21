#include "route/route_render.h"

#include <SDL.h>
#include <stdlib.h>


typedef struct RouteStyle {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    int width;
    int outline_width;
} RouteStyle;

static void draw_route_polyline_vulkan(Renderer *renderer,
                                       const SDL_FPoint *points,
                                       uint32_t count,
                                       int width,
                                       SDL_Color color) {
    if (!renderer || !points || count < 2 || width <= 0) {
        return;
    }

    renderer_set_draw_color(renderer, color.r, color.g, color.b, color.a);
    int radius = (width - 1) / 2;

    for (uint32_t i = 0; i + 1 < count; ++i) {
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
        renderer_draw_line(renderer, a.x, a.y, b.x, b.y);
        for (int step = 1; step <= radius; ++step) {
            float off = (float)step;
            float ox = nx * off;
            float oy = ny * off;
            renderer_draw_line(renderer, a.x + ox, a.y + oy, b.x + ox, b.y + oy);
            renderer_draw_line(renderer, a.x - ox, a.y - oy, b.x - ox, b.y - oy);
        }
    }

    if (radius <= 0) {
        return;
    }

    /* Bridge segment corners so thick offsets do not appear as disjoint rectangles. */
    for (uint32_t i = 1; i + 1 < count; ++i) {
        SDL_FPoint prev = points[i - 1];
        SDL_FPoint cur = points[i];
        SDL_FPoint next = points[i + 1];

        float dx0 = cur.x - prev.x;
        float dy0 = cur.y - prev.y;
        float len0 = SDL_sqrtf(dx0 * dx0 + dy0 * dy0);
        if (len0 <= 0.0001f) {
            continue;
        }
        float nx0 = -dy0 / len0;
        float ny0 = dx0 / len0;

        float dx1 = next.x - cur.x;
        float dy1 = next.y - cur.y;
        float len1 = SDL_sqrtf(dx1 * dx1 + dy1 * dy1);
        if (len1 <= 0.0001f) {
            continue;
        }
        float nx1 = -dy1 / len1;
        float ny1 = dx1 / len1;

        for (int step = 1; step <= radius; ++step) {
            float off = (float)step;
            renderer_draw_line(renderer,
                               cur.x + nx0 * off, cur.y + ny0 * off,
                               cur.x + nx1 * off, cur.y + ny1 * off);
            renderer_draw_line(renderer,
                               cur.x - nx0 * off, cur.y - ny0 * off,
                               cur.x - nx1 * off, cur.y - ny1 * off);
        }
    }
}

static RouteStyle route_style_objective(RouteObjective objective, bool selected) {
    RouteStyle style = {120, 170, 255, 210, 4, 7};
    switch (objective) {
        case ROUTE_OBJECTIVE_SHORTEST_DISTANCE:
            style.r = 40;
            style.g = 220;
            style.b = 255;
            break;
        case ROUTE_OBJECTIVE_LOWEST_TIME:
            style.r = 36;
            style.g = 142;
            style.b = 255;
            break;
        case ROUTE_OBJECTIVE_LOWEST_ELEVATION_GAIN:
            style.r = 92;
            style.g = 255;
            style.b = 150;
            break;
        case ROUTE_OBJECTIVE_MOST_TIME_ABOVE_SPEED_THRESHOLD:
            style.r = 24;
            style.g = 236;
            style.b = 192;
            break;
        default:
            break;
    }

    if (selected) {
        style.a = 210;
        style.width = 5;
        style.outline_width = 7;
    } else {
        style.a = 210;
        style.width = 3;
        style.outline_width = 5;
    }
    return style;
}

static void draw_marker(Renderer *renderer, float x, float y, uint8_t r, uint8_t g, uint8_t b) {
    renderer_set_draw_color(renderer, r, g, b, 255);
    SDL_FRect rect = {x - 4.0f, y - 4.0f, 8.0f, 8.0f};
    renderer_fill_rect(renderer, &rect);
}

static SDL_Vertex *build_route_vertices(const SDL_FPoint *points, int count, float width, SDL_Color color, int *out_vertex_count) {
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

static uint32_t simplify_route_points(SDL_FPoint *points, uint32_t count) {
    if (!points || count < 3u) {
        return count;
    }

    const float min_seg_sq = 0.16f;   /* 0.4 px */
    const float collinear_eps = 0.55f;
    uint32_t write = 1u;

    for (uint32_t i = 1; i + 1 < count; ++i) {
        SDL_FPoint a = points[write - 1u];
        SDL_FPoint b = points[i];
        SDL_FPoint c = points[i + 1u];

        float abx = b.x - a.x;
        float aby = b.y - a.y;
        float bcx = c.x - b.x;
        float bcy = c.y - b.y;
        float ab_len_sq = abx * abx + aby * aby;
        float bc_len_sq = bcx * bcx + bcy * bcy;

        if (ab_len_sq <= min_seg_sq && bc_len_sq <= min_seg_sq) {
            continue;
        }

        float cross = SDL_fabsf(abx * bcy - aby * bcx);
        float ab_len = SDL_sqrtf(ab_len_sq);
        float bc_len = SDL_sqrtf(bc_len_sq);
        if ((ab_len > 0.0f || bc_len > 0.0f) && cross <= collinear_eps * (ab_len + bc_len)) {
            continue;
        }

        points[write++] = b;
    }

    points[write++] = points[count - 1u];
    return write;
}

static void draw_route_path(Renderer *renderer, const Camera *camera, const RouteGraph *graph, const RoutePath *path, RouteStyle style) {
    if (!renderer || !camera || !graph || !path || path->count < 2) {
        return;
    }

    SDL_FPoint *points = (SDL_FPoint *)SDL_malloc(sizeof(SDL_FPoint) * (size_t)path->count);
    if (!points) {
        return;
    }

    for (uint32_t i = 0; i < path->count; ++i) {
        uint32_t node = path->nodes[i];
        float sx = 0.0f;
        float sy = 0.0f;
        camera_world_to_screen(camera, (float)graph->node_x[node], (float)graph->node_y[node], renderer->width, renderer->height, &sx, &sy);
        points[i].x = sx;
        points[i].y = sy;
    }
    uint32_t point_count = simplify_route_points(points, path->count);
    if (point_count < 2u) {
        SDL_free(points);
        return;
    }

    SDL_Color outline = {12, 18, 26, 255};
    SDL_Color core = {style.r, style.g, style.b, style.a};

    if (renderer->backend == RENDERER_BACKEND_VULKAN) {
        draw_route_polyline_vulkan(renderer, points, point_count, style.outline_width, outline);
        draw_route_polyline_vulkan(renderer, points, point_count, style.width, core);
        SDL_free(points);
        return;
    }

    int outline_count = 0;
    SDL_Vertex *outline_verts = build_route_vertices(points, (int)point_count, (float)style.outline_width, outline, &outline_count);
    if (outline_verts && outline_count > 0) {
        renderer_draw_geometry(renderer, outline_verts, outline_count, NULL, 0);
        SDL_free(outline_verts);
    }

    int core_count = 0;
    SDL_Vertex *core_verts = build_route_vertices(points, (int)point_count, (float)style.width, core, &core_count);
    if (core_verts && core_count > 0) {
        renderer_draw_geometry(renderer, core_verts, core_count, NULL, 0);
        SDL_free(core_verts);
    }

    SDL_free(points);
}

static void draw_route_world_segment(Renderer *renderer,
                                     const Camera *camera,
                                     float world_ax,
                                     float world_ay,
                                     float world_bx,
                                     float world_by,
                                     RouteStyle style) {
    if (!renderer || !camera) {
        return;
    }

    float sx0 = 0.0f;
    float sy0 = 0.0f;
    float sx1 = 0.0f;
    float sy1 = 0.0f;
    camera_world_to_screen(camera, world_ax, world_ay, renderer->width, renderer->height, &sx0, &sy0);
    camera_world_to_screen(camera, world_bx, world_by, renderer->width, renderer->height, &sx1, &sy1);
    float dx = sx1 - sx0;
    float dy = sy1 - sy0;
    if ((dx * dx + dy * dy) < 0.25f) {
        return;
    }

    SDL_FPoint points[2];
    points[0].x = sx0;
    points[0].y = sy0;
    points[1].x = sx1;
    points[1].y = sy1;

    SDL_Color outline = {12, 18, 26, 255};
    SDL_Color core = {style.r, style.g, style.b, style.a};

    if (renderer->backend == RENDERER_BACKEND_VULKAN) {
        draw_route_polyline_vulkan(renderer, points, 2u, style.outline_width, outline);
        draw_route_polyline_vulkan(renderer, points, 2u, style.width, core);
        return;
    }

    int outline_count = 0;
    SDL_Vertex *outline_verts = build_route_vertices(points, 2, (float)style.outline_width, outline, &outline_count);
    if (outline_verts && outline_count > 0) {
        renderer_draw_geometry(renderer, outline_verts, outline_count, NULL, 0);
        SDL_free(outline_verts);
    }

    int core_count = 0;
    SDL_Vertex *core_verts = build_route_vertices(points, 2, (float)style.width, core, &core_count);
    if (core_verts && core_count > 0) {
        renderer_draw_geometry(renderer, core_verts, core_count, NULL, 0);
        SDL_free(core_verts);
    }
}

static bool route_path_valid(const RoutePath *path) {
    return path && path->count >= 2;
}

static bool route_alternative_is_selected(const RouteAlternativeSet *alternatives, uint32_t index, RouteObjective selected_objective) {
    if (!alternatives || index >= alternatives->count) {
        return false;
    }
    return alternatives->objectives[index] == selected_objective;
}

static bool draw_alternative_paths(Renderer *renderer,
                                   const Camera *camera,
                                   const RouteGraph *graph,
                                   const RouteAlternativeSet *alternatives,
                                   RouteObjective selected_objective,
                                   const bool *alternative_visible,
                                   bool draw_selected_path) {
    if (!renderer || !camera || !graph || !alternatives || alternatives->count == 0) {
        return false;
    }

    bool drew_selected = false;
    for (uint32_t i = 0; i < alternatives->count; ++i) {
        const RoutePath *candidate = &alternatives->paths[i];
        bool selected = route_alternative_is_selected(alternatives, i, selected_objective);
        if (alternative_visible && !alternative_visible[i]) {
            continue;
        }
        if (!route_path_valid(candidate) || selected) {
            continue;
        }
        draw_route_path(renderer, camera, graph, candidate, route_style_objective(alternatives->objectives[i], false));
    }

    if (!draw_selected_path) {
        return false;
    }

    for (uint32_t i = 0; i < alternatives->count; ++i) {
        const RoutePath *candidate = &alternatives->paths[i];
        bool selected = route_alternative_is_selected(alternatives, i, selected_objective);
        if (alternative_visible && !alternative_visible[i]) {
            continue;
        }
        if (!route_path_valid(candidate) || !selected) {
            continue;
        }
        draw_route_path(renderer, camera, graph, candidate, route_style_objective(alternatives->objectives[i], true));
        drew_selected = true;
        break;
    }

    return drew_selected;
}

static bool route_any_visible_alternative_path(const RouteAlternativeSet *alternatives, const bool *alternative_visible) {
    if (!alternatives || alternatives->count == 0u) {
        return false;
    }
    for (uint32_t i = 0; i < alternatives->count; ++i) {
        if (alternative_visible && !alternative_visible[i]) {
            continue;
        }
        if (route_path_valid(&alternatives->paths[i])) {
            return true;
        }
    }
    return false;
}

void route_render_draw(Renderer *renderer, const Camera *camera, const RouteGraph *graph, const RoutePath *path,
                       const RoutePath *drive_path, const RoutePath *walk_path, const RouteAlternativeSet *alternatives,
                       RouteObjective selected_objective, const bool *alternative_visible,
                       bool has_start, uint32_t start_node, bool has_start_world, float start_world_x, float start_world_y,
                       bool has_goal, uint32_t goal_node, bool has_goal_world, float goal_world_x, float goal_world_y,
                       bool has_transfer, uint32_t transfer_node) {
    if (!renderer || !camera || !graph) {
        return;
    }

    bool has_split_paths = route_path_valid(drive_path) || route_path_valid(walk_path);
    bool selected_visible = true;
    if (alternatives && alternative_visible) {
        for (uint32_t i = 0; i < alternatives->count; ++i) {
            if (alternatives->objectives[i] == selected_objective) {
                selected_visible = alternative_visible[i];
                break;
            }
        }
    }
    bool selected_drawn = draw_alternative_paths(renderer, camera, graph, alternatives, selected_objective, alternative_visible, !has_split_paths);

    if (selected_visible && route_path_valid(drive_path)) {
        draw_route_path(renderer, camera, graph, drive_path, route_style_objective(selected_objective, true));
    }

    if (selected_visible && route_path_valid(walk_path)) {
        draw_route_path(renderer, camera, graph, walk_path, route_style_objective(selected_objective, true));
    }

    bool has_alternatives = alternatives && alternatives->count > 0u;
    if (!has_split_paths && !selected_drawn && !has_alternatives && route_path_valid(path)) {
        draw_route_path(renderer, camera, graph, path, route_style_objective(selected_objective, true));
    }

    bool has_renderable_route = selected_drawn ||
                                has_split_paths ||
                                route_path_valid(path) ||
                                route_any_visible_alternative_path(alternatives, alternative_visible);
    if (selected_visible && has_renderable_route) {
        RouteStyle endpoint_style = route_style_objective(selected_objective, true);
        if (has_start && has_start_world && start_node < graph->node_count) {
            draw_route_world_segment(renderer, camera,
                                     start_world_x, start_world_y,
                                     (float)graph->node_x[start_node], (float)graph->node_y[start_node],
                                     endpoint_style);
        }
        if (has_goal && has_goal_world && goal_node < graph->node_count) {
            draw_route_world_segment(renderer, camera,
                                     (float)graph->node_x[goal_node], (float)graph->node_y[goal_node],
                                     goal_world_x, goal_world_y,
                                     endpoint_style);
        }
    }

    if (has_start && start_node < graph->node_count) {
        float sx = 0.0f;
        float sy = 0.0f;
        if (has_start_world) {
            camera_world_to_screen(camera, start_world_x, start_world_y, renderer->width, renderer->height, &sx, &sy);
        } else {
            camera_world_to_screen(camera, (float)graph->node_x[start_node], (float)graph->node_y[start_node], renderer->width, renderer->height, &sx, &sy);
        }
        draw_marker(renderer, sx, sy, 80, 220, 120);
    }

    if (has_goal && goal_node < graph->node_count) {
        float sx = 0.0f;
        float sy = 0.0f;
        if (has_goal_world) {
            camera_world_to_screen(camera, goal_world_x, goal_world_y, renderer->width, renderer->height, &sx, &sy);
        } else {
            camera_world_to_screen(camera, (float)graph->node_x[goal_node], (float)graph->node_y[goal_node], renderer->width, renderer->height, &sx, &sy);
        }
        draw_marker(renderer, sx, sy, 230, 80, 90);
    }

    if (has_transfer && transfer_node < graph->node_count) {
        float sx = 0.0f;
        float sy = 0.0f;
        camera_world_to_screen(camera, (float)graph->node_x[transfer_node], (float)graph->node_y[transfer_node], renderer->width, renderer->height, &sx, &sy);
        draw_marker(renderer, sx, sy, 250, 200, 70);
    }
}
