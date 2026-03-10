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

static RouteStyle route_style_drive(void) {
    RouteStyle style = {40, 170, 255, 255, 6, 10};
    return style;
}

static RouteStyle route_style_walk(void) {
    RouteStyle style = {60, 200, 130, 230, 5, 8};
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

    SDL_Color outline = {10, 60, 120, 255};
    SDL_Color core = {style.r, style.g, style.b, style.a};

    if (renderer->backend == RENDERER_BACKEND_VULKAN) {
        renderer_set_draw_color(renderer, core.r, core.g, core.b, core.a);
        for (uint32_t i = 0; i + 1 < path->count; ++i) {
            renderer_draw_line(renderer, points[i].x, points[i].y, points[i + 1].x, points[i + 1].y);
        }
        SDL_free(points);
        return;
    }

    int outline_count = 0;
    SDL_Vertex *outline_verts = build_route_vertices(points, (int)path->count, (float)style.outline_width, outline, &outline_count);
    if (outline_verts && outline_count > 0) {
        renderer_draw_geometry(renderer, outline_verts, outline_count, NULL, 0);
        SDL_free(outline_verts);
    }

    int core_count = 0;
    SDL_Vertex *core_verts = build_route_vertices(points, (int)path->count, (float)style.width, core, &core_count);
    if (core_verts && core_count > 0) {
        renderer_draw_geometry(renderer, core_verts, core_count, NULL, 0);
        SDL_free(core_verts);
    }

    SDL_free(points);
}

void route_render_draw(Renderer *renderer, const Camera *camera, const RouteGraph *graph, const RoutePath *path, const RoutePath *drive_path, const RoutePath *walk_path, bool has_start, uint32_t start_node, bool has_goal, uint32_t goal_node, bool has_transfer, uint32_t transfer_node) {
    if (!renderer || !camera || !graph) {
        return;
    }

    if (drive_path && drive_path->count >= 2) {
        draw_route_path(renderer, camera, graph, drive_path, route_style_drive());
    }

    if (walk_path && walk_path->count >= 2) {
        draw_route_path(renderer, camera, graph, walk_path, route_style_walk());
    }

    if ((!drive_path || drive_path->count < 2) && (!walk_path || walk_path->count < 2) && path && path->count >= 2) {
        draw_route_path(renderer, camera, graph, path, route_style_drive());
    }

    if (has_start && start_node < graph->node_count) {
        float sx = 0.0f;
        float sy = 0.0f;
        camera_world_to_screen(camera, (float)graph->node_x[start_node], (float)graph->node_y[start_node], renderer->width, renderer->height, &sx, &sy);
        draw_marker(renderer, sx, sy, 80, 220, 120);
    }

    if (has_goal && goal_node < graph->node_count) {
        float sx = 0.0f;
        float sy = 0.0f;
        camera_world_to_screen(camera, (float)graph->node_x[goal_node], (float)graph->node_y[goal_node], renderer->width, renderer->height, &sx, &sy);
        draw_marker(renderer, sx, sy, 230, 80, 90);
    }

    if (has_transfer && transfer_node < graph->node_count) {
        float sx = 0.0f;
        float sy = 0.0f;
        camera_world_to_screen(camera, (float)graph->node_x[transfer_node], (float)graph->node_y[transfer_node], renderer->width, renderer->height, &sx, &sy);
        draw_marker(renderer, sx, sy, 250, 200, 70);
    }
}
