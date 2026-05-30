#include "app/app_internal.h"
#include "app/app_pin_panel_internal.h"

#include "map/mercator.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void app_pin_panel_set_list_status(AppState *app, const char *message) {
    if (!app) {
        return;
    }
    snprintf(app->ui_state_bridge.pin_editor_status,
             sizeof(app->ui_state_bridge.pin_editor_status),
             "%s",
             message ? message : "");
}

static int app_pin_panel_row_at_point(const AppState *app, int x, int y) {
    if (!app) {
        return -1;
    }
    for (int i = 0; i < app->ui_state_bridge.pin_pane_row_count; ++i) {
        const SDL_FRect *row = &app->ui_state_bridge.pin_pane_row_rects[i];
        if ((float)x >= row->x &&
            (float)x <= row->x + row->w &&
            (float)y >= row->y &&
            (float)y <= row->y + row->h) {
            return app->ui_state_bridge.pin_pane_row_base + i;
        }
    }
    return -1;
}

static void app_pin_panel_update_drag_preview(AppState *app, int mouse_y) {
    SDL_FRect preview = {0};
    int slot = 0;
    if (!app || app->ui_state_bridge.pin_pane_row_count <= 0) {
        return;
    }

    slot = app->ui_state_bridge.pin_pane_row_base + app->ui_state_bridge.pin_pane_row_count;
    preview.x = app->ui_state_bridge.pin_pane_list_rect.x + 8.0f;
    preview.w = app->ui_state_bridge.pin_pane_list_rect.w - 16.0f;
    preview.h = 2.0f;
    preview.y = app->ui_state_bridge.pin_pane_row_rects[app->ui_state_bridge.pin_pane_row_count - 1].y +
                app->ui_state_bridge.pin_pane_row_rects[app->ui_state_bridge.pin_pane_row_count - 1].h;

    for (int i = 0; i < app->ui_state_bridge.pin_pane_row_count; ++i) {
        SDL_FRect row = app->ui_state_bridge.pin_pane_row_rects[i];
        float mid_y = row.y + row.h * 0.5f;
        if ((float)mouse_y <= mid_y) {
            slot = app->ui_state_bridge.pin_pane_row_base + i;
            preview.y = row.y;
            break;
        }
        if ((float)mouse_y <= row.y + row.h) {
            slot = app->ui_state_bridge.pin_pane_row_base + i + 1;
            preview.y = row.y + row.h;
            break;
        }
    }

    if (slot < 0) {
        slot = 0;
    }
    if (slot > (int)app->pins_file.pin_count) {
        slot = (int)app->pins_file.pin_count;
    }
    app->ui_state_bridge.pin_drag_target_slot = slot;
    app->ui_state_bridge.pin_drag_target_index = slot;
    app->ui_state_bridge.pin_drag_preview_rect = preview;
}

static bool app_pin_panel_apply_reorder(AppState *app) {
    char error[256];
    int source = -1;
    int target = -1;
    if (!app || !app->ui_state_bridge.pin_list_drag_active) {
        return false;
    }
    source = app->ui_state_bridge.pin_drag_source_index;
    if (source < 0 || source >= (int)app->pins_file.pin_count) {
        return false;
    }
    target = app->ui_state_bridge.pin_drag_target_slot;
    if (target < 0) {
        return false;
    }
    if (target > source) {
        target -= 1;
    }
    if (target < 0) {
        target = 0;
    }
    if (target >= (int)app->pins_file.pin_count) {
        target = (int)app->pins_file.pin_count - 1;
    }
    if (target == source) {
        return false;
    }
    if (!map_forge_pins_move(&app->pins_file, (size_t)source, (size_t)target)) {
        app_pin_panel_set_list_status(app, "Pin reorder failed.");
        return false;
    }
    if (!map_forge_pins_save(app->pins_path, &app->pins_file, error, sizeof(error))) {
        app_pin_panel_set_list_status(app, error);
        return false;
    }
    app->pins_dirty = false;
    app_pin_panel_select_saved_pin(app, target);
    app_pin_panel_set_list_status(app, "Pin order updated.");
    return true;
}

static bool app_pin_panel_set_route_endpoint_from_pin(AppState *app, int pin_index, bool set_start) {
    MercatorMeters meters = {0};
    RouteEndpointAnchor anchor = {0};
    const MapForgePin *pin = NULL;
    if (!app || pin_index < 0 || pin_index >= (int)app->pins_file.pin_count) {
        return false;
    }
    if (!app->route_state_bridge.route.loaded) {
        app_pin_panel_set_list_status(app, "Route graph is not ready for pin endpoints yet.");
        return false;
    }

    pin = &app->pins_file.pins[pin_index];
    meters = mercator_from_latlon((LatLon){pin->lat, pin->lon});
    if (!app_pick_route_anchor_unbounded(app, (float)meters.x, (float)meters.y, &anchor)) {
        app_pin_panel_set_list_status(app, "No route anchor found near that pin.");
        return false;
    }

    if (set_start) {
        app->route_state_bridge.route.start_node = anchor.node;
        app->route_state_bridge.route.has_start = true;
        app->route_state_bridge.start_anchor = anchor;
        snprintf(app->ui_state_bridge.pin_route_start_id,
                 sizeof(app->ui_state_bridge.pin_route_start_id),
                 "%s",
                 pin->id);
    } else {
        app->route_state_bridge.route.goal_node = anchor.node;
        app->route_state_bridge.route.has_goal = true;
        app->route_state_bridge.goal_anchor = anchor;
        snprintf(app->ui_state_bridge.pin_route_goal_id,
                 sizeof(app->ui_state_bridge.pin_route_goal_id),
                 "%s",
                 pin->id);
    }

    app_pin_panel_select_saved_pin(app, pin_index);
    if (app->route_state_bridge.route.has_start && app->route_state_bridge.route.has_goal) {
        app_route_schedule_recompute(app, 0.0);
    }
    app_pin_panel_set_list_status(app, set_start ? "Route start set from pin." : "Route goal set from pin.");
    return true;
}

bool app_pin_panel_row_has_route_start(const AppState *app, int pin_index) {
    if (!app || pin_index < 0 || pin_index >= (int)app->pins_file.pin_count) {
        return false;
    }
    return app->ui_state_bridge.pin_route_start_id[0] != '\0' &&
           strcmp(app->pins_file.pins[pin_index].id, app->ui_state_bridge.pin_route_start_id) == 0;
}

bool app_pin_panel_row_has_route_goal(const AppState *app, int pin_index) {
    if (!app || pin_index < 0 || pin_index >= (int)app->pins_file.pin_count) {
        return false;
    }
    return app->ui_state_bridge.pin_route_goal_id[0] != '\0' &&
           strcmp(app->pins_file.pins[pin_index].id, app->ui_state_bridge.pin_route_goal_id) == 0;
}

void app_pin_panel_clear_route_start(AppState *app) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.pin_route_start_id[0] = '\0';
}

void app_pin_panel_clear_route_goal(AppState *app) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.pin_route_goal_id[0] = '\0';
}

void app_pin_panel_clear_route_bindings(AppState *app) {
    app_pin_panel_clear_route_start(app);
    app_pin_panel_clear_route_goal(app);
}

void app_pin_panel_cancel_list_drag(AppState *app) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.pin_list_drag_armed = false;
    app->ui_state_bridge.pin_list_drag_active = false;
    app->ui_state_bridge.pin_drag_source_index = -1;
    app->ui_state_bridge.pin_drag_target_index = -1;
    app->ui_state_bridge.pin_drag_target_slot = -1;
    app->ui_state_bridge.pin_drag_start_mouse_y = 0;
    app->ui_state_bridge.pin_drag_last_mouse_y = 0;
    app->ui_state_bridge.pin_drag_preview_rect = (SDL_FRect){0};
}

bool app_pin_panel_handle_list_click(AppState *app, int x, int y) {
    int pin_index = -1;
    if (!app || app->ui_state_bridge.left_pane_section != APP_LEFT_PANE_SECTION_PINS) {
        return false;
    }
    pin_index = app_pin_panel_row_at_point(app, x, y);
    if (pin_index < 0) {
        return false;
    }
    if (app->ui_state_bridge.input.right_click_pressed) {
        app_pin_panel_cancel_list_drag(app);
        return app_pin_panel_set_route_endpoint_from_pin(app, pin_index, false);
    }
    if (!app->ui_state_bridge.input.left_click_pressed) {
        return false;
    }
    if (app->ui_state_bridge.input.shift_down) {
        app_pin_panel_cancel_list_drag(app);
        return app_pin_panel_set_route_endpoint_from_pin(app, pin_index, true);
    }

    app_pin_panel_select_saved_pin(app, pin_index);
    app->ui_state_bridge.pin_list_drag_armed = true;
    app->ui_state_bridge.pin_list_drag_active = false;
    app->ui_state_bridge.pin_drag_source_index = pin_index;
    app->ui_state_bridge.pin_drag_target_index = pin_index;
    app->ui_state_bridge.pin_drag_target_slot = pin_index;
    app->ui_state_bridge.pin_drag_start_mouse_y = y;
    app->ui_state_bridge.pin_drag_last_mouse_y = y;
    app->ui_state_bridge.pin_drag_preview_rect = (SDL_FRect){0};
    app_pin_panel_set_list_status(app, "Pin selected. Drag to reorder, Shift-click for start, right-click for goal.");
    return true;
}

bool app_pin_panel_handle_list_runtime_inputs(AppState *app) {
    int dy = 0;
    if (!app) {
        return false;
    }
    if (app->ui_state_bridge.pin_list_drag_active || app->ui_state_bridge.pin_list_drag_armed) {
        if ((app->ui_state_bridge.input.mouse_buttons & SDL_BUTTON_LMASK) == 0u &&
            !app->ui_state_bridge.input.left_click_pressed) {
            (void)app_pin_panel_apply_reorder(app);
            app_pin_panel_cancel_list_drag(app);
            return true;
        }

        dy = abs(app->ui_state_bridge.input.mouse_y - app->ui_state_bridge.pin_drag_start_mouse_y);
        app->ui_state_bridge.pin_drag_last_mouse_y = app->ui_state_bridge.input.mouse_y;
        if (!app->ui_state_bridge.pin_list_drag_active && dy >= 4) {
            app->ui_state_bridge.pin_list_drag_active = true;
        }
        if (app->ui_state_bridge.pin_list_drag_active) {
            app_pin_panel_update_drag_preview(app, app->ui_state_bridge.input.mouse_y);
            return true;
        }
    }
    return false;
}
