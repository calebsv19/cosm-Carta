#include "app/app_internal.h"
#include "app/app_runtime_ingest_internal.h"

#include "app/region_loader.h"
#include "core/log.h"
#include "ui/font.h"
#include "ui/shared_theme_font_adapter.h"

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;

#define APP_INGEST_IMPORT_CMD_CAPACITY 65536u

static int app_runtime_find_next_region_index(int current_index) {
    int total = region_count();
    if (total <= 0) {
        return -1;
    }
    int base_index = current_index;
    if (base_index < 0) {
        base_index = 0;
    }
    for (int step = 1; step <= total; ++step) {
        int candidate = (base_index + step) % total;
        const RegionInfo *info = region_get(candidate);
        if (info) {
            return candidate;
        }
    }
    return -1;
}

static int app_runtime_find_region_index_by_name(const char *name) {
    if (!name || name[0] == '\0') {
        return -1;
    }
    int total = region_count();
    for (int i = 0; i < total; ++i) {
        const RegionInfo *info = region_get(i);
        if (info && info->name && strcmp(info->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void app_runtime_set_ingest_status(AppState *app, const char *message) {
    if (!app || !message) {
        return;
    }
    snprintf(app->ingest_status, sizeof(app->ingest_status), "%s", message);
}

static const char *app_runtime_package_degraded_label(const RegionPackageValidationResult *validation) {
    if (!validation) {
        return "unknown";
    }
    if (!validation->ok) {
        return "invalid";
    }
    if (!validation->archive_storage) {
        return "none";
    }
    if (validation->runtime_policy_mode == TILE_SOURCE_POLICY_FILESYSTEM_ONLY) {
        return "archive_bypassed";
    }
    if (validation->archive_fallback_tree) {
        return "tree_fallback";
    }
    return "none";
}

void app_runtime_format_region_package_status(const char *region_name,
                                              const RegionPackageValidationResult *validation,
                                              char *out_status,
                                              size_t out_size) {
    const char *resolved_region_name = NULL;
    const char *storage = "unknown";
    const char *policy = "unknown";
    const char *degraded = "unknown";
    const char *contract = "unknown";
    if (!out_status || out_size == 0u) {
        return;
    }
    resolved_region_name = (region_name && region_name[0] != '\0') ? region_name : "unknown";
    if (!validation) {
        snprintf(out_status, out_size, "region=%s package=unknown", resolved_region_name);
        return;
    }
    storage = validation->archive_storage ? "archive_indexed" : "filesystem_tree";
    policy = tile_source_policy_mode_label(validation->runtime_policy_mode);
    degraded = app_runtime_package_degraded_label(validation);
    if (validation->package_contract_v1) {
        contract = "v1";
    } else if (validation->package_contract_legacy) {
        contract = "legacy";
    } else if (validation->has_package_contract) {
        contract = "custom";
    } else {
        contract = "unspecified";
    }
    snprintf(out_status,
             out_size,
             "region=%s storage=%s policy=%s degraded=%s contract=%s",
             resolved_region_name,
             storage,
             policy ? policy : "unknown",
             degraded,
             contract);
}

static void app_runtime_set_region_package_invalid_status(AppState *app,
                                                          const RegionInfo *info,
                                                          const RegionPackageValidationResult *validation) {
    const char *region_name = "unknown";
    const char *summary = "validation_failed";
    if (!app) {
        return;
    }
    if (info && info->name && info->name[0] != '\0') {
        region_name = info->name;
    }
    if (validation && validation->summary[0] != '\0') {
        summary = validation->summary;
    }
    snprintf(app->ingest_package_status,
             sizeof(app->ingest_package_status),
             "region=%s invalid=%s",
             region_name,
             summary);
}

static void app_runtime_clear_import_process_state(AppState *app) {
    if (!app) {
        return;
    }
    if (app->ingest_import_progress_path[0] != '\0') {
        (void)unlink(app->ingest_import_progress_path);
    }
    app->ingest_import_running = false;
    app->ingest_import_pid = 0;
    app->ingest_import_all = false;
    app->ingest_import_expected_count = 0;
    app->ingest_import_open_region[0] = '\0';
    app->ingest_import_total_steps = 0;
    app->ingest_import_completed_steps = 0;
    app->ingest_import_progress_path[0] = '\0';
}

static void app_runtime_poll_import_progress(AppState *app) {
    FILE *progress_file = NULL;
    int completed_steps = 0;
    int total_steps = 0;
    if (!app || !app->ingest_import_running || app->ingest_import_progress_path[0] == '\0') {
        return;
    }
    progress_file = fopen(app->ingest_import_progress_path, "r");
    if (!progress_file) {
        return;
    }
    if (fscanf(progress_file, "%d/%d", &completed_steps, &total_steps) == 2 && total_steps > 0) {
        if (completed_steps < 0) {
            completed_steps = 0;
        }
        if (completed_steps > total_steps) {
            completed_steps = total_steps;
        }
        app->ingest_import_total_steps = total_steps;
        app->ingest_import_completed_steps = completed_steps;
    }
    (void)fclose(progress_file);
}

static bool app_runtime_cmd_append(char *buffer, size_t buffer_cap, size_t *io_offset, const char *fmt, ...) {
    va_list args;
    int written = 0;
    if (!buffer || !io_offset || !fmt || *io_offset >= buffer_cap) {
        return false;
    }
    va_start(args, fmt);
    written = vsnprintf(buffer + *io_offset, buffer_cap - *io_offset, fmt, args);
    va_end(args);
    if (written < 0) {
        return false;
    }
    if (*io_offset + (size_t)written >= buffer_cap) {
        return false;
    }
    *io_offset += (size_t)written;
    return true;
}

static bool app_runtime_pick_folder_macos(char *out_path, size_t out_cap) {
#if defined(__APPLE__)
    FILE *pipe = NULL;
    char line[MAPFORGE_REGION_PATH_CAPACITY];
    if (!out_path || out_cap == 0u) {
        return false;
    }
    pipe = popen("/usr/bin/osascript -e 'POSIX path of (choose folder with prompt \"Choose Carta OSM Input Folder\")'", "r");
    if (!pipe) {
        return false;
    }
    if (!fgets(line, sizeof(line), pipe)) {
        (void)pclose(pipe);
        return false;
    }
    (void)pclose(pipe);
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0') {
        return false;
    }
    snprintf(out_path, out_cap, "%s", line);
    return true;
#else
    (void)out_path;
    (void)out_cap;
    return false;
#endif
}

static bool app_runtime_pick_osm_file_macos(char *out_path, size_t out_cap) {
#if defined(__APPLE__)
    FILE *pipe = NULL;
    char line[MAPFORGE_REGION_PATH_CAPACITY];
    if (!out_path || out_cap == 0u) {
        return false;
    }
    pipe = popen("/usr/bin/osascript -e 'POSIX path of (choose file with prompt \"Choose Carta OSM Source File\")'", "r");
    if (!pipe) {
        return false;
    }
    if (!fgets(line, sizeof(line), pipe)) {
        (void)pclose(pipe);
        return false;
    }
    (void)pclose(pipe);
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0') {
        return false;
    }
    snprintf(out_path, out_cap, "%s", line);
    return true;
#else
    (void)out_path;
    (void)out_cap;
    return false;
#endif
}

static bool app_runtime_has_suffix(const char *name, const char *suffix) {
    size_t name_len = 0u;
    size_t suffix_len = 0u;
    if (!name || !suffix) {
        return false;
    }
    name_len = strlen(name);
    suffix_len = strlen(suffix);
    if (suffix_len == 0u || name_len < suffix_len) {
        return false;
    }
    return strcasecmp(name + (name_len - suffix_len), suffix) == 0;
}

void app_ingest_region_name_from_source(const char *osm_name, char *out_name, size_t out_cap) {
    size_t wi = 0u;
    if (!out_name || out_cap == 0u) {
        return;
    }
    out_name[0] = '\0';
    if (!osm_name || osm_name[0] == '\0') {
        snprintf(out_name, out_cap, "region");
        return;
    }
    size_t len = strlen(osm_name);
    if (app_runtime_has_suffix(osm_name, ".osm.pbf")) {
        len -= strlen(".osm.pbf");
    } else if (app_runtime_has_suffix(osm_name, ".osm.xml")) {
        len -= strlen(".osm.xml");
    } else if (app_runtime_has_suffix(osm_name, ".pbf")) {
        len -= strlen(".pbf");
    } else if (app_runtime_has_suffix(osm_name, ".osm")) {
        len -= strlen(".osm");
    }
    for (size_t i = 0; i < len && wi + 1u < out_cap; ++i) {
        unsigned char c = (unsigned char)osm_name[i];
        if (isalnum(c)) {
            out_name[wi++] = (char)tolower((int)c);
        } else if (c == '-' || c == '_' || c == '.') {
            out_name[wi++] = '_';
        }
    }
    if (wi == 0u) {
        snprintf(out_name, out_cap, "region");
        return;
    }
    out_name[wi] = '\0';
}

static bool app_runtime_select_source_path(AppState *app, const char *source_path) {
    char parent[MAPFORGE_REGION_PATH_CAPACITY];
    char name[APP_INGEST_NAME_CAP];
    const char *slash = NULL;
    size_t parent_len = 0u;
    if (!app || !source_path || source_path[0] == '\0') {
        return false;
    }
    slash = strrchr(source_path, '/');
    if (!slash || slash == source_path || slash[1] == '\0') {
        app_runtime_set_ingest_status(app, "Selected source path is invalid");
        return false;
    }
    parent_len = (size_t)(slash - source_path);
    if (parent_len >= sizeof(parent) || strlen(slash + 1) >= sizeof(name)) {
        app_runtime_set_ingest_status(app, "Selected source path is too long");
        return false;
    }
    memcpy(parent, source_path, parent_len);
    parent[parent_len] = '\0';
    snprintf(name, sizeof(name), "%s", slash + 1);

    snprintf(app->input_root, sizeof(app->input_root), "%s", parent);
    snprintf(app->input_root_edit, sizeof(app->input_root_edit), "%s", parent);
    app_ingest_rescan_sources(app);
    for (int i = 0; i < app->ingest_osm_count; ++i) {
        if (strcmp(app->ingest_osm_files[i], name) == 0) {
            app->ingest_selected_osm = i;
            app->ingest_show_active_tab = false;
            snprintf(app->ingest_status, sizeof(app->ingest_status), "Selected source: %s", name);
            return true;
        }
    }
    snprintf(app->ingest_status, sizeof(app->ingest_status), "Selected file is not a supported OSM source: %s", name);
    return false;
}

static bool app_runtime_shell_quote(const char *src, char *out, size_t out_cap) {
    size_t w = 0u;
    if (!src || !out || out_cap < 3u) {
        return false;
    }
    out[w++] = '\'';
    for (size_t i = 0; src[i] != '\0'; ++i) {
        if (w + 5u >= out_cap) {
            return false;
        }
        if (src[i] == '\'') {
            out[w++] = '\'';
            out[w++] = '\\';
            out[w++] = '\'';
            out[w++] = '\'';
        } else {
            out[w++] = src[i];
        }
    }
    if (w + 2u > out_cap) {
        return false;
    }
    out[w++] = '\'';
    out[w] = '\0';
    return true;
}

static bool app_runtime_import_tool_try_dir(const char *dir,
                                            const char *tool_name,
                                            char *out_path,
                                            size_t out_cap) {
    int n = 0;
    if (!dir || !tool_name || !out_path || out_cap == 0u || dir[0] == '\0') {
        return false;
    }
    n = snprintf(out_path, out_cap, "%s/%s", dir, tool_name);
    if (n < 0 || (size_t)n >= out_cap) {
        return false;
    }
    return access(out_path, X_OK) == 0;
}

static bool app_runtime_resolve_import_tool(const char *tool_name,
                                            char *out_path,
                                            size_t out_cap) {
    const char *tools_dir_env = getenv("MAPFORGE_IMPORT_TOOLS_DIR");
    const char *dev_root_env = getenv("MAPFORGE_DEV_ROOT");
    const char *home = getenv("HOME");
    char candidate_dir[MAPFORGE_REGION_PATH_CAPACITY];
    int n = 0;

    if (app_runtime_import_tool_try_dir(tools_dir_env, tool_name, out_path, out_cap)) {
        return true;
    }
    if (app_runtime_import_tool_try_dir("./build/tools", tool_name, out_path, out_cap)) {
        return true;
    }
    if (dev_root_env && dev_root_env[0] != '\0') {
        n = snprintf(candidate_dir, sizeof(candidate_dir), "%s/build/tools", dev_root_env);
        if (n > 0 && (size_t)n < sizeof(candidate_dir) &&
            app_runtime_import_tool_try_dir(candidate_dir, tool_name, out_path, out_cap)) {
            return true;
        }
    }
    if (home && home[0] != '\0') {
        n = snprintf(candidate_dir, sizeof(candidate_dir), "%s/Desktop/CodeWork/map_forge/build/tools", home);
        if (n > 0 && (size_t)n < sizeof(candidate_dir) &&
            app_runtime_import_tool_try_dir(candidate_dir, tool_name, out_path, out_cap)) {
            return true;
        }
    }
    return false;
}

static bool app_runtime_open_region_index(AppState *app, int region_index) {
    if (!app) {
        return false;
    }
    const RegionInfo *info = region_get(region_index);
    RegionPackageValidationResult validation = {0};
    if (!info) {
        return false;
    }
    if (!region_validate_package(info, &validation)) {
        log_error("Region package validation failed for '%s': %s",
                  info->name ? info->name : "unknown",
                  validation.summary);
        app_runtime_set_region_package_invalid_status(app, info, &validation);
        snprintf(app->ingest_status,
                 sizeof(app->ingest_status),
                 "Failed to open region package: %s",
                 app->ingest_package_status);
        return false;
    }

    app->region_index = region_index;
    app->region = *info;
    if (!region_load_meta(info, &app->region)) {
        log_error("Failed to load region metadata for '%s' after validation", info->name ? info->name : "unknown");
        app_runtime_set_ingest_status(app, "region metadata load failed");
        return false;
    }
    region_log_archive_rollup_summary(&app->region, "switch");
    if (app->region.tiles_dir[0] == '\0') {
        log_error("Failed to resolve tiles directory for region: %s", app->region.name);
        return false;
    }
    log_info("Region '%s' runtime source policy=%s storage=%s archive=%s",
             app->region.name ? app->region.name : "unknown",
             tile_source_policy_mode_label(app->region.tile_source.policy_mode),
             tile_storage_kind_label(app->region.tile_source.storage_kind),
             app->region.has_tile_archive ? "yes" : "no");
    if (validation.archive_storage && validation.archive_fallback_tree) {
        log_info("Region '%s' runtime source degraded: tree fallback enabled (policy=%s archive_reader_supported=%s)",
                 app->region.name ? app->region.name : "unknown",
                 tile_source_policy_mode_label(app->region.tile_source.policy_mode),
                 validation.archive_reader_supported ? "yes" : "no");
    }
    tile_source_runtime_stats_reset();
    app_worker_contract_bump_world_generation(app);
    app_worker_contract_bump_tile_generation(app);
    app_clear_tile_queue(app);
    tile_loader_shutdown(&app->tile_state_bridge.tile_loader);
    vk_tile_cache_clear_with_renderer(&app->tile_state_bridge.vk_tile_cache, app->renderer.vk);
    for (size_t i = 0; i < TILE_LAYER_COUNT; ++i) {
        tile_manager_shutdown(&app->tile_state_bridge.tile_managers[i]);
        tile_manager_init_with_source_for_layer(&app->tile_state_bridge.tile_managers[i],
                                                256,
                                                &app->region.tile_source,
                                                (TileLayerKind)i);
    }
    tile_loader_init_with_source(&app->tile_state_bridge.tile_loader, &app->region.tile_source);
    app_tile_viewport_invalidate(app);
    app->tile_state_bridge.loading_expected = 0;
    app->tile_state_bridge.loading_done = 0;
    app->tile_state_bridge.loading_no_data_time = 0.0f;
    if (!app_load_route_graph(app)) {
        log_error("Route graph load kickoff failed for region '%s'; skipping route interactions in this region.", app->region.name);
    }
    app_playback_reset(app);
    memset(&app->route_state_bridge.hover_anchor, 0, sizeof(app->route_state_bridge.hover_anchor));
    memset(&app->route_state_bridge.start_anchor, 0, sizeof(app->route_state_bridge.start_anchor));
    memset(&app->route_state_bridge.goal_anchor, 0, sizeof(app->route_state_bridge.goal_anchor));
    app->view_state_bridge.building_zoom_bias = app_building_zoom_bias_for_region(&app->region);
    app->view_state_bridge.road_zoom_bias = app_road_zoom_bias_for_region(&app->region);
    app_center_camera_on_region(&app->view_state_bridge.camera, &app->region, app->width, app->height);
    app_reload_pins_state(app);
    app_runtime_format_region_package_status(app->region.name, &validation, app->ingest_package_status, sizeof(app->ingest_package_status));
    snprintf(app->ingest_status, sizeof(app->ingest_status), "Opened region package: %s", app->ingest_package_status);
    snprintf(app->latest_imported_region, sizeof(app->latest_imported_region), "%s", app->region.name ? app->region.name : "");
    return true;
}

static bool app_runtime_open_region_by_name(AppState *app, const char *name) {
    int idx = app_runtime_find_region_index_by_name(name);
    if (idx < 0) {
        return false;
    }
    return app_runtime_open_region_index(app, idx);
}

bool app_ingest_open_selected_active_region(AppState *app) {
    const char *region_name = NULL;
    if (!app) {
        return false;
    }
    if (app->ingest_selected_active < 0 || app->ingest_selected_active >= app->ingest_active_count) {
        return false;
    }
    region_name = app->ingest_active_regions[app->ingest_selected_active];
    if (region_name[0] == '\0') {
        return false;
    }
    if (!app_runtime_open_region_by_name(app, region_name)) {
        app_runtime_set_ingest_status(app, "Failed to open selected region");
        return false;
    }
    snprintf(app->latest_imported_region, sizeof(app->latest_imported_region), "%s", region_name);
    return true;
}

bool app_ingest_source_name_loaded(const char *osm_name, char *out_region, size_t out_region_cap) {
    char region_name[APP_INGEST_NAME_CAP];
    if (!osm_name || osm_name[0] == '\0') {
        return false;
    }
    app_ingest_region_name_from_source(osm_name, region_name, sizeof(region_name));
    if (out_region && out_region_cap > 0u) {
        snprintf(out_region, out_region_cap, "%s", region_name);
    }
    return app_runtime_find_region_index_by_name(region_name) >= 0;
}

bool app_ingest_selected_source_loaded(const AppState *app, char *out_region, size_t out_region_cap) {
    if (!app || app->ingest_selected_osm < 0 || app->ingest_selected_osm >= app->ingest_osm_count) {
        return false;
    }
    return app_ingest_source_name_loaded(app->ingest_osm_files[app->ingest_selected_osm],
                                         out_region,
                                         out_region_cap);
}

bool app_ingest_open_or_import_selected_source(AppState *app) {
    char region_name[APP_INGEST_NAME_CAP];
    if (!app || app->ingest_selected_osm < 0 || app->ingest_selected_osm >= app->ingest_osm_count) {
        app_runtime_set_ingest_status(app, "No OSM source selected");
        return true;
    }
    if (app_ingest_selected_source_loaded(app, region_name, sizeof(region_name))) {
        if (app_runtime_open_region_by_name(app, region_name)) {
            app->ingest_show_active_tab = true;
            app_ingest_rescan_active_regions(app);
            for (int i = 0; i < app->ingest_active_count; ++i) {
                if (strcmp(app->ingest_active_regions[i], region_name) == 0) {
                    app->ingest_selected_active = i;
                    break;
                }
            }
            return true;
        }
        app_runtime_set_ingest_status(app, "Loaded source region failed to open");
        return true;
    }
    return app_ingest_import_selected_osm(app, false);
}

static void app_runtime_poll_import_process(AppState *app) {
    int status = 0;
    pid_t waited = 0;
    bool import_all = false;
    int expected_count = 0;
    char opened_region[APP_INGEST_NAME_CAP];
    if (!app || !app->ingest_import_running) {
        return;
    }
    app_runtime_poll_import_progress(app);
    waited = waitpid((pid_t)app->ingest_import_pid, &status, WNOHANG);
    if (waited == 0) {
        return;
    }
    import_all = app->ingest_import_all;
    expected_count = app->ingest_import_expected_count;
    snprintf(opened_region, sizeof(opened_region), "%s", app->ingest_import_open_region);
    app_runtime_clear_import_process_state(app);

    if (waited < 0) {
        app_runtime_set_ingest_status(app, "Import wait failed");
        return;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        app_ingest_rescan_active_regions(app);
        if (opened_region[0] != '\0' && app_runtime_open_region_by_name(app, opened_region)) {
            snprintf(app->latest_imported_region, sizeof(app->latest_imported_region), "%s", opened_region);
            snprintf(app->ingest_status, sizeof(app->ingest_status), "Imported and opened region package: %s", app->ingest_package_status);
        } else if (expected_count > 0) {
            if (import_all) {
                snprintf(app->ingest_status, sizeof(app->ingest_status), "Imported %d region(s)", expected_count);
            } else {
                app_runtime_set_ingest_status(app, "Import completed");
            }
        }
        return;
    }
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        snprintf(app->ingest_status, sizeof(app->ingest_status), "Import failed (exit=%d; see /tmp/mapforge_*_import.log)", exit_code);
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        snprintf(app->ingest_status, sizeof(app->ingest_status), "Import interrupted by signal %d", sig);
    } else {
        app_runtime_set_ingest_status(app, "Import failed");
    }
}

void app_runtime_ingest_shutdown(AppState *app) {
    pid_t pid = 0;
    if (!app) {
        return;
    }
    if (!app->ingest_import_running) {
        app_runtime_clear_import_process_state(app);
        return;
    }
    pid = (pid_t)app->ingest_import_pid;
    if (pid > 0) {
        (void)kill(pid, SIGTERM);
        (void)waitpid(pid, NULL, 0);
    }
    app_runtime_clear_import_process_state(app);
}

bool app_ingest_import_selected_osm(AppState *app, bool import_all) {
    char region_tool[MAPFORGE_REGION_PATH_CAPACITY];
    char graph_tool[MAPFORGE_REGION_PATH_CAPACITY];
    char q_region_tool[MAPFORGE_REGION_PATH_CAPACITY * 2];
    char q_graph_tool[MAPFORGE_REGION_PATH_CAPACITY * 2];
    char progress_path[MAPFORGE_REGION_PATH_CAPACITY];
    char q_progress_path[MAPFORGE_REGION_PATH_CAPACITY * 2];
    char cmd[APP_INGEST_IMPORT_CMD_CAPACITY];
    size_t cmd_offset = 0u;
    pid_t pid = 0;
    if (!app || app->input_root[0] == '\0') {
        return false;
    }
    app_runtime_poll_import_process(app);
    if (app->ingest_import_running) {
        app_runtime_set_ingest_status(app, "Import already running");
        return true;
    }
    if (app->ingest_osm_count <= 0) {
        app_runtime_set_ingest_status(app, "No OSM source file available for import");
        return true;
    }
    if (!app_runtime_resolve_import_tool("mapforge_region", region_tool, sizeof(region_tool)) ||
        !app_runtime_resolve_import_tool("mapforge_graph", graph_tool, sizeof(graph_tool))) {
        app_runtime_set_ingest_status(app, "Import tools missing: run `make tools graph` or set MAPFORGE_IMPORT_TOOLS_DIR");
        return true;
    }
    if (!app_runtime_shell_quote(region_tool, q_region_tool, sizeof(q_region_tool)) ||
        !app_runtime_shell_quote(graph_tool, q_graph_tool, sizeof(q_graph_tool))) {
        app_runtime_set_ingest_status(app, "Import tool path is too long");
        return true;
    }

    int first = import_all ? 0 : app->ingest_selected_osm;
    int last = import_all ? app->ingest_osm_count : (app->ingest_selected_osm + 1);
    if (first < 0) {
        first = 0;
    }
    if (last > app->ingest_osm_count) {
        last = app->ingest_osm_count;
    }
    if (first >= last) {
        app_runtime_set_ingest_status(app, "No OSM source file available for import");
        return true;
    }

    char opened_region[APP_INGEST_NAME_CAP] = {0};
    int imported_count = 0;
    int total_steps = (last - first) * 2;
    struct timespec now_ts;
    memset(&now_ts, 0, sizeof(now_ts));
    (void)clock_gettime(CLOCK_REALTIME, &now_ts);
    int progress_path_len = snprintf(progress_path,
                                     sizeof(progress_path),
                                     "/tmp/mapforge_ingest_progress_%d_%ld_%ld.txt",
                                     (int)getpid(),
                                     (long)now_ts.tv_sec,
                                     (long)now_ts.tv_nsec);
    if (progress_path_len <= 0 || (size_t)progress_path_len >= sizeof(progress_path)) {
        app_runtime_set_ingest_status(app, "Import progress setup failed");
        return true;
    }
    if (!app_runtime_shell_quote(progress_path, q_progress_path, sizeof(q_progress_path))) {
        app_runtime_set_ingest_status(app, "Import progress path is too long");
        return true;
    }
    if (!app_runtime_cmd_append(cmd,
                                sizeof(cmd),
                                &cmd_offset,
                                "set -e; progress_file=%s; progress_total=%d; progress_done=0; "
                                "printf '%%s/%%s\\n' \"$progress_done\" \"$progress_total\" > \"$progress_file\"; ",
                                q_progress_path,
                                total_steps)) {
        app_runtime_set_ingest_status(app, "Import command setup failed");
        return true;
    }
    for (int i = first; i < last; ++i) {
        char osm_name[APP_INGEST_NAME_CAP];
        char region_name[APP_INGEST_NAME_CAP];
        char osm_path[MAPFORGE_REGION_PATH_CAPACITY];
        char out_dir[MAPFORGE_REGION_PATH_CAPACITY];
        char q_osm[MAPFORGE_REGION_PATH_CAPACITY * 2];
        char q_out[MAPFORGE_REGION_PATH_CAPACITY * 2];
        char q_region[APP_INGEST_NAME_CAP * 2];

        snprintf(osm_name, sizeof(osm_name), "%s", app->ingest_osm_files[i]);
        app_ingest_region_name_from_source(osm_name, region_name, sizeof(region_name));
        snprintf(osm_path, sizeof(osm_path), "%s/%s", app->input_root, osm_name);
        snprintf(out_dir, sizeof(out_dir), "%s/%s", region_data_root(), region_name);
        if (!app_runtime_shell_quote(osm_path, q_osm, sizeof(q_osm)) ||
            !app_runtime_shell_quote(out_dir, q_out, sizeof(q_out)) ||
            !app_runtime_shell_quote(region_name, q_region, sizeof(q_region))) {
            app_runtime_set_ingest_status(app, "Import path is too long");
            return true;
        }

        if (!app_runtime_cmd_append(cmd,
                                    sizeof(cmd),
                                    &cmd_offset,
                                    "%s --replace --region %s --osm %s --out %s --min-z 10 --max-z 18 >/tmp/mapforge_region_import.log 2>&1",
                                    q_region_tool,
                                    q_region,
                                    q_osm,
                                    q_out)) {
            app_runtime_set_ingest_status(app, "Import command is too long");
            return true;
        }
        if (!app_runtime_cmd_append(cmd,
                                    sizeof(cmd),
                                    &cmd_offset,
                                    " && progress_done=$((progress_done+1)) && printf '%%s/%%s\\n' \"$progress_done\" \"$progress_total\" > \"$progress_file\"")) {
            app_runtime_set_ingest_status(app, "Import command is too long");
            return true;
        }
        if (!app_runtime_cmd_append(cmd,
                                    sizeof(cmd),
                                    &cmd_offset,
                                    " && %s --replace --region %s --osm %s --out %s >/tmp/mapforge_graph_import.log 2>&1",
                                    q_graph_tool,
                                    q_region,
                                    q_osm,
                                    q_out)) {
            app_runtime_set_ingest_status(app, "Import command is too long");
            return true;
        }
        if (!app_runtime_cmd_append(cmd,
                                    sizeof(cmd),
                                    &cmd_offset,
                                    " && progress_done=$((progress_done+1)) && printf '%%s/%%s\\n' \"$progress_done\" \"$progress_total\" > \"$progress_file\"")) {
            app_runtime_set_ingest_status(app, "Import command is too long");
            return true;
        }
        if (i + 1 < last && !app_runtime_cmd_append(cmd, sizeof(cmd), &cmd_offset, " && ")) {
            app_runtime_set_ingest_status(app, "Import command is too long");
            return true;
        }

        snprintf(opened_region, sizeof(opened_region), "%s", region_name);
        imported_count += 1;
    }
    if (cmd_offset >= sizeof(cmd)) {
        app_runtime_set_ingest_status(app, "Import command is too long");
        return true;
    }
    char *argv[] = {"/bin/sh", "-c", cmd, NULL};
    int spawn_rc = posix_spawn(&pid, "/bin/sh", NULL, NULL, argv, environ);
    if (spawn_rc != 0) {
        snprintf(app->ingest_status, sizeof(app->ingest_status), "Failed to start import: %s", strerror(spawn_rc));
        return true;
    }

    app->ingest_import_running = true;
    app->ingest_import_pid = (int)pid;
    app->ingest_import_all = import_all;
    app->ingest_import_expected_count = imported_count;
    snprintf(app->ingest_import_open_region, sizeof(app->ingest_import_open_region), "%s", opened_region);
    app->ingest_import_total_steps = total_steps;
    app->ingest_import_completed_steps = 0;
    snprintf(app->ingest_import_progress_path, sizeof(app->ingest_import_progress_path), "%s", progress_path);
    if (import_all) {
        snprintf(app->ingest_status, sizeof(app->ingest_status), "Import started for %d region(s)...", imported_count);
    } else {
        snprintf(app->ingest_status, sizeof(app->ingest_status), "Import started: %s", opened_region[0] != '\0' ? opened_region : "selected source");
    }
    return true;
}

bool app_ingest_pick_source_file(AppState *app) {
    char picked[MAPFORGE_REGION_PATH_CAPACITY];
    if (!app) {
        return false;
    }
    if (!app_runtime_pick_osm_file_macos(picked, sizeof(picked))) {
        app_runtime_set_ingest_status(app, "File picker canceled/unavailable");
        return true;
    }
    (void)app_runtime_select_source_path(app, picked);
    return true;
}

static bool app_runtime_handle_ingest_controls(AppState *app) {
    if (!app) {
        return false;
    }
    bool consumed = false;
    if (app->ui_state_bridge.input.ingest_panel_toggle_pressed) {
        /* O toggles collapse state only; panel remains visible via handle when collapsed. */
        app->ingest_panel_open = true;
        app->ui_state_bridge.hud_ingest_panel_collapsed = !app->ui_state_bridge.hud_ingest_panel_collapsed;
        if (!app->ui_state_bridge.hud_ingest_panel_collapsed) {
            app_ingest_rescan_sources(app);
            app_ingest_rescan_active_regions(app);
            snprintf(app->input_root_edit, sizeof(app->input_root_edit), "%s", app->input_root);
        }
        consumed = true;
    }
    if (!app->ingest_panel_open) {
        return consumed;
    }

    if (app->ui_state_bridge.hud_ingest_panel_collapsed) {
        return consumed;
    }

    app->ui_state_bridge.input.pan_left = false;
    app->ui_state_bridge.input.pan_right = false;
    app->ui_state_bridge.input.pan_up = false;
    app->ui_state_bridge.input.pan_down = false;

    if (app->ui_state_bridge.input.ingest_tab_toggle_pressed) {
        app->ingest_show_active_tab = !app->ingest_show_active_tab;
        consumed = true;
    }
    if (app->ui_state_bridge.input.ingest_edit_toggle_pressed) {
        app->ingest_edit_mode = !app->ingest_edit_mode;
        if (app->ingest_edit_mode) {
            snprintf(app->input_root_edit, sizeof(app->input_root_edit), "%s", app->input_root);
            app_runtime_set_ingest_status(app, "Edit mode enabled");
        } else {
            app_runtime_set_ingest_status(app, "Edit mode disabled");
        }
        consumed = true;
    }
    if (app->ui_state_bridge.input.ingest_folder_dialog_pressed) {
        char picked[MAPFORGE_REGION_PATH_CAPACITY];
        if (app_runtime_pick_folder_macos(picked, sizeof(picked))) {
            snprintf(app->input_root, sizeof(app->input_root), "%s", picked);
            snprintf(app->input_root_edit, sizeof(app->input_root_edit), "%s", app->input_root);
            app_ingest_rescan_sources(app);
            app_runtime_set_ingest_status(app, "Input root updated from folder dialog");
        } else {
            app_runtime_set_ingest_status(app, "Folder dialog canceled/unavailable");
        }
        consumed = true;
    }
    if (app->ingest_edit_mode) {
        if (app->ui_state_bridge.input.backspace_pressed) {
            size_t len = strlen(app->input_root_edit);
            if (len > 0u) {
                app->input_root_edit[len - 1u] = '\0';
            }
            consumed = true;
        }
        if (app->ui_state_bridge.input.text_input_received) {
            size_t cur = strlen(app->input_root_edit);
            size_t add = strlen(app->ui_state_bridge.input.text_input);
            if (cur + add + 1u < sizeof(app->input_root_edit)) {
                strncat(app->input_root_edit, app->ui_state_bridge.input.text_input, sizeof(app->input_root_edit) - cur - 1u);
            }
            consumed = true;
        }
    }

    if (app->ui_state_bridge.input.ingest_select_prev_pressed) {
        if (app->ingest_show_active_tab) {
            if (app->ingest_selected_active > 0) {
                app->ingest_selected_active -= 1;
            }
        } else if (app->ingest_selected_osm > 0) {
            app->ingest_selected_osm -= 1;
        }
        consumed = true;
    }
    if (app->ui_state_bridge.input.ingest_select_next_pressed) {
        if (app->ingest_show_active_tab) {
            if (app->ingest_selected_active + 1 < app->ingest_active_count) {
                app->ingest_selected_active += 1;
            }
        } else if (app->ingest_selected_osm + 1 < app->ingest_osm_count) {
            app->ingest_selected_osm += 1;
        }
        consumed = true;
    }

    if (app->ui_state_bridge.input.ingest_import_all_pressed) {
        if (!app->ingest_show_active_tab) {
            (void)app_ingest_import_selected_osm(app, true);
        }
        consumed = true;
    }

    if (app->ui_state_bridge.input.enter_pressed) {
        if (app->ingest_edit_mode) {
            snprintf(app->input_root, sizeof(app->input_root), "%s", app->input_root_edit);
            app_ingest_rescan_sources(app);
            app_runtime_set_ingest_status(app, "Input root applied");
            consumed = true;
        } else if (app->ingest_show_active_tab) {
            (void)app_ingest_open_selected_active_region(app);
            consumed = true;
        } else {
            (void)app_ingest_open_or_import_selected_source(app);
            consumed = true;
        }
    }

    return consumed;
}

void app_apply_shared_ui_font(AppState *app) {
    char shared_font_path[384] = {0};
    int shared_font_size = 0;
    if (!app) {
        return;
    }
    if (mapforge_shared_font_resolve_ui_regular(shared_font_path,
                                                sizeof(shared_font_path),
                                                &shared_font_size)) {
        ui_font_set(shared_font_path, shared_font_size);
    } else {
        ui_font_set("assets/fonts/Montserrat-Regular.ttf", 10);
    }
    app->ui_state_bridge.hud_layer_debug_layout_dirty = true;
    app->ui_state_bridge.hud_route_panel_layout_dirty = true;
}


bool app_runtime_ingest_tick(AppState *app) {
    if (!app) {
        return false;
    }
    app_runtime_poll_import_process(app);
    return app_runtime_handle_ingest_controls(app);
}

bool app_runtime_cycle_next_region(AppState *app) {
    if (!app) {
        return false;
    }
    int total_regions = region_count();
    if (total_regions <= 0) {
        log_error("No region packs found under '%s'", region_data_root());
        return false;
    }
    int next_index = app_runtime_find_next_region_index(app->region_index);
    if (next_index < 0) {
        log_error("No switchable regions found under '%s'", region_data_root());
        return false;
    }
    (void)app_runtime_open_region_index(app, next_index);
    return true;
}
