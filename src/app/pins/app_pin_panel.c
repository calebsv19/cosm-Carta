#include "app/app_internal.h"
#include "app/app_map_viewport_internal.h"
#include "app/app_pin_panel_internal.h"

#include "map/mercator.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static bool app_point_in_rect(int x, int y, const SDL_FRect *rect) {
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f) {
        return false;
    }
    return (float)x >= rect->x &&
           (float)x <= rect->x + rect->w &&
           (float)y >= rect->y &&
           (float)y <= rect->y + rect->h;
}

static void app_pin_panel_set_status(AppState *app, const char *message) {
    if (!app) {
        return;
    }
    snprintf(app->ui_state_bridge.pin_editor_status,
             sizeof(app->ui_state_bridge.pin_editor_status),
             "%s",
             message ? message : "");
}

static void app_pin_panel_sync_editor_name(AppState *app) {
    if (!app) {
        return;
    }
    snprintf(app->ui_state_bridge.pin_editor_name_edit,
             sizeof(app->ui_state_bridge.pin_editor_name_edit),
             "%s",
             app->ui_state_bridge.pin_editor_draft.name);
    app_pin_panel_name_edit_deactivate(app);
    app_pin_panel_name_edit_sync_cursor_to_end(app);
}

static int app_pin_panel_next_untitled_index(const AppState *app) {
    int max_index = -1;
    if (!app) {
        return 0;
    }
    for (size_t i = 0; i < app->pins_file.pin_count; ++i) {
        const char *name = app->pins_file.pins[i].name;
        int index = -1;
        if (sscanf(name, "Untitled %d", &index) == 1 && index >= max_index) {
            max_index = index + 1;
        }
    }
    return max_index < 0 ? 0 : max_index;
}

static void app_pin_panel_stamp_now(char *dst, size_t dst_size) {
    time_t now = 0;
    struct tm local_tm;
    if (!dst || dst_size == 0u) {
        return;
    }
    now = time(NULL);
    memset(&local_tm, 0, sizeof(local_tm));
#if defined(_POSIX_VERSION)
    localtime_r(&now, &local_tm);
#else
    {
        struct tm *tmp = localtime(&now);
        if (tmp) {
            local_tm = *tmp;
        }
    }
#endif
    strftime(dst, dst_size, "%Y-%m-%dT%H:%M:%S", &local_tm);
}

static void app_pin_panel_slugify_name(const char *name, char *out_slug, size_t out_size) {
    size_t write_index = 0u;
    bool last_was_sep = false;
    if (!out_slug || out_size == 0u) {
        return;
    }
    out_slug[0] = '\0';
    if (!name) {
        return;
    }
    for (size_t i = 0; name[i] != '\0' && write_index + 1u < out_size; ++i) {
        unsigned char ch = (unsigned char)name[i];
        if (isalnum(ch)) {
            out_slug[write_index++] = (char)tolower(ch);
            last_was_sep = false;
            continue;
        }
        if (!last_was_sep && write_index > 0u) {
            out_slug[write_index++] = '_';
            last_was_sep = true;
        }
    }
    while (write_index > 0u && out_slug[write_index - 1u] == '_') {
        write_index -= 1u;
    }
    out_slug[write_index] = '\0';
}

void app_pin_panel_select_saved_pin(AppState *app, int index) {
    if (!app) {
        return;
    }
    if (index < 0 || index >= (int)app->pins_file.pin_count) {
        app->ui_state_bridge.pin_selected_index = -1;
        app->ui_state_bridge.pin_add_mode_active = false;
        app->ui_state_bridge.pin_editor_has_draft = false;
        app->ui_state_bridge.pin_editor_is_new = false;
        app->ui_state_bridge.pin_editor_waiting_for_map_click = false;
        memset(&app->ui_state_bridge.pin_editor_draft, 0, sizeof(app->ui_state_bridge.pin_editor_draft));
        app->ui_state_bridge.pin_editor_name_edit[0] = '\0';
        app_pin_panel_name_edit_deactivate(app);
        app->ui_state_bridge.pin_name_cursor_index = 0;
        return;
    }
    app->ui_state_bridge.pin_selected_index = index;
    app->ui_state_bridge.pin_editor_has_draft = true;
    app->ui_state_bridge.pin_editor_is_new = false;
    app->ui_state_bridge.pin_editor_waiting_for_map_click = false;
    app->ui_state_bridge.pin_editor_draft = app->pins_file.pins[index];
    app_pin_panel_sync_editor_name(app);
}

static void app_pin_panel_begin_new_pin(AppState *app) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.pin_selected_index = -1;
    app->ui_state_bridge.pin_add_mode_active = true;
    app->ui_state_bridge.pin_editor_has_draft = true;
    app->ui_state_bridge.pin_editor_is_new = true;
    app->ui_state_bridge.pin_editor_waiting_for_map_click = true;
    memset(&app->ui_state_bridge.pin_editor_draft, 0, sizeof(app->ui_state_bridge.pin_editor_draft));
    snprintf(app->ui_state_bridge.pin_editor_draft.type,
             sizeof(app->ui_state_bridge.pin_editor_draft.type),
             "general");
    snprintf(app->ui_state_bridge.pin_editor_draft.color,
             sizeof(app->ui_state_bridge.pin_editor_draft.color),
             "blue");
    app->ui_state_bridge.pin_editor_draft.private_flag = true;
    snprintf(app->ui_state_bridge.pin_editor_name_edit,
             sizeof(app->ui_state_bridge.pin_editor_name_edit),
             "Untitled %d",
             app_pin_panel_next_untitled_index(app));
    app_pin_panel_name_edit_deactivate(app);
    app_pin_panel_name_edit_sync_cursor_to_end(app);
    app_pin_panel_set_status(app, "Add mode active. Right-click the map to create pins.");
}

static bool app_pin_panel_save_draft(AppState *app) {
    char error[256];
    char slug[MAPFORGE_PIN_ID_CAPACITY];
    char candidate[MAPFORGE_PIN_ID_CAPACITY];
    char stamp[64];
    int selected_index = -1;
    if (!app || !app->ui_state_bridge.pin_editor_has_draft) {
        return false;
    }
    snprintf(app->ui_state_bridge.pin_editor_draft.name,
             sizeof(app->ui_state_bridge.pin_editor_draft.name),
             "%s",
             app->ui_state_bridge.pin_editor_name_edit);
    if (app->ui_state_bridge.pin_editor_draft.name[0] == '\0') {
        app_pin_panel_set_status(app, "Pin name is required before saving.");
        return false;
    }
    if (app->ui_state_bridge.pin_editor_waiting_for_map_click) {
        app_pin_panel_set_status(app, "Place the draft on the map before saving.");
        return false;
    }
    if (app->ui_state_bridge.pin_editor_is_new || app->ui_state_bridge.pin_editor_draft.id[0] == '\0') {
        app_pin_panel_slugify_name(app->ui_state_bridge.pin_editor_draft.name,
                                   slug,
                                   sizeof(slug));
        if (slug[0] == '\0') {
            snprintf(slug, sizeof(slug), "pin_%zu", app->pins_file.pin_count + 1u);
        }
        snprintf(candidate, sizeof(candidate), "%s", slug);
        for (size_t attempt = 2u;
             map_forge_pins_find_by_id_const(&app->pins_file, candidate) != NULL &&
             attempt < 1000u;
             ++attempt) {
            snprintf(candidate, sizeof(candidate), "%s_%zu", slug, attempt);
        }
        snprintf(app->ui_state_bridge.pin_editor_draft.id,
                 sizeof(app->ui_state_bridge.pin_editor_draft.id),
                 "%s",
                 candidate);
        app_pin_panel_stamp_now(stamp, sizeof(stamp));
        snprintf(app->ui_state_bridge.pin_editor_draft.created_at,
                 sizeof(app->ui_state_bridge.pin_editor_draft.created_at),
                 "%s",
                 stamp);
    }
    app_pin_panel_stamp_now(stamp, sizeof(stamp));
    snprintf(app->ui_state_bridge.pin_editor_draft.updated_at,
             sizeof(app->ui_state_bridge.pin_editor_draft.updated_at),
             "%s",
             stamp);
    if (!map_forge_pins_upsert(&app->pins_file,
                               &app->ui_state_bridge.pin_editor_draft,
                               error,
                               sizeof(error))) {
        app_pin_panel_set_status(app, error);
        return false;
    }
    snprintf(app->pins_file.map_region,
             sizeof(app->pins_file.map_region),
             "%s",
             app->region.name ? app->region.name : "");
    if (!map_forge_pins_save(app->pins_path, &app->pins_file, error, sizeof(error))) {
        app_pin_panel_set_status(app, error);
        return false;
    }
    app->pins_dirty = false;
    for (size_t i = 0; i < app->pins_file.pin_count; ++i) {
        if (strcmp(app->pins_file.pins[i].id, app->ui_state_bridge.pin_editor_draft.id) == 0) {
            selected_index = (int)i;
            break;
        }
    }
    app_pin_panel_select_saved_pin(app, selected_index);
    app_pin_panel_set_status(app, "Pin saved.");
    return true;
}

static bool app_pin_panel_delete_selected(AppState *app) {
    char error[256];
    if (!app) {
        return false;
    }
    if (app->ui_state_bridge.pin_editor_is_new) {
        app->ui_state_bridge.pin_editor_has_draft = false;
        app->ui_state_bridge.pin_add_mode_active = false;
        app->ui_state_bridge.pin_editor_waiting_for_map_click = false;
        app->ui_state_bridge.pin_editor_is_new = false;
        memset(&app->ui_state_bridge.pin_editor_draft, 0, sizeof(app->ui_state_bridge.pin_editor_draft));
        app->ui_state_bridge.pin_editor_name_edit[0] = '\0';
        app_pin_panel_name_edit_deactivate(app);
        app->ui_state_bridge.pin_name_cursor_index = 0;
        app_pin_panel_set_status(app, "Draft pin discarded.");
        return true;
    }
    if (app->ui_state_bridge.pin_selected_index < 0 ||
        app->ui_state_bridge.pin_selected_index >= (int)app->pins_file.pin_count) {
        app_pin_panel_set_status(app, "Select a saved pin to delete.");
        return false;
    }
    if (!map_forge_pins_remove_by_id(&app->pins_file, app->ui_state_bridge.pin_editor_draft.id)) {
        app_pin_panel_set_status(app, "Failed to remove pin.");
        return false;
    }
    if (strcmp(app->ui_state_bridge.pin_route_start_id, app->ui_state_bridge.pin_editor_draft.id) == 0) {
        app_pin_panel_clear_route_start(app);
    }
    if (strcmp(app->ui_state_bridge.pin_route_goal_id, app->ui_state_bridge.pin_editor_draft.id) == 0) {
        app_pin_panel_clear_route_goal(app);
    }
    if (!map_forge_pins_save(app->pins_path, &app->pins_file, error, sizeof(error))) {
        app_pin_panel_set_status(app, error);
        return false;
    }
    app->pins_dirty = false;
    if (app->ui_state_bridge.pin_selected_index >= (int)app->pins_file.pin_count) {
        app->ui_state_bridge.pin_selected_index = (int)app->pins_file.pin_count - 1;
    }
    if (app->pins_file.pin_count > 0u) {
        app_pin_panel_select_saved_pin(app, app->ui_state_bridge.pin_selected_index);
    } else {
        app_pin_panel_select_saved_pin(app, -1);
    }
    app_pin_panel_set_status(app, "Pin deleted.");
    return true;
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
            app->tile_state_bridge.queue_valid = false;
            app->tile_state_bridge.visible_valid = false;
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
        app->tile_state_bridge.queue_valid = false;
        app->tile_state_bridge.visible_valid = false;
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
                    app_pin_panel_select_saved_pin(app, app->ui_state_bridge.pin_selected_index);
                }
                app_pin_panel_set_status(app, "Add mode disabled.");
            } else {
                app_pin_panel_begin_new_pin(app);
            }
            return true;
        }
        if (app_point_in_rect(x, y, &app->ui_state_bridge.pin_pane_save_rect)) {
            (void)app_pin_panel_save_draft(app);
            return true;
        }
        if (app_pin_panel_handle_name_click(app, x, y)) {
            return true;
        }
        if (app_pin_panel_handle_metadata_click(app, x, y)) {
            return true;
        }
        if (app_point_in_rect(x, y, &app->ui_state_bridge.pin_pane_delete_rect)) {
            (void)app_pin_panel_delete_selected(app);
            return true;
        }
        if (app_point_in_rect(x, y, &app->ui_state_bridge.pin_pane_cancel_rect)) {
            app->ui_state_bridge.pin_add_mode_active = false;
            app->ui_state_bridge.pin_editor_waiting_for_map_click = false;
            if (app->ui_state_bridge.pin_selected_index >= 0 &&
                app->ui_state_bridge.pin_selected_index < (int)app->pins_file.pin_count) {
                app_pin_panel_select_saved_pin(app, app->ui_state_bridge.pin_selected_index);
                app_pin_panel_set_status(app, "Edits reverted.");
            } else {
                app_pin_panel_select_saved_pin(app, -1);
                app_pin_panel_set_status(app, "Draft cleared.");
            }
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
            return app_pin_panel_save_draft(app);
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
    app_pin_panel_begin_new_pin(app);
    meters.x = world_x;
    meters.y = world_y;
    latlon = mercator_to_latlon(meters);
    app->ui_state_bridge.pin_editor_draft.lat = latlon.lat;
    app->ui_state_bridge.pin_editor_draft.lon = latlon.lon;
    app->ui_state_bridge.pin_editor_waiting_for_map_click = false;
    if (!app_pin_panel_save_draft(app)) {
        return true;
    }
    app->ui_state_bridge.pin_add_mode_active = true;
    app_pin_panel_set_status(app, "Pin created. Right-click again to add another, or rename the selected pin.");
    return true;
}
