#include "app/app_internal.h"
#include "app/app_place_tools.h"
#include "app/place_runtime.h"
#include "app/app_utility_workspace.h"
#include "app/app_pin_panel_internal.h"
#include "app/app_runtime_ingest_internal.h"
#include "app/app_ui_internal.h"

#include "ui/font.h"
#include "ui/text_input.h"
#include "ui/control_surface.h"

#include <stdio.h>
#include <string.h>

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

static void app_draw_left_pane_header(AppState *app,
                                      const MapForgeThemePalette *palette,
                                      SDL_Color text) {
    if (!app || !palette) {
        return;
    }

    (void)text;
    carta_ui_control_draw(app,app->ui_state_bridge.pin_pane_close_rect,"-",false,(KitUiSurfaceKey){CARTA_CONTROL_PANEL,1},.85f);
    for(unsigned i=0;i<APP_LEFT_PANE_SECTION_COUNT;i++)
        carta_ui_control_draw(app,app->ui_state_bridge.pin_pane_tab_rects[i],app_left_pane_section_label(i),
            i==(unsigned)app->ui_state_bridge.left_pane_section,(KitUiSurfaceKey){CARTA_CONTROL_PANEL,2+i},.85f);
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

    snprintf(line,
             sizeof(line),
             "PINS | region %s",
             app->region.name ? app->region.name : "unknown");
    ui_draw_text(&app->renderer,
                 (int)(app->ui_state_bridge.pin_pane_summary_rect.x + 2.0f),
                 (int)(app->ui_state_bridge.pin_pane_summary_rect.y + 2.0f),
                 line,
                 text,
                 0.95f);
    snprintf(line, sizeof(line), "%zu pins", (size_t)app_utility_pin_count(app));
    ui_draw_text(&app->renderer,
                 (int)(app->ui_state_bridge.pin_pane_summary_rect.x + 2.0f),
                 (int)(app->ui_state_bridge.pin_pane_summary_rect.y + 20.0f),
                 line,
                 muted,
                 0.82f);
    ui_draw_text_clipped(&app->renderer,
                         (int)(app->ui_state_bridge.pin_pane_summary_rect.x + 88.0f),
                         (int)(app->ui_state_bridge.pin_pane_summary_rect.y + 20.0f),
                         app->pins_path[0] != '\0' ? app->pins_path : "no pin file",
                         muted,
                         0.82f,
                         (int)app->ui_state_bridge.pin_pane_summary_rect.w - 92);

    SDL_FRect buttons[4] = {
        app->ui_state_bridge.pin_pane_add_rect,
        app->ui_state_bridge.pin_pane_save_rect,
        app->ui_state_bridge.pin_pane_delete_rect,
        app->ui_state_bridge.pin_pane_cancel_rect
    };
    const char *labels[4] = {"ADD", "SAVE", "DELETE", "CANCEL"};
    for(unsigned i=0;i<4;i++)carta_ui_control_draw(app,buttons[i],labels[i],
        i==0 && (app->ui_state_bridge.left_pane_section==APP_LEFT_PANE_SECTION_PINS?app->ui_state_bridge.pin_add_mode_active:app->ingest_import_running),
        (KitUiSurfaceKey){CARTA_CONTROL_PANEL,5+i},.8f);

    app_pin_panel_draw_name_field(app, palette, text, muted);
    app_pin_panel_draw_metadata(app);

    if (app->ui_state_bridge.pin_editor_has_draft && app->ui_state_bridge.pin_editor_draft.address[0]) {
        snprintf(line,sizeof(line),"%s | %s",app->ui_state_bridge.pin_editor_draft.address,app->ui_state_bridge.pin_editor_draft.location_quality);
        hint=line;
    } else if (app->ui_state_bridge.pin_add_mode_active) {
        hint = "Add mode is active. Right-click on the map to place a new pin.";
    } else if (app->ui_state_bridge.pin_editor_has_draft) {
        snprintf(line,
                 sizeof(line),
                 "Selected pin is ready to edit. Use the title field, metadata chips, Save, or Cancel.");
        hint = line;
    } else {
        hint = "Shift-click start; right-click goal. Ctrl+Shift+S saves route; Ctrl+Shift+O browses routes.";
    }
    ui_draw_text_clipped(&app->renderer,
                         (int)(app->ui_state_bridge.pin_pane_hint_rect.x + 2.0f),
                         (int)(app->ui_state_bridge.pin_pane_hint_rect.y + 2.0f),
                         hint,
                         muted,
                         0.82f,
                         (int)app->ui_state_bridge.pin_pane_hint_rect.w - 8);
    if (app->ui_state_bridge.pin_editor_has_draft && app->ui_state_bridge.pin_editor_draft.id[0]) {
        app_places_detail_text(app, app->ui_state_bridge.pin_editor_draft.id, line, sizeof(line));
        ui_draw_text_clipped(&app->renderer, (int)app->ui_state_bridge.pin_pane_hint_rect.x + 2,
                             (int)app->ui_state_bridge.pin_pane_hint_rect.y + 18, line, text, 0.78f,
                             (int)app->ui_state_bridge.pin_pane_hint_rect.w - 8);
    }
    if (app->ui_state_bridge.pin_editor_status[0] != '\0') {
        ui_draw_text_clipped(&app->renderer,
                             (int)(app->ui_state_bridge.pin_pane_status_rect.x + 2.0f),
                             (int)(app->ui_state_bridge.pin_pane_status_rect.y + 2.0f),
                             app->ui_state_bridge.pin_editor_status,
                             muted,
                             0.82f,
                             (int)app->ui_state_bridge.pin_pane_status_rect.w - 8);
    }

    for (uint32_t i = 0; i < 2u; ++i) {
        SDL_FRect rect = app_places_route_button_rect(app, i);
        const char *label = i == 0 ? "SAVE ROUTE" : "SAVED ROUTES";
        if (i==0 && app->ui_state_bridge.route_plan_name_active) {
            carta_text_bind(&app->ui_state_bridge.route_name_text,app->ui_state_bridge.route_plan_name,sizeof(app->ui_state_bridge.route_plan_name),false);
            carta_text_draw(app,&app->ui_state_bridge.route_name_text,rect,.8f);continue;
        }
        carta_ui_control_draw(app,rect,label,false,(KitUiSurfaceKey){CARTA_CONTROL_ROUTES,i},.8f);
    }
    app_place_tools_draw_toolbar(app);
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
        int pin_index = app_utility_pin_index(app, app->ui_state_bridge.pin_pane_row_base + i);
        const MapForgePin *pin = &app->pins_file.pins[pin_index];
        SDL_FRect row = app->ui_state_bridge.pin_pane_row_rects[i];
        const bool selected = pin_index == app->ui_state_bridge.pin_selected_index;
        const bool route_start = app_pin_panel_row_has_route_start(app, pin_index);
        const bool route_goal = app_pin_panel_row_has_route_goal(app, pin_index);
        const bool drag_source = app->ui_state_bridge.pin_list_drag_active &&
                                 pin_index == app->ui_state_bridge.pin_drag_source_index;
        int badges_w = 0;
        int right_info_w = 0;
        int title_x = (int)(row.x + 10.0f);
        int title_y = (int)(row.y + 8.0f);
        char right_info[128];

        if (route_start) {
            badges_w += 54;
        }
        if (route_goal) {
            badges_w += 52;
        }
        snprintf(right_info,
                 sizeof(right_info),
                 "%s%s%s%s%s",
                 pin->type[0] != '\0' ? pin->type : "general",
                 pin->color[0] != '\0' ? " | " : "",
                 pin->color[0] != '\0' ? pin->color : "",
                 pin->private_flag ? " | " : "",
                 pin->private_flag ? "private" : "");
        right_info_w = ui_measure_text_width(right_info, 0.78f);

        renderer_set_draw_color(&app->renderer,
                                selected ? palette->button_active_primary.r : palette->chip_idle_fill.r,
                                selected ? palette->button_active_primary.g : palette->chip_idle_fill.g,
                                selected ? palette->button_active_primary.b : palette->chip_idle_fill.b,
                                drag_source ? 70 : (selected ? 130 : 110));
        renderer_fill_rect(&app->renderer, &row);
        renderer_set_draw_color(&app->renderer, palette->overlay_outline.r, palette->overlay_outline.g, palette->overlay_outline.b, 180);
        renderer_draw_rect(&app->renderer, &row);

        ui_draw_text_clipped(&app->renderer,
                             title_x,
                             title_y,
                             pin->name,
                             text,
                             0.92f,
                             (int)row.w - 24 - right_info_w - badges_w);
        ui_draw_text_clipped(&app->renderer,
                             (int)(row.x + row.w - 10.0f - badges_w - right_info_w),
                             title_y + 1,
                             right_info,
                             muted,
                             0.78f,
                             right_info_w + 4);

        if (route_start) {
            ui_draw_text(&app->renderer,
                         (int)(row.x + row.w - 10.0f - badges_w),
                         title_y + 1,
                         "START",
                         text,
                         0.78f);
        }
        if (route_goal) {
            int goal_x = (int)(row.x + row.w - 10.0f - (route_goal ? 48 : 0));
            ui_draw_text(&app->renderer,
                         goal_x,
                         title_y + 1,
                         "GOAL",
                         text,
                         0.78f);
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
    snprintf(line,
             sizeof(line),
             "INGEST | region %s",
             app->region.name ? app->region.name : "unknown");
    ui_draw_text(&app->renderer,
                 (int)(app->ui_state_bridge.pin_pane_summary_rect.x + 2.0f),
                 (int)(app->ui_state_bridge.pin_pane_summary_rect.y + 2.0f),
                 line,
                 text,
                 0.95f);
    ui_draw_text(&app->renderer,
                 (int)(app->ui_state_bridge.pin_pane_summary_rect.x + 2.0f),
                 (int)(app->ui_state_bridge.pin_pane_summary_rect.y + 20.0f),
                 list_label,
                 muted,
                 0.85f);
    SDL_FRect root_box={app->ui_state_bridge.pin_pane_content_rect.x+2,
        app->ui_state_bridge.pin_pane_content_rect.y+40,app->ui_state_bridge.pin_pane_content_rect.w-4,24};
    if(app->ingest_edit_mode) {
        carta_text_bind(&app->ui_state_bridge.rollout.ingest_root,app->input_root_edit,sizeof(app->input_root_edit),false);
        carta_text_draw(app,&app->ui_state_bridge.rollout.ingest_root,root_box,.85f);
    } else {
        snprintf(line,sizeof(line),"Input root: %s [E to edit]",app->input_root);
        ui_draw_text_clipped(&app->renderer,(int)root_box.x,(int)root_box.y,line,muted,.85f,(int)root_box.w);
    }
    snprintf(line, sizeof(line), "Status: %s", app->ingest_status);
    ui_draw_text_clipped(&app->renderer,
                         (int)(app->ui_state_bridge.pin_pane_content_rect.x + 2.0f),
                         (int)(app->ui_state_bridge.pin_pane_content_rect.y + 68.0f),
                         line,
                         muted,
                         0.85f,
                         (int)app->ui_state_bridge.pin_pane_content_rect.w - 8);
    if (app->ingest_import_running && app->ingest_import_total_steps > 0) {
        SDL_FRect track = {
            app->ui_state_bridge.pin_pane_content_rect.x + 2.0f,
            app->ui_state_bridge.pin_pane_content_rect.y + 80.0f,
            app->ui_state_bridge.pin_pane_content_rect.w - 4.0f,
            6.0f
        };
        float ratio = (float)app->ingest_import_completed_steps / (float)app->ingest_import_total_steps;
        if (ratio < 0.0f) {
            ratio = 0.0f;
        }
        if (ratio > 1.0f) {
            ratio = 1.0f;
        }
        SDL_FRect fill = track;
        fill.w *= ratio;
        renderer_set_draw_color(&app->renderer, palette->button_outline.r, palette->button_outline.g, palette->button_outline.b, 180);
        renderer_fill_rect(&app->renderer, &track);
        renderer_set_draw_color(&app->renderer, palette->overlay_accent.r, palette->overlay_accent.g, palette->overlay_accent.b, 245);
        renderer_fill_rect(&app->renderer, &fill);
    }

    SDL_FRect buttons[4] = {
        app->ui_state_bridge.pin_pane_add_rect,
        app->ui_state_bridge.pin_pane_save_rect,
        app->ui_state_bridge.pin_pane_delete_rect,
        app->ui_state_bridge.pin_pane_cancel_rect
    };
    const char *labels_source[4] = {"IMPORT", "ALL", "FILE", "RESCAN"};
    const char *labels_active[4] = {"OPEN", "SOURCES", "FILE", "RESCAN"};
    const char **labels = app->ingest_show_active_tab ? labels_active : labels_source;
    for(unsigned i=0;i<4;i++)carta_ui_control_draw(app,buttons[i],labels[i],
        i==0 && (app->ui_state_bridge.left_pane_section==APP_LEFT_PANE_SECTION_PINS?app->ui_state_bridge.pin_add_mode_active:app->ingest_import_running),
        (KitUiSurfaceKey){CARTA_CONTROL_PANEL,5+i},.82f);

    app_place_tools_draw_toolbar(app);
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
                     app->ingest_show_active_tab ? "Import a source to create a region." : "Use FILE to select an OSM/PBF source.",
                     muted,
                     0.9f);
        return;
    }

    for (int i = 0; i < app->ui_state_bridge.pin_pane_row_count; ++i) {
        SDL_FRect row = app->ui_state_bridge.pin_pane_row_rects[i];
        const char *name = app->ingest_show_active_tab ? app->ingest_active_regions[i] : app->ingest_osm_files[i];
        const bool selected = app->ingest_show_active_tab
            ? (i == app->ingest_selected_active)
            : (i == app->ingest_selected_osm);
        bool loaded = false;
        char region_name[APP_INGEST_NAME_CAP];
        region_name[0] = '\0';
        if (!app->ingest_show_active_tab) {
            loaded = app_ingest_source_name_loaded(name, region_name, sizeof(region_name));
        }
        SDL_Color row_fill = selected ? palette->button_active_primary : palette->chip_idle_fill;
        renderer_set_draw_color(&app->renderer, row_fill.r, row_fill.g, row_fill.b, selected ? 150 : 110);
        renderer_fill_rect(&app->renderer, &row);
        renderer_set_draw_color(&app->renderer, palette->overlay_outline.r, palette->overlay_outline.g, palette->overlay_outline.b, 180);
        renderer_draw_rect(&app->renderer, &row);
        if (!app->ingest_show_active_tab && loaded) {
            ui_draw_text(&app->renderer,
                         (int)(row.x + row.w - 54.0f),
                         (int)(row.y + 5.0f),
                         "LOADED",
                         muted,
                         0.72f);
        }
        ui_draw_text_clipped(&app->renderer,
                             (int)(row.x + 8.0f),
                             (int)(row.y + 5.0f),
                             name,
                             text,
                             1.0f,
                             (int)row.w - (loaded ? 70 : 16));
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
    if (app->region.has_bounds) {
        snprintf(line, sizeof(line), "Offline: %.3f..%.3f N, %.3f..%.3f E", app->region.min_lat, app->region.max_lat, app->region.min_lon, app->region.max_lon);
        ui_draw_text_clipped(&app->renderer, x, y, line, muted, 0.8f, (int)app->ui_state_bridge.pin_pane_content_rect.w); y += step;
        snprintf(line, sizeof(line), "Zoom %u..%u | %s", app->region.tile_min_zoom, app->region.tile_max_zoom, app->region.has_tile_archive ? "offline archive" : "loose tiles");
        ui_draw_text_clipped(&app->renderer, x, y, line, muted, 0.8f, (int)app->ui_state_bridge.pin_pane_content_rect.w); y += step;
    }
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
    SDL_Color text;
    SDL_Color muted;
    if (!app) {
        return;
    }

    app_pin_panel_layout(app);
    palette = app_ui_theme_palette();
    text = palette.text_primary;
    muted = palette.text_muted;
    if (!app->ui_state_bridge.left_pane_open) {
        carta_ui_control_draw(app,app->ui_state_bridge.pin_pane_closed_rect,"PINS",false,(KitUiSurfaceKey){CARTA_CONTROL_PANEL,0},.9f);
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
    {
        SDL_FRect accent = {
            app->ui_state_bridge.left_pane_rect.x,
            app->ui_state_bridge.left_pane_rect.y,
            4.0f,
            app->ui_state_bridge.left_pane_rect.h
        };
        renderer_fill_rect(&app->renderer, &accent);
    }

    app_draw_left_pane_header(app, &palette, text);
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
