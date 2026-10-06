#include "app/app_internal.h"
#include "app/app_utility_workspace.h"
#include "app/app_pin_panel_internal.h"

#include <string.h>

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

static int app_left_pane_list_count(const AppState *app) {
    if (!app) {
        return 0;
    }
    switch (app->ui_state_bridge.left_pane_section) {
        case APP_LEFT_PANE_SECTION_INGEST:
            return app->ingest_show_active_tab ? app->ingest_active_count : app->ingest_osm_count;
        case APP_LEFT_PANE_SECTION_PINS:
            return app_utility_pin_count(app);
        case APP_LEFT_PANE_SECTION_INSPECT:
        default:
            return 0;
    }
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
    app->ui_state_bridge.pin_pane_summary_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_list_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_add_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_save_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_delete_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_cancel_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_name_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_type_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_color_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_private_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_hint_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_pane_status_rect = (SDL_FRect){0};
    app->ui_state_bridge.pin_drag_preview_rect = (SDL_FRect){0};
    memset(app->ui_state_bridge.pin_pane_tab_rects, 0, sizeof(app->ui_state_bridge.pin_pane_tab_rects));
    app_pin_panel_clear_rows(app);
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
    const float row_h = 34.0f;
    const float tab_gap = 8.0f;
    const float tab_h = 22.0f;
    const float body_gap = 10.0f;
    const float close_size = 18.0f;
    const float button_h = 22.0f;
    if (!app) {
        return;
    }

    app_pin_panel_clear_layout(app);

    if (!app->ui_state_bridge.left_pane_open) {
        carta_panes_layout(app);
        app->ui_state_bridge.pin_pane_closed_rect = (SDL_FRect){
            8.0f,
            APP_HEADER_HEIGHT + 8.0f,
            72.0f,
            22.0f
        };
        return;
    }

    carta_panes_layout(app);
    if (app->ui_state_bridge.left_pane_rect.w <= 1.0f ||
        app->ui_state_bridge.left_pane_rect.h <= 1.0f) {
        return;
    }

    SDL_FRect pane = app->ui_state_bridge.left_pane_rect;
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

    {
        float tab_y = app->ui_state_bridge.pin_pane_header_rect.y;
        float tab_x = app->ui_state_bridge.pin_pane_header_rect.x;
        float tab_area_w = app->ui_state_bridge.pin_pane_header_rect.w - close_size - tab_gap - 4.0f;
        float tab_w = (tab_area_w - tab_gap * (float)(APP_LEFT_PANE_SECTION_COUNT - 1)) /
                      (float)APP_LEFT_PANE_SECTION_COUNT;
        if (tab_w < 1.0f) tab_w = 1.0f;
        for (int i = 0; i < APP_LEFT_PANE_SECTION_COUNT; ++i) {
            app->ui_state_bridge.pin_pane_tab_rects[i] = (SDL_FRect){
                tab_x + (tab_w + tab_gap) * (float)i,
                tab_y,
                tab_w,
                tab_h
            };
        }
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

    app->ui_state_bridge.pin_pane_summary_rect = (SDL_FRect){
        app->ui_state_bridge.pin_pane_content_rect.x,
        app->ui_state_bridge.pin_pane_content_rect.y,
        app->ui_state_bridge.pin_pane_content_rect.w,
        34.0f
    };

    if (app->ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_PINS) {
        float body_x = app->ui_state_bridge.pin_pane_content_rect.x;
        float body_w = app->ui_state_bridge.pin_pane_content_rect.w;
        float button_gap = 8.0f;
        float button_w = (body_w - button_gap * 3.0f) / 4.0f;
        float y = app->ui_state_bridge.pin_pane_summary_rect.y + app->ui_state_bridge.pin_pane_summary_rect.h + 8.0f;
        if (button_w < 1.0f) button_w = 1.0f;
        app->ui_state_bridge.pin_pane_add_rect = (SDL_FRect){body_x, y, button_w, button_h};
        app->ui_state_bridge.pin_pane_save_rect = (SDL_FRect){body_x + button_w + button_gap, y, button_w, button_h};
        app->ui_state_bridge.pin_pane_delete_rect = (SDL_FRect){body_x + (button_w + button_gap) * 2.0f, y, button_w, button_h};
        app->ui_state_bridge.pin_pane_cancel_rect = (SDL_FRect){body_x + (button_w + button_gap) * 3.0f, y, button_w, button_h};

        y += button_h + 10.0f;
        app->ui_state_bridge.pin_pane_name_rect = (SDL_FRect){body_x, y, body_w, 24.0f};
        y += app->ui_state_bridge.pin_pane_name_rect.h + 10.0f;

        app->ui_state_bridge.pin_pane_type_rect = (SDL_FRect){
            body_x,
            y,
            (body_w - button_gap * 2.0f) / 3.0f,
            22.0f
        };
        app->ui_state_bridge.pin_pane_color_rect = (SDL_FRect){
            app->ui_state_bridge.pin_pane_type_rect.x + app->ui_state_bridge.pin_pane_type_rect.w + button_gap,
            y,
            app->ui_state_bridge.pin_pane_type_rect.w,
            22.0f
        };
        app->ui_state_bridge.pin_pane_private_rect = (SDL_FRect){
            app->ui_state_bridge.pin_pane_color_rect.x + app->ui_state_bridge.pin_pane_color_rect.w + button_gap,
            y,
            app->ui_state_bridge.pin_pane_type_rect.w,
            22.0f
        };

        y += app->ui_state_bridge.pin_pane_type_rect.h + 98.0f;
        app->ui_state_bridge.pin_pane_hint_rect = (SDL_FRect){body_x, y, body_w, 32.0f};
        y += app->ui_state_bridge.pin_pane_hint_rect.h + 4.0f;
        app->ui_state_bridge.pin_pane_status_rect = (SDL_FRect){body_x, y, body_w, 32.0f};
        y += app->ui_state_bridge.pin_pane_status_rect.h + 8.0f;
        y += 108.0f; /* Reserve route and location toolbar rows before the place list. */

        app->ui_state_bridge.pin_pane_list_rect = (SDL_FRect){
            body_x,
            y,
            body_w,
            app->ui_state_bridge.pin_pane_content_rect.y + app->ui_state_bridge.pin_pane_content_rect.h - y
        };
    } else if (app->ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_INGEST) {
        float body_x = app->ui_state_bridge.pin_pane_content_rect.x;
        float body_w = app->ui_state_bridge.pin_pane_content_rect.w;
        float button_gap = 8.0f;
        float button_w = (body_w - button_gap * 3.0f) / 4.0f;
        float y = app->ui_state_bridge.pin_pane_content_rect.y + 86.0f;
        if (button_w < 1.0f) button_w = 1.0f;
        app->ui_state_bridge.pin_pane_add_rect = (SDL_FRect){body_x, y, button_w, button_h};
        app->ui_state_bridge.pin_pane_save_rect = (SDL_FRect){body_x + button_w + button_gap, y, button_w, button_h};
        app->ui_state_bridge.pin_pane_delete_rect = (SDL_FRect){body_x + (button_w + button_gap) * 2.0f, y, button_w, button_h};
        app->ui_state_bridge.pin_pane_cancel_rect = (SDL_FRect){body_x + (button_w + button_gap) * 3.0f, y, button_w, button_h};
        y += button_h + 10.0f;
        app->ui_state_bridge.pin_pane_list_rect = (SDL_FRect){
            body_x,
            y,
            body_w,
            app->ui_state_bridge.pin_pane_content_rect.y + app->ui_state_bridge.pin_pane_content_rect.h - y
        };
    } else {
        app->ui_state_bridge.pin_pane_list_rect = (SDL_FRect){
            app->ui_state_bridge.pin_pane_content_rect.x,
            app->ui_state_bridge.pin_pane_content_rect.y + app->ui_state_bridge.pin_pane_content_rect.h,
            app->ui_state_bridge.pin_pane_content_rect.w,
            0.0f
        };
    }

    if (app->ui_state_bridge.pin_pane_list_rect.h < 0.0f) {
        app->ui_state_bridge.pin_pane_list_rect.h = 0.0f;
    }

    if (app->ui_state_bridge.pin_selected_index >= (int)app->pins_file.pin_count) {
        app->ui_state_bridge.pin_selected_index = (app->pins_file.pin_count > 0u) ? 0 : -1;
    }

    if (app->ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_PINS ||
        app->ui_state_bridge.left_pane_section == APP_LEFT_PANE_SECTION_INGEST) {
        app_pin_panel_build_rows(app, row_h);
    }
}
