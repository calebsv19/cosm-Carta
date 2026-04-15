#include "app/app_internal.h"
#include "app/app_ui_internal.h"

#include "ui/font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <stdint.h>
#include <sys/stat.h>

static int app_cmp_name_rows(const void *left, const void *right) {
    const char *a = (const char *)left;
    const char *b = (const char *)right;
    return strcasecmp(a, b);
}

static bool app_has_suffix(const char *name, const char *suffix) {
    size_t len = 0u;
    size_t suffix_len = 0u;
    if (!name) {
        return false;
    }
    len = strlen(name);
    suffix_len = suffix ? strlen(suffix) : 0u;
    if (suffix_len == 0u || len < suffix_len) {
        return false;
    }
    return strcasecmp(name + len - suffix_len, suffix) == 0;
}

static bool app_has_osm_extension(const char *name) {
    if (!name) {
        return false;
    }
    return app_has_suffix(name, ".osm") ||
           app_has_suffix(name, ".osm.xml") ||
           app_has_suffix(name, ".pbf") ||
           app_has_suffix(name, ".osm.pbf");
}

static bool app_is_dir(const char *path) {
    struct stat st;
    if (!path || path[0] == '\0') {
        return false;
    }
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

static bool app_is_regular_file(const char *path) {
    struct stat st;
    if (!path || path[0] == '\0') {
        return false;
    }
    if (stat(path, &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
}

static bool app_buffer_has_token(const uint8_t *buffer, size_t buffer_size, const char *token) {
    size_t token_len = 0u;
    if (!buffer || !token) {
        return false;
    }
    token_len = strlen(token);
    if (token_len == 0u || token_len > buffer_size) {
        return false;
    }
    for (size_t i = 0; i + token_len <= buffer_size; ++i) {
        if (memcmp(buffer + i, token, token_len) == 0) {
            return true;
        }
    }
    return false;
}

static bool app_is_osm_source_file(const char *root, const char *name) {
    char path[MAPFORGE_REGION_PATH_CAPACITY];
    FILE *file = NULL;
    uint8_t probe[4096];
    size_t read_size = 0u;

    if (!root || !name) {
        return false;
    }
    if (snprintf(path, sizeof(path), "%s/%s", root, name) <= 0) {
        return false;
    }
    if (!app_is_regular_file(path)) {
        return false;
    }
    if (app_has_osm_extension(name)) {
        return true;
    }

    file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    read_size = fread(probe, 1u, sizeof(probe), file);
    fclose(file);
    if (read_size == 0u) {
        return false;
    }
    if (app_buffer_has_token(probe, read_size, "OSMHeader")) {
        return true;
    }
    if (app_buffer_has_token(probe, read_size, "<osm") ||
        app_buffer_has_token(probe, read_size, "<?xml")) {
        return true;
    }
    return false;
}

void app_ingest_rescan_sources(AppState *app) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    if (!app) {
        return;
    }
    app->ingest_osm_count = 0;
    if (!app_is_dir(app->input_root)) {
        snprintf(app->ingest_status, sizeof(app->ingest_status), "Input root not found: %s", app->input_root);
        return;
    }
    dir = opendir(app->input_root);
    if (!dir) {
        snprintf(app->ingest_status, sizeof(app->ingest_status), "Cannot open input root: %s", app->input_root);
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        if (!app_is_osm_source_file(app->input_root, entry->d_name)) {
            continue;
        }
        if (app->ingest_osm_count >= APP_INGEST_LIST_MAX) {
            break;
        }
        snprintf(app->ingest_osm_files[app->ingest_osm_count], APP_INGEST_NAME_CAP, "%s", entry->d_name);
        app->ingest_osm_count += 1;
    }
    closedir(dir);
    if (app->ingest_osm_count > 1) {
        qsort(app->ingest_osm_files,
              (size_t)app->ingest_osm_count,
              sizeof(app->ingest_osm_files[0]),
              app_cmp_name_rows);
    }
    if (app->ingest_selected_osm >= app->ingest_osm_count) {
        app->ingest_selected_osm = app->ingest_osm_count > 0 ? app->ingest_osm_count - 1 : 0;
    }
    if (app->ingest_status[0] == '\0') {
        if (app->ingest_osm_count > 0) {
            snprintf(app->ingest_status, sizeof(app->ingest_status), "Found %d OSM source file(s)", app->ingest_osm_count);
        } else {
            snprintf(app->ingest_status, sizeof(app->ingest_status), "No OSM source files in input root");
        }
    }
}

void app_ingest_rescan_active_regions(AppState *app) {
    if (!app) {
        return;
    }
    app->ingest_active_count = 0;
    int total = region_count();
    for (int i = 0; i < total && app->ingest_active_count < APP_INGEST_LIST_MAX; ++i) {
        const RegionInfo *info = region_get(i);
        if (!info || !info->name) {
            continue;
        }
        snprintf(app->ingest_active_regions[app->ingest_active_count], APP_INGEST_NAME_CAP, "%s", info->name);
        app->ingest_active_count += 1;
    }
    if (app->ingest_selected_active >= app->ingest_active_count) {
        app->ingest_selected_active = app->ingest_active_count > 0 ? app->ingest_active_count - 1 : 0;
    }
}


void app_draw_ingest_panel(AppState *app) {
    if (!app) {
        return;
    }

    memset(&app->ui_state_bridge.hud_ingest_panel_rect, 0, sizeof(app->ui_state_bridge.hud_ingest_panel_rect));
    memset(&app->ui_state_bridge.hud_ingest_collapse_rect, 0, sizeof(app->ui_state_bridge.hud_ingest_collapse_rect));
    memset(&app->ui_state_bridge.hud_ingest_handle_rect, 0, sizeof(app->ui_state_bridge.hud_ingest_handle_rect));
    memset(&app->ui_state_bridge.hud_ingest_source_tab_rect, 0, sizeof(app->ui_state_bridge.hud_ingest_source_tab_rect));
    memset(&app->ui_state_bridge.hud_ingest_active_tab_rect, 0, sizeof(app->ui_state_bridge.hud_ingest_active_tab_rect));
    memset(&app->ui_state_bridge.hud_ingest_import_rect, 0, sizeof(app->ui_state_bridge.hud_ingest_import_rect));
    memset(&app->ui_state_bridge.hud_ingest_import_all_rect, 0, sizeof(app->ui_state_bridge.hud_ingest_import_all_rect));
    memset(&app->ui_state_bridge.hud_ingest_edit_toggle_rect, 0, sizeof(app->ui_state_bridge.hud_ingest_edit_toggle_rect));
    memset(&app->ui_state_bridge.hud_ingest_folder_rect, 0, sizeof(app->ui_state_bridge.hud_ingest_folder_rect));
    memset(&app->ui_state_bridge.hud_ingest_apply_rect, 0, sizeof(app->ui_state_bridge.hud_ingest_apply_rect));
    memset(&app->ui_state_bridge.hud_ingest_row_rects, 0, sizeof(app->ui_state_bridge.hud_ingest_row_rects));
    app->ui_state_bridge.hud_ingest_row_base = 0;
    app->ui_state_bridge.hud_ingest_row_count = 0;

    if (!app->ingest_panel_open) {
        return;
    }

    MapForgeThemePalette palette = app_ui_theme_palette();
    SDL_Color text = palette.text_primary;
    SDL_Color muted = palette.text_muted;
    int line_h = ui_font_line_height(1.0f);
    if (line_h <= 0) {
        return;
    }

    float base_y = APP_HEADER_HEIGHT + 8.0f;
    if (app->ui_state_bridge.overlay.enabled) {
        float debug_bottom = APP_HEADER_HEIGHT + 8.0f;
        if (app->ui_state_bridge.hud_layer_debug_collapsed &&
            app->ui_state_bridge.hud_layer_debug_handle_rect.h > 0.0f) {
            debug_bottom = app->ui_state_bridge.hud_layer_debug_handle_rect.y +
                           app->ui_state_bridge.hud_layer_debug_handle_rect.h;
        } else if (app->ui_state_bridge.hud_layer_debug_panel_rect.h > 0.0f) {
            debug_bottom = app->ui_state_bridge.hud_layer_debug_panel_rect.y +
                           app->ui_state_bridge.hud_layer_debug_panel_rect.h;
        }
        base_y = debug_bottom + 8.0f;
    }
    /* Keep ingest panel visible even when debug HUD grows beyond viewport. */
    if (base_y > (float)app->height - 148.0f) {
        base_y = APP_HEADER_HEIGHT + 8.0f;
    }

    if (app->ui_state_bridge.hud_ingest_panel_collapsed) {
        SDL_FRect handle = {6.0f, base_y, 20.0f, 20.0f};
        app->ui_state_bridge.hud_ingest_handle_rect = handle;
        renderer_set_draw_color(&app->renderer, palette.overlay_fill.r, palette.overlay_fill.g, palette.overlay_fill.b, palette.overlay_fill.a);
        renderer_fill_rect(&app->renderer, &handle);
        renderer_set_draw_color(&app->renderer, palette.overlay_outline.r, palette.overlay_outline.g, palette.overlay_outline.b, palette.overlay_outline.a);
        renderer_draw_rect(&app->renderer, &handle);
        ui_draw_text(&app->renderer, (int)handle.x + 6, (int)handle.y + 4, ">", text, 1.0f);
        return;
    }

    float panel_x = 10.0f;
    float panel_y = base_y;
    float panel_w = 560.0f;
    float panel_w_max = (float)app->width - 20.0f;
    if (panel_w > panel_w_max) {
        panel_w = panel_w_max;
    }
    if (panel_w < 420.0f) {
        panel_w = 420.0f;
    }
    float panel_h = (float)(line_h * 11 + 96);
    float panel_h_max = (float)app->height - panel_y - 8.0f;
    if (panel_h > panel_h_max) {
        panel_h = panel_h_max;
    }
    if (panel_h < 170.0f) {
        panel_h = 170.0f;
    }
    SDL_FRect panel = {panel_x, panel_y, panel_w, panel_h};
    app->ui_state_bridge.hud_ingest_panel_rect = panel;
    SDL_FRect collapse = {panel.x + panel.w - 18.0f, panel.y + 3.0f, 14.0f, 14.0f};
    app->ui_state_bridge.hud_ingest_collapse_rect = collapse;

    renderer_set_draw_color(&app->renderer, palette.overlay_fill.r, palette.overlay_fill.g, palette.overlay_fill.b, palette.overlay_fill.a);
    renderer_fill_rect(&app->renderer, &panel);
    renderer_set_draw_color(&app->renderer, palette.overlay_outline.r, palette.overlay_outline.g, palette.overlay_outline.b, palette.overlay_outline.a);
    renderer_draw_rect(&app->renderer, &panel);
    SDL_FRect accent = {panel.x, panel.y, 3.0f, panel.h};
    renderer_set_draw_color(&app->renderer, palette.overlay_accent.r, palette.overlay_accent.g, palette.overlay_accent.b, palette.overlay_accent.a);
    renderer_fill_rect(&app->renderer, &accent);

    renderer_set_draw_color(&app->renderer, palette.overlay_fill.r, palette.overlay_fill.g, palette.overlay_fill.b, 245);
    renderer_fill_rect(&app->renderer, &collapse);
    renderer_set_draw_color(&app->renderer, palette.overlay_outline.r, palette.overlay_outline.g, palette.overlay_outline.b, palette.overlay_outline.a);
    renderer_draw_rect(&app->renderer, &collapse);
    ui_draw_text(&app->renderer, (int)collapse.x + 4, (int)collapse.y + 1, "-", text, 1.0f);

    ui_draw_text(&app->renderer,
                 (int)panel.x + 10,
                 (int)panel.y + 8,
                 "INGEST PANEL (O toggle, TAB tab, E edit path, B folder, A import all)",
                 text,
                 1.0f);

    SDL_FRect source_tab = {panel.x + 10.0f, panel.y + 26.0f, 116.0f, 20.0f};
    SDL_FRect active_tab = {panel.x + 132.0f, panel.y + 26.0f, 128.0f, 20.0f};
    app->ui_state_bridge.hud_ingest_source_tab_rect = source_tab;
    app->ui_state_bridge.hud_ingest_active_tab_rect = active_tab;
    SDL_Color source_fill = app->ingest_show_active_tab ? palette.button_fill : palette.button_active_primary;
    SDL_Color active_fill = app->ingest_show_active_tab ? palette.button_active_success : palette.button_fill;
    renderer_set_draw_color(&app->renderer, source_fill.r, source_fill.g, source_fill.b, source_fill.a);
    renderer_fill_rect(&app->renderer, &source_tab);
    renderer_set_draw_color(&app->renderer, active_fill.r, active_fill.g, active_fill.b, active_fill.a);
    renderer_fill_rect(&app->renderer, &active_tab);
    renderer_set_draw_color(&app->renderer, palette.button_outline.r, palette.button_outline.g, palette.button_outline.b, palette.button_outline.a);
    renderer_draw_rect(&app->renderer, &source_tab);
    renderer_draw_rect(&app->renderer, &active_tab);
    ui_draw_text(&app->renderer, (int)source_tab.x + 8, (int)source_tab.y + 4, "OSM SOURCES", text, 1.0f);
    ui_draw_text(&app->renderer, (int)active_tab.x + 8, (int)active_tab.y + 4, "ACTIVE REGIONS", text, 1.0f);

    SDL_FRect edit_btn = {panel.x + panel.w - 324.0f, panel.y + 26.0f, 64.0f, 20.0f};
    SDL_FRect folder_btn = {panel.x + panel.w - 254.0f, panel.y + 26.0f, 64.0f, 20.0f};
    SDL_FRect apply_btn = {panel.x + panel.w - 184.0f, panel.y + 26.0f, 64.0f, 20.0f};
    SDL_FRect import_btn = {panel.x + panel.w - 108.0f, panel.y + 26.0f, 98.0f, 20.0f};
    SDL_FRect import_all_btn = {panel.x + panel.w - 108.0f, panel.y + 50.0f, 98.0f, 20.0f};
    app->ui_state_bridge.hud_ingest_edit_toggle_rect = edit_btn;
    app->ui_state_bridge.hud_ingest_folder_rect = folder_btn;
    app->ui_state_bridge.hud_ingest_apply_rect = apply_btn;
    app->ui_state_bridge.hud_ingest_import_rect = import_btn;
    app->ui_state_bridge.hud_ingest_import_all_rect = import_all_btn;

    renderer_set_draw_color(&app->renderer, palette.button_fill.r, palette.button_fill.g, palette.button_fill.b, palette.button_fill.a);
    renderer_fill_rect(&app->renderer, &edit_btn);
    renderer_fill_rect(&app->renderer, &folder_btn);
    renderer_fill_rect(&app->renderer, &apply_btn);
    renderer_fill_rect(&app->renderer, &import_btn);
    renderer_fill_rect(&app->renderer, &import_all_btn);
    renderer_set_draw_color(&app->renderer, palette.button_outline.r, palette.button_outline.g, palette.button_outline.b, palette.button_outline.a);
    renderer_draw_rect(&app->renderer, &edit_btn);
    renderer_draw_rect(&app->renderer, &folder_btn);
    renderer_draw_rect(&app->renderer, &apply_btn);
    renderer_draw_rect(&app->renderer, &import_btn);
    renderer_draw_rect(&app->renderer, &import_all_btn);
    ui_draw_text(&app->renderer, (int)edit_btn.x + 8, (int)edit_btn.y + 4, app->ingest_edit_mode ? "EDIT*" : "EDIT", text, 1.0f);
    ui_draw_text(&app->renderer, (int)folder_btn.x + 8, (int)folder_btn.y + 4, "FOLDER", text, 1.0f);
    ui_draw_text(&app->renderer, (int)apply_btn.x + 8, (int)apply_btn.y + 4, "APPLY", text, 1.0f);
    ui_draw_text(&app->renderer, (int)import_btn.x + 8, (int)import_btn.y + 4, "IMPORT", text, 1.0f);
    ui_draw_text(&app->renderer, (int)import_all_btn.x + 8, (int)import_all_btn.y + 4, "IMPORT ALL", text, 1.0f);

    const char *path_text = app->ingest_edit_mode ? app->input_root_edit : app->input_root;
    ui_draw_text_clipped(&app->renderer, (int)panel.x + 10, (int)panel.y + 52, "Input Root:", text, 1.0f, (int)panel.w - 22);
    ui_draw_text_clipped(&app->renderer, (int)panel.x + 92, (int)panel.y + 52, path_text, muted, 1.0f, (int)panel.w - 214);
    ui_draw_text_clipped(&app->renderer, (int)panel.x + 10, (int)panel.y + 68, "Status:", text, 1.0f, (int)panel.w - 22);
    ui_draw_text_clipped(&app->renderer, (int)panel.x + 62, (int)panel.y + 68, app->ingest_status, muted, 1.0f, (int)panel.w - 74);

    float list_y = panel.y + 88.0f;
    float list_bottom_pad = 98.0f;
    bool show_import_progress = app->ingest_import_running || app->ingest_import_total_steps > 0;
    if (show_import_progress) {
        int total_steps = app->ingest_import_total_steps;
        int completed_steps = app->ingest_import_completed_steps;
        if (total_steps < 1) {
            total_steps = 1;
        }
        if (completed_steps < 0) {
            completed_steps = 0;
        }
        if (completed_steps > total_steps) {
            completed_steps = total_steps;
        }

        float progress_label_y = panel.y + 82.0f;
        float progress_track_x = panel.x + 86.0f;
        float progress_track_w = panel.w - 250.0f;
        if (progress_track_w < 120.0f) {
            progress_track_w = 120.0f;
        }
        if (progress_track_w > 220.0f) {
            progress_track_w = 220.0f;
        }
        float progress_track_h = 8.0f;
        SDL_FRect progress_track = {
            progress_track_x,
            progress_label_y + (float)((line_h - (int)progress_track_h) / 2),
            progress_track_w,
            progress_track_h
        };
        ui_draw_text_clipped(&app->renderer, (int)panel.x + 10, (int)progress_label_y, "Progress:", text, 1.0f, 74);
        renderer_set_draw_color(&app->renderer, palette.progress_bg.r, palette.progress_bg.g, palette.progress_bg.b, palette.progress_bg.a);
        renderer_fill_rect(&app->renderer, &progress_track);
        renderer_set_draw_color(&app->renderer, palette.route_panel_outline.r, palette.route_panel_outline.g, palette.route_panel_outline.b, palette.route_panel_outline.a);
        renderer_draw_rect(&app->renderer, &progress_track);

        float ratio = (float)completed_steps / (float)total_steps;
        if (ratio < 0.0f) {
            ratio = 0.0f;
        }
        if (ratio > 1.0f) {
            ratio = 1.0f;
        }
        SDL_FRect progress_fill = {progress_track.x + 1.0f, progress_track.y + 1.0f, (progress_track.w - 2.0f) * ratio, progress_track.h - 2.0f};
        if (progress_fill.w > 0.0f) {
            renderer_set_draw_color(&app->renderer, palette.progress_fill.r, palette.progress_fill.g, palette.progress_fill.b, palette.progress_fill.a);
            renderer_fill_rect(&app->renderer, &progress_fill);
        }

        if (app->ingest_import_running && ratio < 1.0f) {
            float pulse_w = progress_track.w * 0.14f;
            if (pulse_w < 20.0f) {
                pulse_w = 20.0f;
            }
            if (pulse_w > 60.0f) {
                pulse_w = 60.0f;
            }
            uint32_t cycle_px = (uint32_t)(progress_track.w + pulse_w);
            if (cycle_px < 1u) {
                cycle_px = 1u;
            }
            uint32_t pulse_step = (SDL_GetTicks() / 12u) % cycle_px;
            float pulse_start = progress_track.x + 1.0f + (float)pulse_step - pulse_w;
            float pulse_end = pulse_start + pulse_w;
            float clip_start = progress_track.x + 1.0f;
            float clip_end = progress_track.x + progress_track.w - 1.0f;
            if (pulse_end > clip_start && pulse_start < clip_end) {
                SDL_FRect pulse = {0};
                pulse.x = pulse_start > clip_start ? pulse_start : clip_start;
                pulse.w = (pulse_end < clip_end ? pulse_end : clip_end) - pulse.x;
                pulse.y = progress_track.y + 1.0f;
                pulse.h = progress_track.h - 2.0f;
                if (pulse.w > 0.0f && pulse.h > 0.0f) {
                    renderer_set_draw_color(&app->renderer, palette.overlay_accent.r, palette.overlay_accent.g, palette.overlay_accent.b, 110);
                    renderer_fill_rect(&app->renderer, &pulse);
                }
            }
        }

        char progress_text[96];
        int percent = (completed_steps * 100) / total_steps;
        snprintf(progress_text,
                 sizeof(progress_text),
                 "Import Progress: %d/%d steps (%d%%)%s",
                 completed_steps,
                 total_steps,
                 percent,
                 app->ingest_import_running ? " running" : "");
        float progress_text_x = progress_track.x + progress_track.w + 8.0f;
        int progress_text_w = (int)(panel.x + panel.w - progress_text_x - 10.0f);
        if (progress_text_w > 8) {
            ui_draw_text_clipped(&app->renderer,
                                 (int)progress_text_x,
                                 (int)progress_label_y,
                                 progress_text,
                                 muted,
                                 1.0f,
                                 progress_text_w);
        }

        list_y += 22.0f;
        list_bottom_pad += 22.0f;
    }

    SDL_FRect list = {panel.x + 10.0f, list_y, panel.w - 20.0f, panel.h - list_bottom_pad};
    renderer_set_draw_color(&app->renderer, palette.route_panel_fill.r, palette.route_panel_fill.g, palette.route_panel_fill.b, palette.route_panel_fill.a);
    renderer_fill_rect(&app->renderer, &list);
    renderer_set_draw_color(&app->renderer, palette.route_panel_outline.r, palette.route_panel_outline.g, palette.route_panel_outline.b, palette.route_panel_outline.a);
    renderer_draw_rect(&app->renderer, &list);

    int total = app->ingest_show_active_tab ? app->ingest_active_count : app->ingest_osm_count;
    int selected = app->ingest_show_active_tab ? app->ingest_selected_active : app->ingest_selected_osm;
    int visible_rows = (int)((list.h - 10.0f) / (float)(line_h + 2));
    if (visible_rows < 1) {
        visible_rows = 1;
    }
    int start = 0;
    if (selected >= visible_rows) {
        start = selected - visible_rows + 1;
    }
    if (start < 0) {
        start = 0;
    }
    app->ui_state_bridge.hud_ingest_row_base = start;

    for (int i = 0; i < visible_rows; ++i) {
        int idx = start + i;
        if (idx >= total || idx >= APP_INGEST_LIST_MAX) {
            break;
        }
        float y = list.y + 6.0f + (float)i * (float)(line_h + 2);
        SDL_FRect row = {list.x + 6.0f, y - 1.0f, list.w - 12.0f, (float)line_h + 2.0f};
        app->ui_state_bridge.hud_ingest_row_rects[i] = row;
        app->ui_state_bridge.hud_ingest_row_count = i + 1;
        if (idx == selected) {
            renderer_set_draw_color(&app->renderer, 42, 62, 86, 190);
            renderer_fill_rect(&app->renderer, &row);
            renderer_set_draw_color(&app->renderer, 90, 130, 180, 255);
            renderer_draw_rect(&app->renderer, &row);
        }
        const char *name = app->ingest_show_active_tab
            ? app->ingest_active_regions[idx]
            : app->ingest_osm_files[idx];
        ui_draw_text_clipped(&app->renderer,
                             (int)row.x + 6,
                             (int)row.y + 1,
                             name,
                             text,
                             1.0f,
                             (int)row.w - 10);
    }

    if (total == 0) {
        const char *msg = app->ingest_show_active_tab
            ? "No imported regions found"
            : "No OSM source files found in input root";
        ui_draw_text(&app->renderer, (int)list.x + 8, (int)list.y + 8, msg, muted, 1.0f);
    }
}
