#include "app/app_internal.h"

#include "core/log.h"

#include <stdio.h>

bool app_load_route_graph(AppState *app) {
    if (!app) {
        return false;
    }

    char path[512];
    snprintf(path, sizeof(path), "data/regions/%s/graph/graph.bin", app->region.name);
    if (!route_state_load_graph(&app->route, path)) {
        log_error("Missing route graph for region: %s", app->region.name);
        return false;
    }
    return true;
}

static bool app_find_nearest_node(const RouteGraph *graph, float world_x, float world_y, uint32_t *out_node, double *out_dist) {
    if (!graph || graph->node_count == 0 || !out_node || !out_dist) {
        return false;
    }

    uint32_t best = 0;
    double best_dist = 0.0;
    for (uint32_t i = 0; i < graph->node_count; ++i) {
        double dx = graph->node_x[i] - world_x;
        double dy = graph->node_y[i] - world_y;
        double dist = dx * dx + dy * dy;
        if (i == 0 || dist < best_dist) {
            best_dist = dist;
            best = i;
        }
    }

    *out_node = best;
    *out_dist = best_dist;
    return true;
}

bool app_is_near_node(const AppState *app, float world_x, float world_y, uint32_t *out_node) {
    if (!app || !app->route.loaded || app->route.graph.node_count == 0 || !out_node) {
        return false;
    }

    uint32_t node = 0;
    double dist = 0.0;
    if (!app_find_nearest_node(&app->route.graph, world_x, world_y, &node, &dist)) {
        return false;
    }

    float ppm = camera_pixels_per_meter(&app->camera);
    if (ppm <= 0.0f) {
        return false;
    }

    float snap_radius_m = 12.0f / ppm;
    if (dist > (double)(snap_radius_m * snap_radius_m)) {
        return false;
    }

    *out_node = node;
    return true;
}

void app_update_hover(AppState *app) {
    if (!app || !app->route.loaded) {
        if (app) {
            app->has_hover = false;
        }
        return;
    }

    float world_x = 0.0f;
    float world_y = 0.0f;
    camera_screen_to_world(&app->camera, (float)app->input.mouse_x, (float)app->input.mouse_y, app->width, app->height, &world_x, &world_y);

    uint32_t node = 0;
    if (app_is_near_node(app, world_x, world_y, &node)) {
        app->hover_node = node;
        app->has_hover = true;
    } else {
        app->has_hover = false;
    }
}

bool app_mouse_over_node(const AppState *app, uint32_t node, float radius) {
    if (!app || !app->route.loaded || node >= app->route.graph.node_count) {
        return false;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->camera,
                           (float)app->route.graph.node_x[node],
                           (float)app->route.graph.node_y[node],
                           app->width, app->height, &sx, &sy);
    float dx = (float)app->input.mouse_x - sx;
    float dy = (float)app->input.mouse_y - sy;
    return (dx * dx + dy * dy) <= radius * radius;
}

void app_draw_hover_marker(AppState *app) {
    if (!app || !app->has_hover || !app->route.loaded || app->hover_node >= app->route.graph.node_count) {
        return;
    }

    if ((app->route.has_start && app->hover_node == app->route.start_node) ||
        (app->route.has_goal && app->hover_node == app->route.goal_node)) {
        return;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->camera,
                           (float)app->route.graph.node_x[app->hover_node],
                           (float)app->route.graph.node_y[app->hover_node],
                           app->width, app->height, &sx, &sy);
    renderer_set_draw_color(&app->renderer, 80, 200, 255, 220);
    SDL_FRect rect = {sx - 5.0f, sy - 5.0f, 10.0f, 10.0f};
    renderer_draw_rect(&app->renderer, &rect);
}

bool app_recompute_route(AppState *app) {
    if (!app || !app->route.has_start || !app->route.has_goal) {
        return false;
    }

    bool ok = route_state_route(&app->route, app->route.start_node, app->route.goal_node);
    if (ok) {
        app_playback_reset(app);
    }
    return ok;
}
