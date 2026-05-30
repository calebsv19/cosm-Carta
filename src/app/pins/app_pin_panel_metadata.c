#include "app/app_internal.h"
#include "app/app_pin_panel_internal.h"
#include "app/app_ui_internal.h"

#include "ui/font.h"

#include <stdio.h>
#include <string.h>

static const char *const app_pin_type_options[] = {
    "general",
    "waypoint",
    "pet_sit",
    "route",
    NULL
};

static const char *const app_pin_color_options[] = {
    "blue",
    "green",
    "yellow",
    "red",
    "purple",
    NULL
};

static size_t app_pin_option_count(const char *const *options) {
    size_t count = 0u;
    while (options && options[count] != NULL) {
        count += 1u;
    }
    return count;
}

static void app_pin_cycle_option(char *dst,
                                 size_t dst_size,
                                 const char *const *options) {
    size_t count = app_pin_option_count(options);
    size_t next_index = 0u;
    if (!dst || dst_size == 0u || count == 0u) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(dst, options[i]) == 0) {
            next_index = (i + 1u) % count;
            snprintf(dst, dst_size, "%s", options[next_index]);
            return;
        }
    }
    snprintf(dst, dst_size, "%s", options[0]);
}

static void app_pin_metadata_draw_button(AppState *app,
                                         const SDL_FRect *rect,
                                         const char *label,
                                         bool active) {
    MapForgeThemePalette palette = app_ui_theme_palette();
    SDL_Color text = {225, 230, 240, 255};
    SDL_Color fill = active ? palette.button_active_primary : palette.button_fill;
    if (!app || !rect || rect->w <= 0.0f || rect->h <= 0.0f || !label) {
        return;
    }
    renderer_set_draw_color(&app->renderer, fill.r, fill.g, fill.b, 220);
    renderer_fill_rect(&app->renderer, rect);
    renderer_set_draw_color(&app->renderer,
                            palette.button_outline.r,
                            palette.button_outline.g,
                            palette.button_outline.b,
                            palette.button_outline.a);
    renderer_draw_rect(&app->renderer, rect);
    ui_draw_text_clipped(&app->renderer,
                         (int)(rect->x + 8.0f),
                         (int)(rect->y + 4.0f),
                         label,
                         text,
                         0.8f,
                         (int)rect->w - 12);
}

static void app_pin_metadata_draw_fact(AppState *app,
                                       int y,
                                       const char *label,
                                       const char *value,
                                       SDL_Color label_color,
                                       SDL_Color value_color) {
    int x = 0;
    int value_w = 0;
    int value_x = 0;
    int max_value_w = 0;
    if (!app || !label || !value) {
        return;
    }
    x = (int)app->ui_state_bridge.pin_pane_type_rect.x;
    max_value_w = (int)app->ui_state_bridge.pin_pane_content_rect.w - 96;
    value_w = ui_measure_text_width(value, 0.8f);
    value_x = (int)(app->ui_state_bridge.pin_pane_content_rect.x +
                    app->ui_state_bridge.pin_pane_content_rect.w - 6.0f -
                    (float)value_w);
    if (value_x < x + 72) {
        value_x = x + 72;
    }
    ui_draw_text(&app->renderer, x, y, label, label_color, 0.78f);
    ui_draw_text_clipped(&app->renderer,
                         value_x,
                         y,
                         value,
                         value_color,
                         0.8f,
                         max_value_w);
}

void app_pin_panel_draw_metadata(AppState *app) {
    char coords[128];
    char updated[96];
    const int fact_y = (int)(app->ui_state_bridge.pin_pane_type_rect.y + app->ui_state_bridge.pin_pane_type_rect.h + 8.0f);
    SDL_Color muted = {190, 198, 210, 255};
    SDL_Color text = {225, 230, 240, 255};
    if (!app || !app->ui_state_bridge.pin_editor_has_draft) {
        return;
    }

    snprintf(coords,
             sizeof(coords),
             "TYPE: %s",
             app->ui_state_bridge.pin_editor_draft.type[0] != '\0'
                 ? app->ui_state_bridge.pin_editor_draft.type
                 : "general");
    app_pin_metadata_draw_button(app, &app->ui_state_bridge.pin_pane_type_rect, coords, false);

    snprintf(coords,
             sizeof(coords),
             "COLOR: %s",
             app->ui_state_bridge.pin_editor_draft.color[0] != '\0'
                 ? app->ui_state_bridge.pin_editor_draft.color
                 : "blue");
    app_pin_metadata_draw_button(app, &app->ui_state_bridge.pin_pane_color_rect, coords, false);

    snprintf(coords,
             sizeof(coords),
             "PRIVATE: %s",
             app->ui_state_bridge.pin_editor_draft.private_flag ? "ON" : "OFF");
    app_pin_metadata_draw_button(app,
                                 &app->ui_state_bridge.pin_pane_private_rect,
                                 coords,
                                 app->ui_state_bridge.pin_editor_draft.private_flag);

    snprintf(coords,
             sizeof(coords),
             "%.5f, %.5f",
             app->ui_state_bridge.pin_editor_draft.lat,
             app->ui_state_bridge.pin_editor_draft.lon);
    app_pin_metadata_draw_fact(app, fact_y, "Coords", coords, muted, text);

    if (app->ui_state_bridge.pin_editor_draft.created_at[0] != '\0') {
        snprintf(updated,
                 sizeof(updated),
                 "%s",
                 app->ui_state_bridge.pin_editor_draft.updated_at[0] != '\0'
                     ? app->ui_state_bridge.pin_editor_draft.updated_at
                     : app->ui_state_bridge.pin_editor_draft.created_at);
        app_pin_metadata_draw_fact(app, fact_y + 18, "Updated", updated, muted, text);
    }
}

bool app_pin_panel_handle_metadata_click(AppState *app, int x, int y) {
    if (!app || !app->ui_state_bridge.pin_editor_has_draft) {
        return false;
    }
    if ((float)x >= app->ui_state_bridge.pin_pane_type_rect.x &&
        (float)x <= app->ui_state_bridge.pin_pane_type_rect.x + app->ui_state_bridge.pin_pane_type_rect.w &&
        (float)y >= app->ui_state_bridge.pin_pane_type_rect.y &&
        (float)y <= app->ui_state_bridge.pin_pane_type_rect.y + app->ui_state_bridge.pin_pane_type_rect.h) {
        app_pin_cycle_option(app->ui_state_bridge.pin_editor_draft.type,
                             sizeof(app->ui_state_bridge.pin_editor_draft.type),
                             app_pin_type_options);
        snprintf(app->ui_state_bridge.pin_editor_status,
                 sizeof(app->ui_state_bridge.pin_editor_status),
                 "Type set to %s.",
                 app->ui_state_bridge.pin_editor_draft.type);
        return true;
    }
    if ((float)x >= app->ui_state_bridge.pin_pane_color_rect.x &&
        (float)x <= app->ui_state_bridge.pin_pane_color_rect.x + app->ui_state_bridge.pin_pane_color_rect.w &&
        (float)y >= app->ui_state_bridge.pin_pane_color_rect.y &&
        (float)y <= app->ui_state_bridge.pin_pane_color_rect.y + app->ui_state_bridge.pin_pane_color_rect.h) {
        app_pin_cycle_option(app->ui_state_bridge.pin_editor_draft.color,
                             sizeof(app->ui_state_bridge.pin_editor_draft.color),
                             app_pin_color_options);
        snprintf(app->ui_state_bridge.pin_editor_status,
                 sizeof(app->ui_state_bridge.pin_editor_status),
                 "Color set to %s.",
                 app->ui_state_bridge.pin_editor_draft.color);
        return true;
    }
    if ((float)x >= app->ui_state_bridge.pin_pane_private_rect.x &&
        (float)x <= app->ui_state_bridge.pin_pane_private_rect.x + app->ui_state_bridge.pin_pane_private_rect.w &&
        (float)y >= app->ui_state_bridge.pin_pane_private_rect.y &&
        (float)y <= app->ui_state_bridge.pin_pane_private_rect.y + app->ui_state_bridge.pin_pane_private_rect.h) {
        app->ui_state_bridge.pin_editor_draft.private_flag = !app->ui_state_bridge.pin_editor_draft.private_flag;
        snprintf(app->ui_state_bridge.pin_editor_status,
                 sizeof(app->ui_state_bridge.pin_editor_status),
                 "Private flag %s.",
                 app->ui_state_bridge.pin_editor_draft.private_flag ? "enabled" : "disabled");
        return true;
    }
    return false;
}
