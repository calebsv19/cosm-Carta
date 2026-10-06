#include "app/app_ui_internal.h"
#include "app/app_utility_workspace.h"
#include "ui/font.h"
#include <stdio.h>
#include <string.h>

// Names the four prepared geographic workspaces.
static const char *const workspace_labels[] = {"Map", "Dog Walking", "Hiking & History", "Data"};

void app_utility_workspace_draw(AppState *app) {
    if (!app)
        return;
    MapForgeThemePalette palette = app_ui_theme_palette();
    for (uint32_t i = 0u; i < 4u; ++i) {
        SDL_FRect rect = app_utility_workspace_rect(app, i);
        SDL_Color fill =
            app->ui_state_bridge.utility_workspace == i ? palette.button_active_primary : palette.button_fill;
        renderer_set_draw_color(&app->renderer, fill.r, fill.g, fill.b, fill.a);
        renderer_fill_rect(&app->renderer, &rect);
        SDL_Color text = (299u * fill.r + 587u * fill.g + 114u * fill.b) / 1000u > 160u
                             ? (SDL_Color){20, 28, 38, 255}
                             : (SDL_Color){235, 240, 248, 255};
        ui_draw_text_clipped(&app->renderer, (int)rect.x + 8, (int)rect.y + 5, workspace_labels[i], text,
                             0.85f, (int)rect.w - 12);
    }
    if (app->width > 800) {
        char label[192];
        snprintf(label, sizeof(label), "%d places | Ctrl+F search: %s", app_utility_pin_count(app),
                 app->ui_state_bridge.place_search_active
                     ? app->ui_state_bridge.place_search
                     : (app->ui_state_bridge.place_search[0] ? app->ui_state_bridge.place_search : "all"));
        if (app->ui_state_bridge.utility_workspace == APP_UTILITY_HIKING_HISTORY) {
            struct json_object *a = NULL;
            json_object_object_get_ex(app->pins_file.source_json, "hiking", &a);
            unsigned recordings = 0, trails = 0, peaks = 0;
            for (size_t i = 0;
                 a && json_object_is_type(a, json_type_array) && i < json_object_array_length(a); i++) {
                struct json_object *v = json_object_array_get_idx(a, i), *kind = NULL, *archived = NULL;
                json_object_object_get_ex(v, "kind", &kind);
                json_object_object_get_ex(v, "archived", &archived);
                if (json_object_get_boolean(archived) || !kind)
                    continue;
                const char *s = json_object_get_string(kind);
                recordings += !strcmp(s, "recorded");
                trails += !strcmp(s, "recommended");
                peaks += !strcmp(s, "peak");
            }
            snprintf(label, sizeof(label), "Catalog: %u recordings | %u routes | %u peaks", recordings,
                     trails, peaks);
        }
        ui_draw_text_clipped(&app->renderer, 615, 44, label, palette.text_muted, 0.8f, app->width - 625);
    }
}
