#include "app/app_place_tools_internal.h"
#include "app/history_ui.h"
#include "ui/control_surface.h"
#include "ui/font.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// Produces the responsive modal bounds.
SDL_FRect tools_rect(const AppState *app) {
    float w = fminf(800, (float)app->width - 24), h = fminf(650, (float)app->height - 92);
    return (SDL_FRect){((float)app->width - w) * .5f, 76, w, h};
}

// Draws one existing-style button/field with readable selected contrast.
static void tools_box(AppState *app, SDL_FRect r, const char *label, bool active) {
    MapForgeThemePalette p = app_ui_theme_palette();
    SDL_Color fill = active ? p.button_active_primary : p.button_fill;
    renderer_set_draw_color(&app->renderer, fill.r, fill.g, fill.b, 255);
    renderer_fill_rect(&app->renderer, &r);
    renderer_set_draw_color(&app->renderer, p.button_outline.r, p.button_outline.g, p.button_outline.b, 255);
    renderer_draw_rect(&app->renderer, &r);
    SDL_Color text = (299u * fill.r + 587u * fill.g + 114u * fill.b) / 1000u > 160u
                         ? (SDL_Color){20, 28, 38, 255}
                         : p.text_primary;
    ui_draw_text_clipped(&app->renderer, (int)r.x + 6, (int)r.y + 5, label, text, .78f, (int)r.w - 12);
}

// Draws bounded text in the modal content region.
static void tools_line(AppState *app, float x, float y, float width, const char *text) {
    ui_draw_text_clipped(&app->renderer, (int)x, (int)y, text, app_ui_theme_palette().text_primary, .8f,
                         (int)width);
}

// Returns the toolbar rectangles shared by hit testing and drawing.
SDL_FRect tools_toolbar(const AppState *app, unsigned index) {
    SDL_FRect s = app->ui_state_bridge.pin_pane_status_rect;
    float w = (s.w - 16) / 3;
    return (SDL_FRect){s.x + (index % 3) * (w + 8), s.y + s.h + 38 + (index / 3) * 26, w, 22};
}

// Draws and registers one editable text field only within visible content bounds.
static void tools_field_draw(AppState *app, int index, float x, float y, float width, const char *label,
                             const char *value) {
    SDL_FRect outer = tools_rect(app);
    float top = outer.y + 52, bottom = outer.y + outer.h - 72;
    if (index >= 68)
        return;
    tools.fields[index] = (SDL_FRect){0};
    if ((unsigned)(index + 1) > tools.field_count)
        tools.field_count = (unsigned)index + 1;
    if (y < top || y + 44 > bottom)
        return;
    tools_line(app, x, y, width, label);
    SDL_FRect box = {x, y + 18, width, 24};
    tools.fields[index] = box;
    if(tools.field==index)carta_text_draw(app,&tools.text,box,.78f);
    else tools_box(app,box,value,false);
}

// Draws/chips one explicit service selection list.
static void tools_services_draw(AppState *app, float x, float y, float width, struct json_object *kinds) {
    for (unsigned i = 0; i < 4; ++i) {
        bool active = false;
        for (size_t j = 0; kinds && j < json_object_array_length(kinds); ++j)
            if (!strcmp(service_keys[i], json_object_get_string(json_object_array_get_idx(kinds, j))))
                active = true;
        tools_box(app, (SDL_FRect){x + i * (width / 4), y, width / 4 - 4, 22}, service_labels[i], active);
    }
}

// Draws the visible location controls without changing search or place state.
void app_place_tools_draw_toolbar(AppState *app) {
    if (!app->ui_state_bridge.left_pane_open ||
        app->ui_state_bridge.left_pane_section != APP_LEFT_PANE_SECTION_PINS)
        return;
    const char *labels[] = {"ALL PLACES",   "WALKING", "SITTING", "FIT VISIBLE", "CENTER HOME",
                            "EDIT DETAILS", "SEARCH",  "BACKUPS", "CLEAR SEARCH"};
    for (unsigned i = 0; i < 9; ++i) {
        bool active = (i == 1 && app->ui_state_bridge.service_filter == 2) ||
                      (i == 2 && app->ui_state_bridge.service_filter == 4);
        if(i==6 && app->ui_state_bridge.place_search_active && !tools.kind && !history_ui_modal()) {
            carta_text_bind(&app->ui_state_bridge.search_text,app->ui_state_bridge.place_search,sizeof(app->ui_state_bridge.place_search),false);
            carta_text_draw(app,&app->ui_state_bridge.search_text,tools_toolbar(app,i),.78f);
        } else carta_ui_control_draw(app,tools_toolbar(app,i),labels[i],active,(KitUiSurfaceKey){CARTA_CONTROL_PLACES,i},.78f);
    }
}

// Draws a scrollable home form with provenance and explicit pet/service controls.
static void tools_profile_draw(AppState *app, SDL_FRect r) {
    float x = r.x + 14, w = r.w - 28, y = r.y + 54 - tools.scroll, top = r.y + 52, bottom = r.y + r.h - 72;
    char line[768];
    struct json_object *e = NULL;
    json_object_object_get_ex(tools.draft, "location_evidence", &e);
    char source[384];
    snprintf(source, sizeof(source), "%s | %s", tools_text(tools.draft, "location_quality"),
             tools_text(tools.draft, "location_source"));
    const char *texts[] = {source, tools_text(e, "input_address"), tools_text(e, "matched_address")};
    for (unsigned i = 0; i < 3; ++i) {
        snprintf(line, sizeof(line), "%s%s",
                 i == 0   ? "Placement source: "
                 : i == 1 ? "Original address: "
                          : "Map/provider address: ",
                 texts[i][0] ? texts[i] : "not recorded");
        if (y >= top && y + 18 < bottom)
            tools_line(app, x, y, w, line);
        y += 22;
    }
    for (int i = 0; i < 3; ++i) {
        const char *keys[] = {"name", "address", "notes"},
                   *labels[] = {"Home name", "Display address (does not move the pin)", "Notes"};
        tools_field_draw(app, i, x, y, w, labels[i], tools_text(tools.draft, keys[i]));
        y += 48;
    }
    struct json_object *p = NULL, *pets = NULL, *home = NULL;
    json_object_object_get_ex(tools.draft, "profile", &p);
    json_object_object_get_ex(p, "pets", &pets);
    json_object_object_get_ex(p, "home_services", &home);
    if (y >= top && y + 40 < bottom) {
        tools_line(app, x, y, w, "Home services without a specific pet");
        tools_services_draw(app, x, y + 18, w, home);
    }
    y += 48;
    if (y >= top && y + 22 < bottom) {
        tools_box(app, (SDL_FRect){x, y, 110, 22}, "ADD PET", false);
        tools_box(app, (SDL_FRect){x + 118, y, 150, 22}, "LINK EXISTING PET", tools.linking);
    }
    y += 30;
    if (tools.linking) {
        struct json_object *all = NULL;
        json_object_object_get_ex(app->pins_file.source_json, "pets", &all);
        struct json_object *pet =
            all && json_object_array_length(all)
                ? json_object_array_get_idx(all, (size_t)tools.link_index % json_object_array_length(all))
                : NULL;
        if (y >= top && y + 22 < bottom) {
            snprintf(line, sizeof(line), "%s | %s", tools_text(pet, "name"), tools_text(pet, "id"));
            tools_box(app, (SDL_FRect){x, y, w - 82, 22}, pet ? line : "No existing pets", false);
            tools_box(app, (SDL_FRect){x + w - 76, y, 76, 22}, "LINK", false);
        }
        y += 30;
    }
    for (size_t i = 0; i < json_object_array_length(pets); ++i) {
        struct json_object *pet = json_object_array_get_idx(pets, i), *kinds = NULL;
        json_object_object_get_ex(pet, "services", &kinds);
        tools_field_draw(app, 3 + (int)i * 2, x, y, w * .58f - 4, "Pet name", tools_text(pet, "name"));
        tools_field_draw(app, 4 + (int)i * 2, x + w * .58f, y, w * .42f, "Species (optional)",
                         tools_text(pet, "species"));
        y += 48;
        if (y >= top && y + 22 < bottom) {
            tools_services_draw(app, x, y, w - 90, kinds);
            tools_box(app, (SDL_FRect){x + w - 84, y, 84, 22}, "DETACH", false);
        }
        y += 34;
    }
    float fy = r.y + r.h - 62;
    tools_box(app, (SDL_FRect){x, fy, 140, 24}, "SAVE DETAILS", false);
    tools_box(app, (SDL_FRect){x + 148, fy, 120, 24}, "CANCEL", false);
}

// Draws a native saved-intent list and independently readable endpoint/staleness details.
static void tools_routes_draw(AppState *app, SDL_FRect r) {
    float x = r.x + 14, w = r.w - 28, top = r.y + 104;
    int visible = (int)(r.h - 186) / 36;
    if (visible < 1)
        visible = 1;
    tools_box(app, (SDL_FRect){x, r.y + 54, 160, 24}, tools.archived ? "INCLUDING ARCHIVED" : "ACTIVE ROUTES",
              tools.archived);
    tools_line(app, x + 170, r.y + 59, w - 180, "Select a route; opening recomputes it on the current map.");
    for (int i = 0; i < visible; ++i) {
        struct json_object *row = tools_route_row(tools.scroll + i), *intent = NULL;
        if (!row)
            break;
        json_object_object_get_ex(row, "saved_intent", &intent);
        char label[160];
        snprintf(label, sizeof(label), "%s%s", tools_int(intent, "archived") ? "[archived] " : "",
                 tools_text(intent, "name"));
        tools_box(app, (SDL_FRect){x, top + i * 36, w * .46f, 30}, label, tools.selected == tools.scroll + i);
    }
    struct json_object *row = tools_route_row(tools.selected), *intent = NULL;
    if (row)
        json_object_object_get_ex(row, "saved_intent", &intent);
    float dx = x + w * .48f, dw = w * .52f;
    char line[512];
    if (intent) {
        tools_field_draw(app, 0, dx, top, dw, "Route name", tools_text(intent, "name"));
        float y = top + 54;
        const char *names[] = {"from_place_name", "to_place_name"};
        for (unsigned i = 0; i < 2; ++i) {
            snprintf(line, sizeof(line), "%s%s", i ? "To: " : "From: ",
                     tools_text(row, names[i])[0] ? tools_text(row, names[i]) : "missing place");
            tools_line(app, dx, y, dw, line);
            y += 26;
        }
        snprintf(line, sizeof(line), "%s | %s", tools_text(intent, "mode"), tools_text(intent, "objective"));
        tools_line(app, dx, y, dw, line);
        y += 26;
        snprintf(line, sizeof(line), "Region: %s", tools_text(intent, "region"));
        tools_line(app, dx, y, dw, line);
        y += 26;
        tools_line(app, dx, y, dw,
                   tools_int(row, "coordinates_changed") ? "Endpoint coordinates changed."
                                                         : "Endpoint coordinates match saved intent.");
        y += 26;
        tools_line(app, dx, y, dw, "Graph checked only when opening.");
        y += 26;
        struct json_object *distance = NULL;
        json_object_object_get_ex(intent, "distance_m", &distance);
        snprintf(line, sizeof(line), "Saved snapshot: %.2f km", json_object_get_double(distance) / 1000);
        tools_line(app, dx, y, dw, line);
    } else
        tools_line(app, dx, top, dw, "No route selected. Saved intent is available offline.");
    float fy = r.y + r.h - 62;
    const char *labels[] = {"OPEN", "RENAME", intent && tools_int(intent, "archived") ? "RESTORE" : "ARCHIVE",
                            "PREV PAGE", "NEXT PAGE"};
    for (unsigned i = 0; i < 5; ++i)
        tools_box(app, (SDL_FRect){x + i * (w / 5), fy, w / 5 - 5, 24}, labels[i], false);
}

// Draws personal backup/export controls with an explicit restore preview boundary.
static void tools_recovery_draw(AppState *app, SDL_FRect r) {
    float x = r.x + 14, w = r.w - 28;
    tools_line(app, x, r.y + 56, w,
               "Private snapshots contain your addresses and pet records. Keep exports in trusted storage.");
    tools_line(app, x, r.y + 82, w,
               "Backups exclude map packs. Restore replaces this personal workspace after a verified "
               "recovery copy.");
    tools_field_draw(app, 0, x, r.y + 114, w, "SQLite backup to restore (absolute path)", tools.source);
    for (unsigned i = 0; i < 4; ++i) {
        const char *labels[] = {"CREATE BACKUP", "EXPORT JSON", "PREVIEW RESTORE", "RESTORE BACKUP"};
        tools_box(app, (SDL_FRect){x + i * w / 4, r.y + 180, w / 4 - 6, 26}, labels[i],
                  i == 3 && tools.restore);
    }
    if (tools.restore) {
        char line[200];
        snprintf(line, sizeof(line), "Backup: %lld places, %lld pets, %lld services, %lld saved routes",
                 (long long)tools_int(tools.restore, "places"), (long long)tools_int(tools.restore, "pets"),
                 (long long)tools_int(tools.restore, "services"),
                 (long long)tools_int(tools.restore, "route_plans"));
        tools_line(app, x, r.y + 228, w, line);
    }
    tools_line(app, x, r.y + 270, w,
               "New backup/export files are created beside places.sqlite in pins/backups.");
    tools_line(app, x, r.y + 298, w,
               "Recent snapshots: select one, then Preview Restore. No existing backup is overwritten.");
    int visible = (int)(r.h - 410) / 28;
    if (visible < 1)
        visible = 1;
    for (int i = 0;
         i < visible && tools.draft && (size_t)(tools.scroll + i) < json_object_array_length(tools.draft);
         ++i) {
        struct json_object *v = json_object_array_get_idx(tools.draft, (size_t)(tools.scroll + i));
        tools_box(app, (SDL_FRect){x, r.y + 330 + i * 28, w, 24}, tools_text(v, "name"),
                  !strcmp(tools.source, tools_text(v, "path")));
    }
}

// Draws the final input-owning overlay after map and ordinary UI.
void app_place_tools_draw(AppState *app) {
    if (!tools.kind)
        return;
    SDL_FRect r = tools_rect(app), shade = {0, 0, (float)app->width, (float)app->height};
    renderer_set_draw_color(&app->renderer, 0, 0, 0, 190);
    renderer_fill_rect(&app->renderer, &shade);
    MapForgeThemePalette p = app_ui_theme_palette();
    renderer_set_draw_color(&app->renderer, p.route_panel_fill.r, p.route_panel_fill.g, p.route_panel_fill.b,
                            255);
    renderer_fill_rect(&app->renderer, &r);
    tools_line(app, r.x + 14, r.y + 14, r.w - 100,
               tools.kind == 1   ? "HOME DETAILS & SERVICES"
               : tools.kind == 2 ? "SAVED ROUTES"
                                 : "PERSONAL BACKUP & RESTORE");
    tools_box(app, (SDL_FRect){r.x + r.w - 74, r.y + 8, 64, 26}, "CLOSE", false);
    memset(tools.fields, 0, sizeof(tools.fields));
    tools.field_count = 0;
    if (tools.kind == 1)
        tools_profile_draw(app, r);
    else if (tools.kind == 2)
        tools_routes_draw(app, r);
    else
        tools_recovery_draw(app, r);
    tools_line(app, r.x + 14, r.y + r.h - 28, r.w - 28, tools.status);
}
