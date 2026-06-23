#include "app/app_internal.h"
#include "app/app_map_viewport_internal.h"
#include "app/app_pin_panel_internal.h"

#include "map/mercator.h"

static bool app_point_in_rect(int x, int y, const SDL_FRect *rect) {
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f) {
        return false;
    }
    return (float)x >= rect->x &&
           (float)x <= rect->x + rect->w &&
           (float)y >= rect->y &&
           (float)y <= rect->y + rect->h;
}

bool app_pin_panel_handle_click(AppState *app, int x, int y) {
    if (!app) {
        return false;
    }
    app_pin_panel_layout(app);
    if (!app->ui_state_bridge.left_pane_open) {
        if (app_point_in_rect(x, y, &app->ui_state_bridge.pin_pane_closed_rect) &&
            app->ui_state_bridge.input.left_click_pressed) {
            app->ui_state_bridge.left_pane_open = true;
            app_pin_panel_layout(app);
            app_tile_viewport_invalidate(app);
            return true;
        }
        return false;
    }
    if (!app_point_in_rect(x, y, &app->ui_state_bridge.left_pane_rect)) {
        return false;
    }
    if (app->ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_PINS &&
        app_pin_panel_handle_list_click(app, x, y)) {
        return true;
    }
    if (!app->ui_state_bridge.input.left_click_pressed) {
        return true;
    }
    if (app_point_in_rect(x, y, &app->ui_state_bridge.pin_pane_close_rect)) {
        app_pin_panel_cancel_list_drag(app);
        app->ui_state_bridge.left_pane_open = false;
        app_pin_panel_layout(app);
        app_tile_viewport_invalidate(app);
        return true;
    }
    for (int i = 0; i < APP_LEFT_PANE_SECTION_COUNT; ++i) {
        if (app_point_in_rect(x, y, &app->ui_state_bridge.pin_pane_tab_rects[i])) {
            app_pin_panel_cancel_list_drag(app);
            app->ui_state_bridge.left_pane_section = (AppLeftPaneSection)i;
            app_pin_panel_layout(app);
            return true;
        }
    }
    if (app->ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_PINS) {
        if (app_point_in_rect(x, y, &app->ui_state_bridge.pin_pane_add_rect)) {
            if (app->ui_state_bridge.pin_add_mode_active) {
                app->ui_state_bridge.pin_add_mode_active = false;
                app->ui_state_bridge.pin_editor_waiting_for_map_click = false;
                if (app->ui_state_bridge.pin_selected_index >= 0 &&
                    app->ui_state_bridge.pin_selected_index < (int)app->pins_file.pin_count) {
                    app_pin_editor_select_saved_pin(app, app->ui_state_bridge.pin_selected_index);
                }
                app_pin_editor_set_status(app, "Add mode disabled.");
            } else {
                app_pin_editor_begin_new_pin(app);
            }
            return true;
        }
        if (app_point_in_rect(x, y, &app->ui_state_bridge.pin_pane_save_rect)) {
            (void)app_pin_editor_save_draft(app);
            return true;
        }
        if (app_pin_panel_handle_name_click(app, x, y)) {
            return true;
        }
        if (app_pin_panel_handle_metadata_click(app, x, y)) {
            return true;
        }
        if (app_point_in_rect(x, y, &app->ui_state_bridge.pin_pane_delete_rect)) {
            (void)app_pin_editor_delete_selected(app);
            return true;
        }
        if (app_point_in_rect(x, y, &app->ui_state_bridge.pin_pane_cancel_rect)) {
            app_pin_editor_cancel_draft(app);
            return true;
        }
    }
    return true;
}

bool app_pin_panel_handle_runtime_inputs(AppState *app) {
    if (!app ||
        !app->ui_state_bridge.left_pane_open ||
        app->ui_state_bridge.left_pane_section != APP_LEFT_PANE_SECTION_PINS) {
        return false;
    }
    if (app->ui_state_bridge.input.mouse_wheel_y != 0 &&
        app_point_in_rect(app->ui_state_bridge.input.mouse_x,
                          app->ui_state_bridge.input.mouse_y,
                          &app->ui_state_bridge.pin_pane_list_rect)) {
        int available = (int)app->pins_file.pin_count;
        int visible = app->ui_state_bridge.pin_pane_row_count > 0 ? app->ui_state_bridge.pin_pane_row_count : 1;
        int max_base = available - visible;
        if (max_base < 0) {
            max_base = 0;
        }
        app->ui_state_bridge.pin_pane_row_base -= app->ui_state_bridge.input.mouse_wheel_y;
        if (app->ui_state_bridge.pin_pane_row_base < 0) {
            app->ui_state_bridge.pin_pane_row_base = 0;
        }
        if (app->ui_state_bridge.pin_pane_row_base > max_base) {
            app->ui_state_bridge.pin_pane_row_base = max_base;
        }
        app_pin_panel_layout(app);
        return true;
    }
    if (app_pin_panel_handle_list_runtime_inputs(app)) {
        return true;
    }
    if (!app->ui_state_bridge.pin_editor_has_draft) {
        return false;
    }
    if (app_pin_panel_handle_name_runtime_inputs(app)) {
        if (app->ui_state_bridge.input.enter_pressed) {
            return app_pin_editor_save_draft(app);
        }
        return true;
    }
    return false;
}

bool app_pin_panel_handle_map_click(AppState *app, int x, int y) {
    float world_x = 0.0f;
    float world_y = 0.0f;
    MercatorMeters meters = {0};
    LatLon latlon = {0};
    if (!app ||
        !app->ui_state_bridge.pin_add_mode_active ||
        !app->ui_state_bridge.input.right_click_pressed) {
        return false;
    }
    if (!app_map_screen_to_world(app, (float)x, (float)y, &world_x, &world_y)) {
        return false;
    }
    app_pin_editor_begin_new_pin(app);
    meters.x = world_x;
    meters.y = world_y;
    latlon = mercator_to_latlon(meters);
    app->ui_state_bridge.pin_editor_draft.lat = latlon.lat;
    app->ui_state_bridge.pin_editor_draft.lon = latlon.lon;
    app->ui_state_bridge.pin_editor_waiting_for_map_click = false;
    if (!app_pin_editor_save_draft(app)) {
        return true;
    }
    app->ui_state_bridge.pin_add_mode_active = true;
    app_pin_editor_set_status(app, "Pin created. Right-click again to add another, or rename the selected pin.");
    return true;
}
