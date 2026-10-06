#include "app/app_internal.h"
#include "app/app_place_tools.h"
#include <json-c/json.h>
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
    if ((299u * fill.r + 587u * fill.g + 114u * fill.b) / 1000u > 160u) text = (SDL_Color){20, 28, 38, 255};
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

// Positions collection toggles below the existing pin facts.
static SDL_FRect app_pin_collection_rect(const AppState *app, uint32_t index) {
    float width = app->ui_state_bridge.pin_pane_content_rect.w / 3.0f;
    return (SDL_FRect){app->ui_state_bridge.pin_pane_content_rect.x + (index % 3u) * width,
        app->ui_state_bridge.pin_pane_type_rect.y + app->ui_state_bridge.pin_pane_type_rect.h + 48.0f + (index / 3u) * 25.0f,
        width - 4.0f, 22.0f};
}

void app_pin_panel_draw_metadata(AppState *app) {
    char coords[128];
    char updated[96];
    const int fact_y = (int)(app->ui_state_bridge.pin_pane_type_rect.y + app->ui_state_bridge.pin_pane_type_rect.h + 8.0f);
    MapForgeThemePalette palette = app_ui_theme_palette();
    SDL_Color muted = palette.text_muted;
    SDL_Color text = palette.text_primary;
    if (!app || !app->ui_state_bridge.pin_editor_has_draft) {
        return;
    }

    const char *labels[] = {"General", "Dog Walk", "Pet Sit", "Hiking", "Visited"};
    for (uint32_t i = 0u; i < 5u; ++i) {
        SDL_FRect rect = app_pin_collection_rect(app, i);
        app_pin_metadata_draw_button(app, &rect, labels[i], map_forge_pin_in_collection(&app->ui_state_bridge.pin_editor_draft, 1u << i));
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
    for (uint32_t i = 0u; i < 5u; ++i) {
        SDL_FRect rect = app_pin_collection_rect(app, i);
        if (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h) {
            if ((i == 1u || i == 2u) && !app->ui_state_bridge.pin_editor_is_new) {
                struct json_object *services = NULL;
                json_object_object_get_ex(app->pins_file.source_json, "services", &services);
                for (size_t j = 0; services && j < json_object_array_length(services); ++j) {
                    struct json_object *item = json_object_array_get_idx(services, j), *id = NULL;
                    if (json_object_object_get_ex(item, "place_id", &id) &&
                        !strcmp(json_object_get_string(id), app->ui_state_bridge.pin_editor_draft.id)) {
                        app_place_tools_open(app, 1);
                        return true;
                    }
                }
            }
            app->ui_state_bridge.pin_editor_draft.collection_mask ^= 1u << i;
            app_pin_editor_set_status(app, "Collection membership changed. Save to retain it.");
            return true;
        }
    }
    if ((float)x >= app->ui_state_bridge.pin_pane_type_rect.x &&
        (float)x <= app->ui_state_bridge.pin_pane_type_rect.x + app->ui_state_bridge.pin_pane_type_rect.w &&
        (float)y >= app->ui_state_bridge.pin_pane_type_rect.y &&
        (float)y <= app->ui_state_bridge.pin_pane_type_rect.y + app->ui_state_bridge.pin_pane_type_rect.h) {
        app_pin_cycle_option(app->ui_state_bridge.pin_editor_draft.type,
                             sizeof(app->ui_state_bridge.pin_editor_draft.type),
                             app_pin_type_options);
        app_pin_editor_set_statusf(app,
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
        app_pin_editor_set_statusf(app,
                                   "Color set to %s.",
                                   app->ui_state_bridge.pin_editor_draft.color);
        return true;
    }
    if ((float)x >= app->ui_state_bridge.pin_pane_private_rect.x &&
        (float)x <= app->ui_state_bridge.pin_pane_private_rect.x + app->ui_state_bridge.pin_pane_private_rect.w &&
        (float)y >= app->ui_state_bridge.pin_pane_private_rect.y &&
        (float)y <= app->ui_state_bridge.pin_pane_private_rect.y + app->ui_state_bridge.pin_pane_private_rect.h) {
        app->ui_state_bridge.pin_editor_draft.private_flag = !app->ui_state_bridge.pin_editor_draft.private_flag;
        app_pin_editor_set_statusf(app,
                                   "Private flag %s.",
                                   app->ui_state_bridge.pin_editor_draft.private_flag ? "enabled" : "disabled");
        return true;
    }
    return false;
}
