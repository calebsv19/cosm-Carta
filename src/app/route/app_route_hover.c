#include "app/app_internal.h"

#include <string.h>

void app_update_hover(AppState *app) {
    if (!app || !app->route_state_bridge.route.loaded) {
        if (app) {
            app->route_state_bridge.has_hover = false;
        }
        return;
    }

    float world_x = 0.0f;
    float world_y = 0.0f;
    camera_screen_to_world(&app->view_state_bridge.camera,
                           (float)app->ui_state_bridge.input.mouse_x,
                           (float)app->ui_state_bridge.input.mouse_y,
                           app->width,
                           app->height,
                           &world_x,
                           &world_y);

    RouteEndpointAnchor hover = {0};
    if (app_pick_route_anchor(app, world_x, world_y, &hover)) {
        app->route_state_bridge.hover_node = hover.node;
        app->route_state_bridge.hover_anchor = hover;
        app->route_state_bridge.has_hover = true;
    } else {
        memset(&app->route_state_bridge.hover_anchor, 0, sizeof(app->route_state_bridge.hover_anchor));
        app->route_state_bridge.has_hover = false;
    }
}

bool app_mouse_over_node(const AppState *app, uint32_t node, float radius) {
    if (!app || !app->route_state_bridge.route.loaded || node >= app->route_state_bridge.route.graph.node_count) {
        return false;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->view_state_bridge.camera,
                           (float)app->route_state_bridge.route.graph.node_x[node],
                           (float)app->route_state_bridge.route.graph.node_y[node],
                           app->width,
                           app->height,
                           &sx,
                           &sy);
    float dx = (float)app->ui_state_bridge.input.mouse_x - sx;
    float dy = (float)app->ui_state_bridge.input.mouse_y - sy;
    return (dx * dx + dy * dy) <= radius * radius;
}

bool app_mouse_over_anchor(const AppState *app, const RouteEndpointAnchor *anchor, float radius) {
    if (!app || !anchor || !anchor->valid || radius <= 0.0f) {
        return false;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->view_state_bridge.camera, anchor->world_x, anchor->world_y, app->width, app->height, &sx, &sy);
    float dx = (float)app->ui_state_bridge.input.mouse_x - sx;
    float dy = (float)app->ui_state_bridge.input.mouse_y - sy;
    return (dx * dx + dy * dy) <= radius * radius;
}

void app_draw_hover_marker(AppState *app) {
    if (!app || !app->route_state_bridge.has_hover || !app->route_state_bridge.route.loaded || !app->route_state_bridge.hover_anchor.valid) {
        return;
    }

    if ((app->route_state_bridge.start_anchor.valid && app_mouse_over_anchor(app, &app->route_state_bridge.start_anchor, 4.5f)) ||
        (app->route_state_bridge.goal_anchor.valid && app_mouse_over_anchor(app, &app->route_state_bridge.goal_anchor, 4.5f))) {
        return;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    camera_world_to_screen(&app->view_state_bridge.camera,
                           app->route_state_bridge.hover_anchor.world_x,
                           app->route_state_bridge.hover_anchor.world_y,
                           app->width,
                           app->height,
                           &sx,
                           &sy);
    renderer_set_draw_color(&app->renderer, 80, 200, 255, 220);
    SDL_FRect rect = {sx - 5.0f, sy - 5.0f, 10.0f, 10.0f};
    renderer_draw_rect(&app->renderer, &rect);

    if (app->route_state_bridge.route_edge_snap_debug && app->route_state_bridge.hover_anchor.on_edge &&
        app->route_state_bridge.hover_anchor.edge_from < app->route_state_bridge.route.graph.node_count &&
        app->route_state_bridge.hover_anchor.edge_to < app->route_state_bridge.route.graph.node_count) {
        float ax = 0.0f;
        float ay = 0.0f;
        float bx = 0.0f;
        float by = 0.0f;
        camera_world_to_screen(&app->view_state_bridge.camera,
                               (float)app->route_state_bridge.route.graph.node_x[app->route_state_bridge.hover_anchor.edge_from],
                               (float)app->route_state_bridge.route.graph.node_y[app->route_state_bridge.hover_anchor.edge_from],
                               app->width,
                               app->height,
                               &ax,
                               &ay);
        camera_world_to_screen(&app->view_state_bridge.camera,
                               (float)app->route_state_bridge.route.graph.node_x[app->route_state_bridge.hover_anchor.edge_to],
                               (float)app->route_state_bridge.route.graph.node_y[app->route_state_bridge.hover_anchor.edge_to],
                               app->width,
                               app->height,
                               &bx,
                               &by);
        renderer_set_draw_color(&app->renderer, 255, 210, 70, 220);
        renderer_draw_line(&app->renderer, ax, ay, bx, by);
    }
}
