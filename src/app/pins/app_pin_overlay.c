#include "app/app_internal.h"
#include "app/app_map_viewport_internal.h"
#include "app/app_pin_panel_internal.h"

#include "map/mercator.h"

static bool app_find_pin_at_screen_point(const AppState *app,
                                         int screen_x,
                                         int screen_y,
                                         float hit_radius_px,
                                         int *out_pin_index) {
    float best_dist_sq = 0.0f;
    bool found = false;
    if (!app || !out_pin_index || app->pins_file.pin_count == 0u || hit_radius_px <= 0.0f) {
        return false;
    }

    for (size_t i = 0; i < app->pins_file.pin_count; ++i) {
        const MapForgePin *pin = &app->pins_file.pins[i];
        MercatorMeters meters = mercator_from_latlon((LatLon){pin->lat, pin->lon});
        float sx = 0.0f;
        float sy = 0.0f;
        float dx = 0.0f;
        float dy = 0.0f;
        float dist_sq = 0.0f;
        if (!app_map_world_to_screen(app, (float)meters.x, (float)meters.y, &sx, &sy)) {
            continue;
        }
        dx = (float)screen_x - sx;
        dy = (float)screen_y - sy;
        dist_sq = dx * dx + dy * dy;
        if (dist_sq > hit_radius_px * hit_radius_px) {
            continue;
        }
        if (!found || dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            *out_pin_index = (int)i;
            found = true;
        }
    }
    return found;
}

static void app_draw_pin_preview_marker(AppState *app, float local_x, float local_y) {
    if (!app) {
        return;
    }
    renderer_set_draw_color(&app->renderer, 255, 214, 96, 210);
    renderer_draw_line(&app->renderer, local_x - 10.0f, local_y, local_x + 10.0f, local_y);
    renderer_draw_line(&app->renderer, local_x, local_y - 10.0f, local_x, local_y + 10.0f);
    SDL_FRect ring = {local_x - 7.0f, local_y - 7.0f, 14.0f, 14.0f};
    renderer_draw_rect(&app->renderer, &ring);
}

static void app_draw_pin_marker(AppState *app,
                                float local_x,
                                float local_y,
                                bool selected,
                                bool route_start,
                                bool route_goal) {
    SDL_FRect outer = {
        local_x - (selected ? 6.0f : 4.0f),
        local_y - (selected ? 6.0f : 4.0f),
        selected ? 12.0f : 8.0f,
        selected ? 12.0f : 8.0f
    };
    SDL_FRect inner = {
        local_x - (selected ? 3.0f : 2.0f),
        local_y - (selected ? 3.0f : 2.0f),
        selected ? 6.0f : 4.0f,
        selected ? 6.0f : 4.0f
    };

    if (!app) {
        return;
    }

    if (route_start || route_goal) {
        if (route_start && route_goal) {
            renderer_set_draw_color(&app->renderer, 196, 132, 255, 240);
        } else if (route_start) {
            renderer_set_draw_color(&app->renderer, 72, 210, 120, 240);
        } else {
            renderer_set_draw_color(&app->renderer, 255, 96, 108, 240);
        }
        renderer_fill_rect(&app->renderer, &outer);
        renderer_set_draw_color(&app->renderer, 230, 236, 246, 255);
        renderer_draw_rect(&app->renderer, &outer);
        if (selected) {
            renderer_fill_rect(&app->renderer, &inner);
        }
        return;
    }

    if (selected) {
        renderer_set_draw_color(&app->renderer, 96, 185, 255, 235);
        renderer_fill_rect(&app->renderer, &outer);
        renderer_set_draw_color(&app->renderer, 228, 240, 255, 255);
        renderer_fill_rect(&app->renderer, &inner);
        return;
    }

    renderer_set_draw_color(&app->renderer, 108, 168, 228, 210);
    renderer_fill_rect(&app->renderer, &outer);
    renderer_set_draw_color(&app->renderer, 228, 240, 255, 230);
    renderer_draw_rect(&app->renderer, &outer);
}

void app_draw_pins_overlay(AppState *app) {
    if (!app) {
        return;
    }

    if (app->ui_state_bridge.pin_add_mode_active) {
        float world_x = 0.0f;
        float world_y = 0.0f;
        float local_x = 0.0f;
        float local_y = 0.0f;
        if (app_map_screen_to_world(app,
                                    (float)app->ui_state_bridge.input.mouse_x,
                                    (float)app->ui_state_bridge.input.mouse_y,
                                    &world_x,
                                    &world_y) &&
            app_map_world_to_viewport_local(app, world_x, world_y, &local_x, &local_y)) {
            app_draw_pin_preview_marker(app, local_x, local_y);
        }
    }

    if (app->pins_file.pin_count == 0u) {
        return;
    }

    for (size_t i = 0; i < app->pins_file.pin_count; ++i) {
        const MapForgePin *pin = &app->pins_file.pins[i];
        MercatorMeters meters = mercator_from_latlon((LatLon){pin->lat, pin->lon});
        float local_x = 0.0f;
        float local_y = 0.0f;
        if (!app_map_world_to_viewport_local(app, (float)meters.x, (float)meters.y, &local_x, &local_y)) {
            continue;
        }
        app_draw_pin_marker(app,
                            local_x,
                            local_y,
                            (int)i == app->ui_state_bridge.pin_selected_index,
                            app_pin_panel_row_has_route_start(app, (int)i),
                            app_pin_panel_row_has_route_goal(app, (int)i));
    }
}

bool app_select_pin_at_screen_point(AppState *app, int screen_x, int screen_y) {
    int pin_index = -1;
    if (!app || app->ui_state_bridge.pin_add_mode_active) {
        return false;
    }
    if (!app_map_viewport_contains_screen_point(app, screen_x, screen_y)) {
        return false;
    }
    if (!app_find_pin_at_screen_point(app, screen_x, screen_y, 10.0f, &pin_index)) {
        return false;
    }
    app_pin_editor_select_saved_pin(app, pin_index);
    app_pin_editor_set_status(app, "Pin selected from map.");
    return true;
}
