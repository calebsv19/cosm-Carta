#include "app/app_internal.h"
#include "app/app_ui_internal.h"

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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
}
