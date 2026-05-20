#include "app/app_internal.h"
#include "app/app_ui_internal.h"

#include "core/time.h"
#include "ui/font.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

static bool app_pin_name_point_in_rect(int x, int y, const SDL_FRect *rect) {
    if (!rect || rect->w <= 0.0f || rect->h <= 0.0f) {
        return false;
    }
    return (float)x >= rect->x &&
           (float)x <= rect->x + rect->w &&
           (float)y >= rect->y &&
           (float)y <= rect->y + rect->h;
}

static int app_pin_name_cursor_clamp(const AppState *app, int cursor) {
    size_t len = 0u;
    if (!app) {
        return 0;
    }
    len = strnlen(app->ui_state_bridge.pin_editor_name_edit,
                  sizeof(app->ui_state_bridge.pin_editor_name_edit));
    if (cursor < 0) {
        return 0;
    }
    if ((size_t)cursor > len) {
        return (int)len;
    }
    return cursor;
}

static int app_pin_name_cursor_from_screen_x(const AppState *app, int screen_x) {
    static const char *prefix = "Name: ";
    int base_x = 0;
    int prefix_w = 0;
    int relative_x = 0;
    int best_index = 0;
    int best_delta = 0;
    size_t len = 0u;
    char prefix_buf[MAPFORGE_PIN_NAME_CAPACITY];
    if (!app) {
        return 0;
    }
    len = strnlen(app->ui_state_bridge.pin_editor_name_edit,
                  sizeof(app->ui_state_bridge.pin_editor_name_edit));
    base_x = (int)(app->ui_state_bridge.pin_pane_name_rect.x + 8.0f);
    prefix_w = ui_measure_text_width(prefix, 0.9f);
    relative_x = screen_x - (base_x + prefix_w);
    if (relative_x <= 0) {
        return 0;
    }
    best_index = (int)len;
    best_delta = relative_x;
    for (size_t i = 0; i <= len; ++i) {
        memcpy(prefix_buf, app->ui_state_bridge.pin_editor_name_edit, i);
        prefix_buf[i] = '\0';
        int w = ui_measure_text_width(prefix_buf, 0.9f);
        int delta = abs(relative_x - w);
        if (delta <= best_delta) {
            best_delta = delta;
            best_index = (int)i;
        }
    }
    return best_index;
}

bool app_pin_panel_name_edit_active(const AppState *app) {
    return app && app->ui_state_bridge.pin_name_edit_active;
}

void app_pin_panel_name_edit_deactivate(AppState *app) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.pin_name_edit_active = false;
}

void app_pin_panel_name_edit_sync_cursor_to_end(AppState *app) {
    size_t len = 0u;
    if (!app) {
        return;
    }
    len = strnlen(app->ui_state_bridge.pin_editor_name_edit,
                  sizeof(app->ui_state_bridge.pin_editor_name_edit));
    app->ui_state_bridge.pin_name_cursor_index = (int)len;
}

static void app_pin_panel_name_edit_activate(AppState *app, int screen_x) {
    if (!app) {
        return;
    }
    app->ui_state_bridge.pin_name_edit_active = true;
    app->ui_state_bridge.pin_name_cursor_index = app_pin_name_cursor_clamp(
        app,
        app_pin_name_cursor_from_screen_x(app, screen_x));
}

void app_pin_panel_draw_name_field(AppState *app,
                                   const MapForgeThemePalette *palette,
                                   SDL_Color text,
                                   SDL_Color muted) {
    static const char *prefix = "Name: ";
    int base_x = 0;
    int base_y = 0;
    int prefix_w = 0;
    int caret_x = 0;
    int caret_y = 0;
    int caret_h = 0;
    int name_w = 0;
    char caret_prefix[MAPFORGE_PIN_NAME_CAPACITY];
    const bool edit_active = app_pin_panel_name_edit_active(app);
    const bool show_caret = edit_active && fmod(time_now_seconds(), 1.0) < 0.55;
    if (!app || !palette) {
        return;
    }

    renderer_set_draw_color(&app->renderer, palette->route_panel_fill.r, palette->route_panel_fill.g, palette->route_panel_fill.b, 220);
    renderer_fill_rect(&app->renderer, &app->ui_state_bridge.pin_pane_name_rect);
    renderer_set_draw_color(&app->renderer,
                            edit_active ? palette->overlay_accent.r : palette->route_panel_outline.r,
                            edit_active ? palette->overlay_accent.g : palette->route_panel_outline.g,
                            edit_active ? palette->overlay_accent.b : palette->route_panel_outline.b,
                            edit_active ? 255 : palette->route_panel_outline.a);
    renderer_draw_rect(&app->renderer, &app->ui_state_bridge.pin_pane_name_rect);

    base_x = (int)(app->ui_state_bridge.pin_pane_name_rect.x + 8.0f);
    base_y = (int)(app->ui_state_bridge.pin_pane_name_rect.y + 5.0f);
    ui_draw_text(&app->renderer, base_x, base_y, prefix, muted, 0.9f);

    prefix_w = ui_measure_text_width(prefix, 0.9f);
    if (app->ui_state_bridge.pin_editor_name_edit[0] != '\0') {
        ui_draw_text_clipped(&app->renderer,
                             base_x + prefix_w,
                             base_y,
                             app->ui_state_bridge.pin_editor_name_edit,
                             text,
                             0.9f,
                             (int)app->ui_state_bridge.pin_pane_name_rect.w - 16 - prefix_w);
    } else if (!edit_active) {
        ui_draw_text_clipped(&app->renderer,
                             base_x + prefix_w,
                             base_y,
                             "<new pin>",
                             muted,
                             0.9f,
                             (int)app->ui_state_bridge.pin_pane_name_rect.w - 16 - prefix_w);
    }

    if (!show_caret) {
        return;
    }
    memset(caret_prefix, 0, sizeof(caret_prefix));
    memcpy(caret_prefix,
           app->ui_state_bridge.pin_editor_name_edit,
           (size_t)app_pin_name_cursor_clamp(app, app->ui_state_bridge.pin_name_cursor_index));
    name_w = ui_measure_text_width(caret_prefix, 0.9f);
    caret_x = base_x + prefix_w + name_w;
    caret_y = (int)app->ui_state_bridge.pin_pane_name_rect.y + 4;
    caret_h = (int)app->ui_state_bridge.pin_pane_name_rect.h - 8;
    SDL_FRect caret = {(float)caret_x, (float)caret_y, 2.0f, (float)caret_h};
    renderer_set_draw_color(&app->renderer, palette->overlay_accent.r, palette->overlay_accent.g, palette->overlay_accent.b, 255);
    renderer_fill_rect(&app->renderer, &caret);
}

bool app_pin_panel_handle_name_click(AppState *app, int x, int y) {
    double now = 0.0;
    bool in_rect = false;
    if (!app || !app->ui_state_bridge.pin_editor_has_draft) {
        return false;
    }
    in_rect = app_pin_name_point_in_rect(x, y, &app->ui_state_bridge.pin_pane_name_rect);
    if (!in_rect) {
        app_pin_panel_name_edit_deactivate(app);
        return false;
    }
    now = time_now_seconds();
    if (app->ui_state_bridge.pin_name_edit_active ||
        (now - app->ui_state_bridge.pin_name_last_click_time_sec) <= 0.35) {
        app_pin_panel_name_edit_activate(app, x);
        snprintf(app->ui_state_bridge.pin_editor_status,
                 sizeof(app->ui_state_bridge.pin_editor_status),
                 "Rename active. Use left/right to move the caret, Enter to save.");
    } else {
        snprintf(app->ui_state_bridge.pin_editor_status,
                 sizeof(app->ui_state_bridge.pin_editor_status),
                 "Double-click the name field to rename.");
    }
    app->ui_state_bridge.pin_name_last_click_time_sec = now;
    return true;
}

bool app_pin_panel_handle_name_runtime_inputs(AppState *app) {
    bool changed = false;
    if (!app || !app->ui_state_bridge.pin_editor_has_draft || !app_pin_panel_name_edit_active(app)) {
        return false;
    }
    app->ui_state_bridge.pin_name_cursor_index = app_pin_name_cursor_clamp(
        app,
        app->ui_state_bridge.pin_name_cursor_index);
    if (app->ui_state_bridge.input.cursor_left_pressed) {
        changed |= app_pin_name_edit_move_left(app->ui_state_bridge.pin_editor_name_edit,
                                               &app->ui_state_bridge.pin_name_cursor_index);
    }
    if (app->ui_state_bridge.input.cursor_right_pressed) {
        changed |= app_pin_name_edit_move_right(app->ui_state_bridge.pin_editor_name_edit,
                                                &app->ui_state_bridge.pin_name_cursor_index);
    }
    if (app->ui_state_bridge.input.text_input_received) {
        changed |= app_pin_name_edit_insert_text(app->ui_state_bridge.pin_editor_name_edit,
                                                 sizeof(app->ui_state_bridge.pin_editor_name_edit),
                                                 &app->ui_state_bridge.pin_name_cursor_index,
                                                 app->ui_state_bridge.input.text_input);
    }
    if (app->ui_state_bridge.input.backspace_pressed) {
        changed |= app_pin_name_edit_backspace(app->ui_state_bridge.pin_editor_name_edit,
                                               &app->ui_state_bridge.pin_name_cursor_index);
    }
    if (app->ui_state_bridge.input.enter_pressed) {
        return true;
    }
    return changed;
}
