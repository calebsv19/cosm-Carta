#include "app/app_internal.h"
#include "app/app_ui_internal.h"

#include "map/mercator.h"
#include "ui/font.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
    APP_PIN_PANE_ROOT_ID = 1u,
    APP_PIN_PANE_LEFT_ID = 2u,
    APP_PIN_PANE_MAP_ID = 3u
};

static bool app_point_in_rect(int x, int y, const SDL_FRect *rect) {
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f) {
        return false;
    }
    return (float)x >= rect->x &&
           (float)x <= rect->x + rect->w &&
           (float)y >= rect->y &&
           (float)y <= rect->y + rect->h;
}

static SDL_FRect app_full_map_rect(const AppState *app) {
    SDL_FRect rect = {0.0f, APP_HEADER_HEIGHT, 0.0f, 0.0f};
    if (!app) {
        return rect;
    }
    rect.w = (float)app->width;
    rect.h = (float)app->height - APP_HEADER_HEIGHT;
    if (rect.h < 0.0f) {
        rect.h = 0.0f;
    }
    return rect;
}

static const char *app_left_pane_section_label(AppLeftPaneSection section) {
    switch (section) {
        case APP_LEFT_PANE_SECTION_INGEST:
            return "INGEST";
        case APP_LEFT_PANE_SECTION_INSPECT:
            return "INSPECT";
        case APP_LEFT_PANE_SECTION_PINS:
        default:
            return "PINS";
    }
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
    selected_index = -1;
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

static void app_pin_panel_clear_rows(AppState *app) {
    if (!app) {
        return;
    }
    memset(app->ui_state_bridge.pin_pane_row_rects, 0, sizeof(app->ui_state_bridge.pin_pane_row_rects));
    app->ui_state_bridge.pin_pane_row_count = 0;
}

static void app_pin_panel_clear_layout(AppState *app) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.left_pane_rect = (SDL_FRect){0};
    app->ui_state_bridge.map_viewport_rect = app_full_map_rect(app);
    app->ui_state_bridge.pin_pane_closed_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_header_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_close_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_content_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_list_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_add_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_save_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_delete_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_cancel_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_name_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_type_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_color_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_private_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_drag_preview_rect = (SDL_FRect){0};
    memset(app->ui_state_bridge.pin_pane_tab_rects, 0, sizeof(app->ui_state_bridge.pin_pane_tab_rects));
    app_pin_panel_clear_rows(app);
}

static void app_pin_panel_assign_leaf_rects(AppState *app,
                                            const CorePaneLeafRect *leaf_rects,
                                            uint32_t leaf_count) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.left_pane_rect = (SDL_FRect){0};
    /* The pane is an overlay host, not a live viewport split. Keep the
     * map viewport anchored to the full map rect so opening the pane does not
     * shift render/world-space math under the cursor. */
    app->ui_state_bridge.map_viewport_rect = app_full_map_rect(app);
    for (uint32_t i = 0; i < leaf_count; ++i) {
        SDL_FRect rect = {
            leaf_rects[i].rect.x,
            leaf_rects[i].rect.y,
            leaf_rects[i].rect.width,
            leaf_rects[i].rect.height
        };
        if (leaf_rects[i].id == APP_PIN_PANE_LEFT_ID) {
            app->ui_state_bridge.left_pane_rect = rect;
        }
    }
}

static void app_pin_panel_build_shell(AppState *app) {
    CorePaneNode nodes[3];
    CorePaneLeafRect leaf_rects[2];
    uint32_t leaf_count = 0u;
    CorePaneRect bounds;

    if (!app) {
        return;
    }

    bounds.x = 0.0f;
    bounds.y = APP_HEADER_HEIGHT;
    bounds.width = (float)app->width;
    bounds.height = (float)app->height - APP_HEADER_HEIGHT;
    if (bounds.width <= 1.0f || bounds.height <= 1.0f) {
        app->ui_state_bridge.left_pane_rect = (SDL_FRect){0};
        app->ui_state_bridge.map_viewport_rect = app_full_map_rect(app);
        return;
    }

    memset(nodes, 0, sizeof(nodes));
    nodes[0].type = CORE_PANE_NODE_SPLIT;
    nodes[0].id = APP_PIN_PANE_ROOT_ID;
    nodes[0].axis = CORE_PANE_AXIS_HORIZONTAL;
    nodes[0].ratio_01 = 0.28f;
    nodes[0].child_a = 1u;
    nodes[0].child_b = 2u;
    nodes[0].constraints.min_size_a = 280.0f;
    nodes[0].constraints.min_size_b = 480.0f;
    nodes[1].type = CORE_PANE_NODE_LEAF;
    nodes[1].id = APP_PIN_PANE_LEFT_ID;
    nodes[2].type = CORE_PANE_NODE_LEAF;
    nodes[2].id = APP_PIN_PANE_MAP_ID;

    if (!core_pane_solve(nodes, 3u, 0u, bounds, leaf_rects, 2u, &leaf_count)) {
        app->ui_state_bridge.left_pane_rect = (SDL_FRect){0};
        app->ui_state_bridge.map_viewport_rect = app_full_map_rect(app);
        return;
    }

    app_pin_panel_assign_leaf_rects(app, leaf_rects, leaf_count);
}

static int app_left_pane_list_count(const AppState *app) {
    if (!app) {
        return 0;
    }
    switch (app->ui_state_bridge.left_pane_section) {
        case APP_LEFT_PANE_SECTION_INGEST:
            return app->ingest_show_active_tab ? app->ingest_active_count : app->ingest_osm_count;
        case APP_LEFT_PANE_SECTION_PINS:
            return (int)app->pins_file.pin_count;
        case APP_LEFT_PANE_SECTION_INSPECT:
        default:
            return 0;
    }
}

static void app_pin_panel_build_rows(AppState *app, float row_h) {
    int max_rows = 0;
    int available = 0;
    int row_base = 0;
    if (!app || row_h <= 0.0f) {
        return;
    }
    available = app_left_pane_list_count(app);
    if (app->ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_PINS) {
        int max_base = available - APP_PIN_LIST_MAX;
        if (max_base < 0) {
            max_base = 0;
        }
        if (app->ui_state_bridge.pin_pane_row_base < 0) {
            app->ui_state_bridge.pin_pane_row_base = 0;
        }
        if (app->ui_state_bridge.pin_pane_row_base > max_base) {
            app->ui_state_bridge.pin_pane_row_base = max_base;
        }
        row_base = app->ui_state_bridge.pin_pane_row_base;
    } else {
        app->ui_state_bridge.pin_pane_row_base = 0;
    }
    max_rows = (int)(app->ui_state_bridge.pin_pane_list_rect.h / row_h);
    if (max_rows > APP_PIN_LIST_MAX) {
        max_rows = APP_PIN_LIST_MAX;
    }
    if (available - row_base < max_rows) {
        max_rows = available - row_base;
    }
    if (max_rows < 0) {
        max_rows = 0;
    }
    app->ui_state_bridge.pin_pane_row_count = max_rows;
    for (int i = 0; i < max_rows; ++i) {
        app->ui_state_bridge.pin_pane_row_rects[i] = (SDL_FRect){
            app->ui_state_bridge.pin_pane_list_rect.x,
            app->ui_state_bridge.pin_pane_list_rect.y + (float)i * row_h,
            app->ui_state_bridge.pin_pane_list_rect.w,
            row_h - 2.0f
        };
    }
}

void app_pin_panel_layout(AppState *app) {
    const float pad = 10.0f;
    const float row_h = 30.0f;
    const float tab_gap = 8.0f;
    const float tab_h = 22.0f;
    const float body_gap = 10.0f;
    const float close_size = 18.0f;
    const float button_h = 20.0f;
    if (!app) {
        return;
    }

    app_pin_panel_clear_layout(app);

    if (!app->ui_state_bridge.left_pane_open) {
        app->ui_state_bridge.pin_pane_closed_rect = (SDL_FRect){
            8.0f,
            APP_HEADER_HEIGHT + 8.0f,
            72.0f,
            22.0f
        };
        return;
    }

    app_pin_panel_build_shell(app);
    SDL_FRect pane = app->ui_state_bridge.left_pane_rect;
    float list_top = 0.0f;
    if (pane.w <= 1.0f || pane.h <= 1.0f) {
        return;
    }

    app->ui_state_bridge.pin_pane_header_rect = (SDL_FRect){
        pane.x + pad,
        pane.y + pad,
        pane.w - pad * 2.0f,
        tab_h
    };
    app->ui_state_bridge.pin_pane_close_rect = (SDL_FRect){
        app->ui_state_bridge.pin_pane_header_rect.x + app->ui_state_bridge.pin_pane_header_rect.w - close_size,
        app->ui_state_bridge.pin_pane_header_rect.y + 2.0f,
        close_size,
        close_size
    };

    float tab_y = app->ui_state_bridge.pin_pane_header_rect.y;
    float tab_x = app->ui_state_bridge.pin_pane_header_rect.x;
    float tab_area_w = app->ui_state_bridge.pin_pane_header_rect.w - close_size - tab_gap - 4.0f;
    float tab_w = (tab_area_w - tab_gap * (float)(APP_LEFT_PANE_SECTION_COUNT - 1)) /
                  (float)APP_LEFT_PANE_SECTION_COUNT;
    if (tab_w < 72.0f) {
        tab_w = 72.0f;
    }
    for (int i = 0; i < APP_LEFT_PANE_SECTION_COUNT; ++i) {
        app->ui_state_bridge.pin_pane_tab_rects[i] = (SDL_FRect){
            tab_x + (tab_w + tab_gap) * (float)i,
            tab_y,
            tab_w,
            tab_h
        };
    }

    app->ui_state_bridge.pin_pane_content_rect = (SDL_FRect){
        pane.x + pad,
        app->ui_state_bridge.pin_pane_header_rect.y + app->ui_state_bridge.pin_pane_header_rect.h + body_gap,
        pane.w - pad * 2.0f,
        pane.h - (pad * 2.0f) - app->ui_state_bridge.pin_pane_header_rect.h - body_gap
    };
    if (app->ui_state_bridge.pin_pane_content_rect.h < 0.0f) {
        app->ui_state_bridge.pin_pane_content_rect.h = 0.0f;
    }

    switch (app->ui_state_bridge.left_pane_section) {
        case APP_LEFT_PANE_SECTION_INGEST:
            list_top = 92.0f;
            break;
        case APP_LEFT_PANE_SECTION_INSPECT:
            list_top = app->ui_state_bridge.pin_pane_content_rect.h;
            break;
        case APP_LEFT_PANE_SECTION_PINS:
        default:
            list_top = 218.0f;
            break;
    }
    if (list_top > app->ui_state_bridge.pin_pane_content_rect.h) {
        list_top = app->ui_state_bridge.pin_pane_content_rect.h;
    }
    app->ui_state_bridge.pin_pane_list_rect = (SDL_FRect){
        app->ui_state_bridge.pin_pane_content_rect.x,
        app->ui_state_bridge.pin_pane_content_rect.y + list_top,
        app->ui_state_bridge.pin_pane_content_rect.w,
        app->ui_state_bridge.pin_pane_content_rect.h - list_top
    };
    if (app->ui_state_bridge.pin_pane_list_rect.h < 0.0f) {
        app->ui_state_bridge.pin_pane_list_rect.h = 0.0f;
    }

    if (app->ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_PINS) {
        float body_x = app->ui_state_bridge.pin_pane_content_rect.x;
        float body_y = app->ui_state_bridge.pin_pane_content_rect.y + 42.0f;
        float body_w = app->ui_state_bridge.pin_pane_content_rect.w;
        float button_gap = 8.0f;
        float button_w = (body_w - button_gap * 3.0f) / 4.0f;
        if (button_w < 64.0f) {
            button_w = 64.0f;
        }
        app->ui_state_bridge.pin_pane_add_rect = (SDL_FRect){body_x, body_y, button_w, button_h};
        app->ui_state_bridge.pin_pane_save_rect = (SDL_FRect){body_x + button_w + button_gap, body_y, button_w, button_h};
        app->ui_state_bridge.pin_pane_delete_rect = (SDL_FRect){body_x + (button_w + button_gap) * 2.0f, body_y, button_w, button_h};
        app->ui_state_bridge.pin_pane_cancel_rect = (SDL_FRect){body_x + (button_w + button_gap) * 3.0f, body_y, button_w, button_h};
        app->ui_state_bridge.pin_pane_name_rect = (SDL_FRect){
            body_x,
            body_y + button_h + 10.0f,
            body_w,
            24.0f
        };
        app->ui_state_bridge.pin_pane_type_rect = (SDL_FRect){
            body_x,
            app->ui_state_bridge.pin_pane_name_rect.y + app->ui_state_bridge.pin_pane_name_rect.h + 10.0f,
            (body_w - button_gap * 2.0f) / 3.0f,
            20.0f
        };
        app->ui_state_bridge.pin_pane_color_rect = (SDL_FRect){
            app->ui_state_bridge.pin_pane_type_rect.x + app->ui_state_bridge.pin_pane_type_rect.w + button_gap,
            app->ui_state_bridge.pin_pane_type_rect.y,
            app->ui_state_bridge.pin_pane_type_rect.w,
            20.0f
        };
        app->ui_state_bridge.pin_pane_private_rect = (SDL_FRect){
            app->ui_state_bridge.pin_pane_color_rect.x + app->ui_state_bridge.pin_pane_color_rect.w + button_gap,
            app->ui_state_bridge.pin_pane_type_rect.y,
            app->ui_state_bridge.pin_pane_type_rect.w,
            20.0f
        };
    }

    if (app->ui_state_bridge.pin_selected_index >= (int)app->pins_file.pin_count) {
        app->ui_state_bridge.pin_selected_index = (app->pins_file.pin_count > 0u) ? 0 : -1;
    }

    if (app->ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_PINS ||
        app->ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_INGEST) {
        app_pin_panel_build_rows(app, row_h);
    }
}

static void app_draw_left_pane_header(AppState *app,
                                      const MapForgeThemePalette *palette,
                                      SDL_Color text,
                                      SDL_Color muted) {
    if (!app || !palette) {
        return;
    }

    renderer_set_draw_color(&app->renderer, palette->button_fill.r, palette->button_fill.g, palette->button_fill.b, palette->button_fill.a);
    renderer_fill_rect(&app->renderer, &app->ui_state_bridge.pin_pane_close_rect);
    renderer_set_draw_color(&app->renderer, palette->button_outline.r, palette->button_outline.g, palette->button_outline.b, palette->button_outline.a);
    renderer_draw_rect(&app->renderer, &app->ui_state_bridge.pin_pane_close_rect);
    ui_draw_text(&app->renderer,
                 (int)(app->ui_state_bridge.pin_pane_close_rect.x + 6.0f),
                 (int)(app->ui_state_bridge.pin_pane_close_rect.y + 2.0f),
                 "-",
                 text,
                 1.0f);

    for (int i = 0; i < APP_LEFT_PANE_SECTION_COUNT; ++i) {
        SDL_FRect rect = app->ui_state_bridge.pin_pane_tab_rects[i];
        const bool active = i == (int)app->ui_state_bridge.left_pane_section;
        SDL_Color fill = active ? palette->button_active_primary : palette->button_fill;
        SDL_Color outline = active ? palette->overlay_accent : palette->button_outline;
        renderer_set_draw_color(&app->renderer, fill.r, fill.g, fill.b, fill.a);
        renderer_fill_rect(&app->renderer, &rect);
        renderer_set_draw_color(&app->renderer, outline.r, outline.g, outline.b, outline.a);
        renderer_draw_rect(&app->renderer, &rect);
        ui_draw_text_clipped(&app->renderer,
                     (int)(rect.x + 8.0f),
                     (int)(rect.y + 4.0f),
                     app_left_pane_section_label((AppLeftPaneSection)i),
                     text,
                     0.85f,
                     (int)rect.w - 12);
    }
    (void)muted;
}

static void app_draw_left_pane_pins(AppState *app,
                                    const MapForgeThemePalette *palette,
                                    SDL_Color text,
                                    SDL_Color muted) {
    char line[256];
    const char *hint = NULL;
    if (!app || !palette) {
        return;
    }

    char summary[128];
    char section[192];
    snprintf(summary,
             sizeof(summary),
             "%zu pins | %s",
             app->pins_file.pin_count,
             app->pins_path[0] != '\0' ? app->pins_path : "no pin file");
    snprintf(section,
             sizeof(section),
             "PINS | region %s",
             app->region.name ? app->region.name : "unknown");
    ui_draw_text(&app->renderer,
                 (int)(app->ui_state_bridge.pin_pane_content_rect.x + 2.0f),
                 (int)(app->ui_state_bridge.pin_pane_content_rect.y + 2.0f),
                 section,
                 text,
                 0.95f);
    ui_draw_text_clipped(&app->renderer,
                 (int)(app->ui_state_bridge.pin_pane_content_rect.x + 2.0f),
                 (int)(app->ui_state_bridge.pin_pane_content_rect.y + 22.0f),
                 summary,
                 muted,
                 0.85f,
                 (int)app->ui_state_bridge.pin_pane_content_rect.w - 8);

    SDL_FRect buttons[4] = {
        app->ui_state_bridge.pin_pane_add_rect,
        app->ui_state_bridge.pin_pane_save_rect,
        app->ui_state_bridge.pin_pane_delete_rect,
        app->ui_state_bridge.pin_pane_cancel_rect
    };
    const char *labels[4] = {"ADD", "SAVE", "DELETE", "CANCEL"};
    for (int i = 0; i < 4; ++i) {
        SDL_Color fill = palette->button_fill;
        if (i == 0 && app->ui_state_bridge.pin_add_mode_active) {
            fill = palette->button_active_primary;
        }
        renderer_set_draw_color(&app->renderer, fill.r, fill.g, fill.b, 220);
        renderer_fill_rect(&app->renderer, &buttons[i]);
        renderer_set_draw_color(&app->renderer, palette->button_outline.r, palette->button_outline.g, palette->button_outline.b, palette->button_outline.a);
        renderer_draw_rect(&app->renderer, &buttons[i]);
        ui_draw_text_clipped(&app->renderer,
                             (int)(buttons[i].x + 8.0f),
                             (int)(buttons[i].y + 4.0f),
                             labels[i],
                             text,
                             0.8f,
                             (int)buttons[i].w - 12);
    }

    app_pin_panel_draw_name_field(app, palette, text, muted);

    app_pin_panel_draw_metadata(app);

    if (app->ui_state_bridge.pin_add_mode_active) {
        hint = "Add mode is active. Move over the map for a placement preview, then right-click to create.";
    } else if (app->ui_state_bridge.pin_editor_has_draft) {
        snprintf(line,
                 sizeof(line),
                 "Draft: lat %.5f lon %.5f",
                 app->ui_state_bridge.pin_editor_draft.lat,
                 app->ui_state_bridge.pin_editor_draft.lon);
        hint = line;
    } else {
        hint = "Select, drag, Shift-click for route start, or right-click for route goal.";
    }
    ui_draw_text_clipped(&app->renderer,
                         (int)(app->ui_state_bridge.pin_pane_content_rect.x + 2.0f),
                         (int)(app->ui_state_bridge.pin_pane_type_rect.y + app->ui_state_bridge.pin_pane_type_rect.h + 44.0f),
                         hint,
                         muted,
                         0.85f,
                         (int)app->ui_state_bridge.pin_pane_content_rect.w - 8);
    if (app->ui_state_bridge.pin_editor_status[0] != '\0') {
        ui_draw_text_clipped(&app->renderer,
                             (int)(app->ui_state_bridge.pin_pane_content_rect.x + 2.0f),
                             (int)(app->ui_state_bridge.pin_pane_type_rect.y + app->ui_state_bridge.pin_pane_type_rect.h + 62.0f),
                             app->ui_state_bridge.pin_editor_status,
                             muted,
                             0.85f,
                             (int)app->ui_state_bridge.pin_pane_content_rect.w - 8);
    }

    renderer_set_draw_color(&app->renderer, palette->route_panel_fill.r, palette->route_panel_fill.g, palette->route_panel_fill.b, 220);
    renderer_fill_rect(&app->renderer, &app->ui_state_bridge.pin_pane_list_rect);
    renderer_set_draw_color(&app->renderer, palette->route_panel_outline.r, palette->route_panel_outline.g, palette->route_panel_outline.b, palette->route_panel_outline.a);
    renderer_draw_rect(&app->renderer, &app->ui_state_bridge.pin_pane_list_rect);

    if (app->pins_file.pin_count == 0u) {
        ui_draw_text(&app->renderer,
                     (int)(app->ui_state_bridge.pin_pane_list_rect.x + 10.0f),
                     (int)(app->ui_state_bridge.pin_pane_list_rect.y + 10.0f),
                     "No saved pins yet.",
                     text,
                     1.0f);
        ui_draw_text(&app->renderer,
                     (int)(app->ui_state_bridge.pin_pane_list_rect.x + 10.0f),
                     (int)(app->ui_state_bridge.pin_pane_list_rect.y + 30.0f),
                     "Use ADD mode, then right-click the map to create pins.",
                     muted,
                     0.9f);
        return;
    }

    for (int i = 0; i < app->ui_state_bridge.pin_pane_row_count; ++i) {
        int pin_index = app->ui_state_bridge.pin_pane_row_base + i;
        const MapForgePin *pin = &app->pins_file.pins[pin_index];
        SDL_FRect row = app->ui_state_bridge.pin_pane_row_rects[i];
        const bool selected = pin_index == app->ui_state_bridge.pin_selected_index;
        const bool route_start = app_pin_panel_row_has_route_start(app, pin_index);
        const bool route_goal = app_pin_panel_row_has_route_goal(app, pin_index);
        const bool drag_source = app->ui_state_bridge.pin_list_drag_active &&
                                 pin_index == app->ui_state_bridge.pin_drag_source_index;
        snprintf(line,
                 sizeof(line),
                 "%s%s%s",
                 pin->name,
                 pin->type[0] != '\0' ? " | " : "",
                 pin->type[0] != '\0' ? pin->type : "");

        renderer_set_draw_color(&app->renderer,
                                selected ? palette->button_active_primary.r : palette->chip_idle_fill.r,
                                selected ? palette->button_active_primary.g : palette->chip_idle_fill.g,
                                selected ? palette->button_active_primary.b : palette->chip_idle_fill.b,
                                drag_source ? 70 : (selected ? 130 : 110));
        renderer_fill_rect(&app->renderer, &row);
        renderer_set_draw_color(&app->renderer, palette->overlay_outline.r, palette->overlay_outline.g, palette->overlay_outline.b, 180);
        renderer_draw_rect(&app->renderer, &row);
        ui_draw_text(&app->renderer, (int)(row.x + 8.0f), (int)(row.y + 5.0f), line, text, 1.0f);

        if (route_start) {
            ui_draw_text(&app->renderer,
                         (int)(row.x + row.w - 110.0f),
                         (int)(row.y + 5.0f),
                         "START",
                         text,
                         0.85f);
        }
        if (route_goal) {
            ui_draw_text(&app->renderer,
                         (int)(row.x + row.w - 56.0f),
                         (int)(row.y + 5.0f),
                         "GOAL",
                         text,
                         0.85f);
        }
        if (pin->private_flag) {
            ui_draw_text(&app->renderer,
                         (int)(row.x + 8.0f),
                         (int)(row.y + 5.0f),
                         "PRIVATE",
                         muted,
                         0.9f);
        }
    }

    if (app->ui_state_bridge.pin_list_drag_active &&
        app->ui_state_bridge.pin_drag_preview_rect.w > 1.0f) {
        renderer_set_draw_color(&app->renderer, palette->overlay_accent.r, palette->overlay_accent.g, palette->overlay_accent.b, 255);
        renderer_fill_rect(&app->renderer, &app->ui_state_bridge.pin_drag_preview_rect);
    }
}

static void app_draw_left_pane_ingest(AppState *app,
                                      const MapForgeThemePalette *palette,
                                      SDL_Color text,
                                      SDL_Color muted) {
    if (!app || !palette) {
        return;
    }

    char line[512];
    const char *list_label = app->ingest_show_active_tab ? "ACTIVE REGIONS" : "OSM SOURCES";
    char section[192];
    snprintf(section,
             sizeof(section),
             "INGEST | region %s",
             app->region.name ? app->region.name : "unknown");

    ui_draw_text(&app->renderer,
                 (int)(app->ui_state_bridge.pin_pane_content_rect.x + 2.0f),
                 (int)(app->ui_state_bridge.pin_pane_content_rect.y + 2.0f),
                 section,
                 text,
                 0.95f);
    ui_draw_text(&app->renderer,
                 (int)(app->ui_state_bridge.pin_pane_content_rect.x + 2.0f),
                 (int)(app->ui_state_bridge.pin_pane_content_rect.y + 22.0f),
                 list_label,
                 muted,
                 0.85f);
    snprintf(line, sizeof(line), "Input root: %s", app->input_root);
    ui_draw_text_clipped(&app->renderer,
                         (int)(app->ui_state_bridge.pin_pane_content_rect.x + 2.0f),
                         (int)(app->ui_state_bridge.pin_pane_content_rect.y + 42.0f),
                         line,
                         muted,
                         0.85f,
                         (int)app->ui_state_bridge.pin_pane_content_rect.w - 8);
    snprintf(line, sizeof(line), "Status: %s", app->ingest_status);
    ui_draw_text_clipped(&app->renderer,
                         (int)(app->ui_state_bridge.pin_pane_content_rect.x + 2.0f),
                         (int)(app->ui_state_bridge.pin_pane_content_rect.y + 60.0f),
                         line,
                         muted,
                         0.85f,
                         (int)app->ui_state_bridge.pin_pane_content_rect.w - 8);

    renderer_set_draw_color(&app->renderer, palette->route_panel_fill.r, palette->route_panel_fill.g, palette->route_panel_fill.b, 220);
    renderer_fill_rect(&app->renderer, &app->ui_state_bridge.pin_pane_list_rect);
    renderer_set_draw_color(&app->renderer, palette->route_panel_outline.r, palette->route_panel_outline.g, palette->route_panel_outline.b, palette->route_panel_outline.a);
    renderer_draw_rect(&app->renderer, &app->ui_state_bridge.pin_pane_list_rect);

    if (app->ui_state_bridge.pin_pane_row_count == 0) {
        ui_draw_text(&app->renderer,
                     (int)(app->ui_state_bridge.pin_pane_list_rect.x + 10.0f),
                     (int)(app->ui_state_bridge.pin_pane_list_rect.y + 80.0f),
                     app->ingest_show_active_tab ? "No imported regions found." : "No OSM source files found.",
                     text,
                     1.0f);
        ui_draw_text(&app->renderer,
                     (int)(app->ui_state_bridge.pin_pane_list_rect.x + 10.0f),
                     (int)(app->ui_state_bridge.pin_pane_list_rect.y + 100.0f),
                     "Use existing ingest shortcuts while pane-hosted controls are rebuilt.",
                     muted,
                     0.9f);
        return;
    }

    for (int i = 0; i < app->ui_state_bridge.pin_pane_row_count; ++i) {
        SDL_FRect row = app->ui_state_bridge.pin_pane_row_rects[i];
        const char *name = app->ingest_show_active_tab ? app->ingest_active_regions[i] : app->ingest_osm_files[i];
        renderer_set_draw_color(&app->renderer, palette->chip_idle_fill.r, palette->chip_idle_fill.g, palette->chip_idle_fill.b, 110);
        renderer_fill_rect(&app->renderer, &row);
        renderer_set_draw_color(&app->renderer, palette->overlay_outline.r, palette->overlay_outline.g, palette->overlay_outline.b, 180);
        renderer_draw_rect(&app->renderer, &row);
        ui_draw_text_clipped(&app->renderer,
                             (int)(row.x + 8.0f),
                             (int)(row.y + 5.0f),
                             name,
                             text,
                             1.0f,
                             (int)row.w - 16);
    }
}

static void app_draw_left_pane_inspect(AppState *app,
                                       const MapForgeThemePalette *palette,
                                       SDL_Color text,
                                       SDL_Color muted) {
    char line[256];
    int x = 0;
    int y = 0;
    const int step = 20;
    if (!app || !palette) {
        return;
    }

    renderer_set_draw_color(&app->renderer, palette->route_panel_fill.r, palette->route_panel_fill.g, palette->route_panel_fill.b, 220);
    renderer_fill_rect(&app->renderer, &app->ui_state_bridge.pin_pane_content_rect);
    renderer_set_draw_color(&app->renderer, palette->route_panel_outline.r, palette->route_panel_outline.g, palette->route_panel_outline.b, palette->route_panel_outline.a);
    renderer_draw_rect(&app->renderer, &app->ui_state_bridge.pin_pane_content_rect);

    x = (int)(app->ui_state_bridge.pin_pane_content_rect.x + 10.0f);
    y = (int)(app->ui_state_bridge.pin_pane_content_rect.y + 10.0f);
    snprintf(line, sizeof(line), "INSPECT | region %s", app->region.name ? app->region.name : "unknown");
    ui_draw_text(&app->renderer, x, y, line, text, 1.0f);
    y += step;
    snprintf(line, sizeof(line), "Viewport: %.0fx%.0f", app->ui_state_bridge.map_viewport_rect.w, app->ui_state_bridge.map_viewport_rect.h);
    ui_draw_text(&app->renderer, x, y, line, muted, 0.9f);
    y += step;
    snprintf(line, sizeof(line), "Camera: x=%.1f y=%.1f zoom=%.2f", app->view_state_bridge.camera.x, app->view_state_bridge.camera.y, app->view_state_bridge.camera.zoom);
    ui_draw_text(&app->renderer, x, y, line, muted, 0.9f);
    y += step;
    snprintf(line, sizeof(line), "Pins path: %s", app->pins_path[0] != '\0' ? app->pins_path : "none");
    ui_draw_text_clipped(&app->renderer, x, y, line, muted, 0.9f, (int)app->ui_state_bridge.pin_pane_content_rect.w - 20);
    y += step;
    snprintf(line, sizeof(line), "Latest import: %s", app->latest_imported_region[0] != '\0' ? app->latest_imported_region : "none");
    ui_draw_text_clipped(&app->renderer, x, y, line, muted, 0.9f, (int)app->ui_state_bridge.pin_pane_content_rect.w - 20);
    y += step + 6;
    ui_draw_text(&app->renderer, x, y, "Inspect is the future host for pane-based diagnostics.", text, 0.95f);
}

void app_draw_pin_panel(AppState *app) {
    MapForgeThemePalette palette;
    const SDL_Color text = {225, 230, 240, 255};
    const SDL_Color muted = {190, 198, 210, 255};
    if (!app) {
        return;
    }

    app_pin_panel_layout(app);
    palette = app_ui_theme_palette();
    if (!app->ui_state_bridge.left_pane_open) {
        SDL_FRect handle = app->ui_state_bridge.pin_pane_closed_rect;
        if (handle.w <= 1.0f || handle.h <= 1.0f) {
            return;
        }
        renderer_set_draw_color(&app->renderer, palette.overlay_fill.r, palette.overlay_fill.g, palette.overlay_fill.b, 232);
        renderer_fill_rect(&app->renderer, &handle);
        renderer_set_draw_color(&app->renderer, palette.overlay_outline.r, palette.overlay_outline.g, palette.overlay_outline.b, palette.overlay_outline.a);
        renderer_draw_rect(&app->renderer, &handle);
        ui_draw_text(&app->renderer, (int)(handle.x + 9.0f), (int)(handle.y + 4.0f), "PINS", text, 0.9f);
        return;
    }
    if (app->ui_state_bridge.left_pane_rect.w <= 1.0f || app->ui_state_bridge.left_pane_rect.h <= 1.0f) {
        return;
    }

    renderer_set_draw_color(&app->renderer, palette.overlay_fill.r, palette.overlay_fill.g, palette.overlay_fill.b, 236);
    renderer_fill_rect(&app->renderer, &app->ui_state_bridge.left_pane_rect);
    renderer_set_draw_color(&app->renderer, palette.overlay_outline.r, palette.overlay_outline.g, palette.overlay_outline.b, palette.overlay_outline.a);
    renderer_draw_rect(&app->renderer, &app->ui_state_bridge.left_pane_rect);

    renderer_set_draw_color(&app->renderer, palette.overlay_accent.r, palette.overlay_accent.g, palette.overlay_accent.b, 220);
    SDL_FRect accent = {
        app->ui_state_bridge.left_pane_rect.x,
        app->ui_state_bridge.left_pane_rect.y,
        4.0f,
        app->ui_state_bridge.left_pane_rect.h
    };
    renderer_fill_rect(&app->renderer, &accent);

    app_draw_left_pane_header(app, &palette, text, muted);
    switch (app->ui_state_bridge.left_pane_section) {
        case APP_LEFT_PANE_SECTION_INGEST:
            app_draw_left_pane_ingest(app, &palette, text, muted);
            break;
        case APP_LEFT_PANE_SECTION_INSPECT:
            app_draw_left_pane_inspect(app, &palette, text, muted);
            break;
        case APP_LEFT_PANE_SECTION_PINS:
        default:
            app_draw_left_pane_pins(app, &palette, text, muted);
            break;
    }
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
