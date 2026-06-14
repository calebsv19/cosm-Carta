/*
 * time_scope.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "timer_hud/time_scope.h"
#include "timer_hud/settings_loader.h"
#include "../core/session.h"
#include "../events/event_tracker.h"
#include "../logging/logger.h"
#include "../core/timer_manager.h"
#include "../hud/hud_renderer.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define TS_DEFAULT_PROGRAM_NAME "app"
#define TS_OUTPUT_DIR_NAME "timerhud"
#define TS_DEFAULT_SETTINGS_FILE "settings.json"
#define TS_DEFAULT_LOG_FILE "timing.json"

static void ts_strlcpy(char* dst, size_t dst_cap, const char* src) {
    if (!dst || dst_cap == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_cap - 1);
    dst[dst_cap - 1] = '\0';
}

static int path_is_absolute(const char* path) {
    return path && path[0] == '/';
}

static int path_exists(const char* path) {
    return (path && access(path, F_OK) == 0);
}

static void join_path(char* out, size_t out_cap, const char* a, const char* b) {
    if (!out || out_cap == 0) return;
    if (!a || !a[0]) {
        ts_strlcpy(out, out_cap, b ? b : "");
        return;
    }
    if (!b || !b[0]) {
        ts_strlcpy(out, out_cap, a);
        return;
    }

    if (a[strlen(a) - 1] == '/') {
        snprintf(out, out_cap, "%s%s", a, b);
    } else {
        snprintf(out, out_cap, "%s/%s", a, b);
    }
}

static int ensure_dir_recursive(const char* path) {
    if (!path || !path[0]) return 0;

    char tmp[PATH_MAX];
    ts_strlcpy(tmp, sizeof(tmp), path);

    for (char* p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                return 0;
            }
            *p = '/';
        }
    }

    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
        return 0;
    }
    return 1;
}

static int ensure_parent_dir(const char* filepath) {
    if (!filepath || !filepath[0]) return 0;

    char dir[PATH_MAX];
    ts_strlcpy(dir, sizeof(dir), filepath);

    char* slash = strrchr(dir, '/');
    if (!slash) return 1;
    *slash = '\0';

    if (!dir[0]) return 1;
    return ensure_dir_recursive(dir);
}

static int copy_file_contents(const char* src_path, const char* dst_path) {
    FILE* in = NULL;
    FILE* out = NULL;
    char buffer[4096];
    size_t n = 0;
    int ok = 1;

    if (!src_path || !src_path[0] || !dst_path || !dst_path[0]) {
        return 0;
    }

    in = fopen(src_path, "rb");
    if (!in) {
        return 0;
    }

    if (!ensure_parent_dir(dst_path)) {
        fclose(in);
        return 0;
    }

    out = fopen(dst_path, "wb");
    if (!out) {
        fclose(in);
        return 0;
    }

    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, n, out) != n) {
            ok = 0;
            break;
        }
    }

    if (ferror(in)) {
        ok = 0;
    }
    if (fclose(in) != 0) {
        ok = 0;
    }
    if (fclose(out) != 0) {
        ok = 0;
    }
    return ok;
}

static void sanitize_program_name(const char* in, char* out, size_t out_cap) {
    if (!out || out_cap == 0) return;
    if (!in || !in[0]) {
        ts_strlcpy(out, out_cap, TS_DEFAULT_PROGRAM_NAME);
        return;
    }

    size_t j = 0;
    for (size_t i = 0; in[i] && j + 1 < out_cap; ++i) {
        unsigned char c = (unsigned char)in[i];
        if (isalnum(c) || c == '_' || c == '-') {
            out[j++] = (char)c;
        } else {
            out[j++] = '_';
        }
    }
    out[j] = '\0';

    if (j == 0) {
        ts_strlcpy(out, out_cap, TS_DEFAULT_PROGRAM_NAME);
    }
}

static int resolve_executable_path(char* out, size_t out_cap) {
    if (!out || out_cap == 0) return 0;

#if defined(__APPLE__)
    {
        uint32_t size = (uint32_t)out_cap;
        if (_NSGetExecutablePath(out, &size) == 0 && out[0]) {
            return 1;
        }
    }
#elif defined(__linux__)
    {
        ssize_t n = readlink("/proc/self/exe", out, out_cap - 1);
        if (n > 0) {
            out[n] = '\0';
            return 1;
        }
    }
#endif

    return 0;
}

static int resolve_executable_dir(char* out, size_t out_cap) {
    char exe_path[PATH_MAX];
    if (!resolve_executable_path(exe_path, sizeof(exe_path))) {
        return 0;
    }

    char* slash = strrchr(exe_path, '/');
    if (!slash) {
        return 0;
    }
    *slash = '\0';

    if (!exe_path[0]) {
        return 0;
    }

    ts_strlcpy(out, out_cap, exe_path);
    return 1;
}

static void resolve_program_name(const TimerHUDSession* session, char* out, size_t out_cap) {
    if (!out || out_cap == 0) return;

    if (session && session->program_name[0]) {
        sanitize_program_name(session->program_name, out, out_cap);
        return;
    }

    {
        char exe_path[PATH_MAX];
        if (resolve_executable_path(exe_path, sizeof(exe_path))) {
            const char* base = strrchr(exe_path, '/');
            sanitize_program_name(base ? base + 1 : exe_path, out, out_cap);
            return;
        }
    }

    ts_strlcpy(out, out_cap, TS_DEFAULT_PROGRAM_NAME);
}

static void resolve_output_root(const TimerHUDSession* session, char* out, size_t out_cap) {
    if (!out || out_cap == 0) return;

    if (session && session->output_root[0]) {
        if (path_is_absolute(session->output_root)) {
            ts_strlcpy(out, out_cap, session->output_root);
            return;
        }

        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) {
            join_path(out, out_cap, cwd, session->output_root);
            return;
        }
    }

    if (resolve_executable_dir(out, out_cap)) {
        return;
    }

    if (!getcwd(out, out_cap)) {
        ts_strlcpy(out, out_cap, ".");
    }
}

static void resolve_settings_path(const TimerHUDSession* session,
                                  char* out,
                                  size_t out_cap,
                                  const char* root,
                                  const char* program_dir) {
    if (!out || out_cap == 0) return;

    if (session && session->settings_path[0]) {
        if (path_is_absolute(session->settings_path)) {
            ts_strlcpy(out, out_cap, session->settings_path);
        } else {
            join_path(out, out_cap, root, session->settings_path);
        }
        return;
    }

    join_path(out, out_cap, program_dir, TS_DEFAULT_SETTINGS_FILE);
}

static void resolve_log_path(const TimerHUDSession* session, char* out, size_t out_cap, const char* program_dir) {
    const char* configured = NULL;

    if (!out || out_cap == 0) return;

    configured = ts_session_get_log_filepath(session);
    if (!configured || !configured[0]) {
        configured = TS_DEFAULT_LOG_FILE;
    }

    if (path_is_absolute(configured)) {
        ts_strlcpy(out, out_cap, configured);
        return;
    }

    join_path(out, out_cap, program_dir, configured);
}

TimerHUDSession* ts_session_create(void) {
    return ts_session_create_internal();
}

void ts_session_destroy(TimerHUDSession* session) {
    if (!session || session == ts_default_session_internal()) {
        return;
    }
    ts_session_shutdown(session);
    ts_session_destroy_internal(session);
}

TimerHUDSession* ts_default_session(void) {
    return ts_default_session_internal();
}

void ts_session_register_backend(TimerHUDSession* session, const TimerHUDBackend* backend) {
    if (!session) {
        return;
    }
    hud_set_backend(session, backend);
}

void ts_register_backend(const TimerHUDBackend* backend) {
    ts_session_register_backend(ts_default_session_internal(), backend);
}

void ts_session_set_settings_path(TimerHUDSession* session, const char* path) {
    if (!session) {
        return;
    }
    ts_strlcpy(session->settings_path, sizeof(session->settings_path), path ? path : "");
}

void ts_set_settings_path(const char* path) {
    ts_session_set_settings_path(ts_default_session_internal(), path);
}

void ts_session_set_output_root(TimerHUDSession* session, const char* path) {
    if (!session) {
        return;
    }
    ts_strlcpy(session->output_root, sizeof(session->output_root), path ? path : "");
}

void ts_set_output_root(const char* path) {
    ts_session_set_output_root(ts_default_session_internal(), path);
}

void ts_session_set_program_name(TimerHUDSession* session, const char* name) {
    if (!session) {
        return;
    }
    ts_strlcpy(session->program_name, sizeof(session->program_name), name ? name : "");
}

void ts_set_program_name(const char* name) {
    ts_session_set_program_name(ts_default_session_internal(), name);
}

bool ts_seed_settings_file(const char* default_settings_path, const char* runtime_settings_path) {
    if (!runtime_settings_path || !runtime_settings_path[0]) {
        return false;
    }
    if (path_exists(runtime_settings_path)) {
        return true;
    }
    if (!default_settings_path || !default_settings_path[0]) {
        return true;
    }
    if (!path_exists(default_settings_path)) {
        return true;
    }
    if (!copy_file_contents(default_settings_path, runtime_settings_path)) {
        fprintf(stderr,
                "[TimeScope] Failed to seed settings from %s to %s\n",
                default_settings_path,
                runtime_settings_path);
        return false;
    }
    return true;
}

bool ts_session_apply_init_config(TimerHUDSession* session, const TimerHUDInitConfig* config) {
    if (!session || !config) {
        return false;
    }

    if (config->program_name && config->program_name[0]) {
        ts_session_set_program_name(session, config->program_name);
    }
    if (config->output_root && config->output_root[0]) {
        ts_session_set_output_root(session, config->output_root);
    }
    if (config->seed_settings_if_missing &&
        config->settings_path &&
        config->settings_path[0] &&
        !ts_seed_settings_file(config->default_settings_path, config->settings_path)) {
        return false;
    }
    if (config->settings_path && config->settings_path[0]) {
        ts_session_set_settings_path(session, config->settings_path);
    }
    return true;
}

bool ts_apply_init_config(const TimerHUDInitConfig* config) {
    return ts_session_apply_init_config(ts_default_session_internal(), config);
}

void ts_session_init(TimerHUDSession* session) {
    char root_dir[PATH_MAX];
    char program_name[64];
    char output_dir[PATH_MAX];
    char program_dir[PATH_MAX];
    char settings_path[PATH_MAX];
    char log_path[PATH_MAX];

    if (!session) {
        return;
    }

    resolve_output_root(session, root_dir, sizeof(root_dir));
    resolve_program_name(session, program_name, sizeof(program_name));

    join_path(output_dir, sizeof(output_dir), root_dir, TS_OUTPUT_DIR_NAME);
    join_path(program_dir, sizeof(program_dir), output_dir, program_name);

    if (!ensure_dir_recursive(output_dir) || !ensure_dir_recursive(program_dir)) {
        fprintf(stderr, "[TimeScope] Failed to create output directory: %s\n", program_dir);
    }

    resolve_settings_path(session, settings_path, sizeof(settings_path), root_dir, program_dir);

    if (!path_exists(settings_path)) {
        join_path(log_path, sizeof(log_path), program_dir, TS_DEFAULT_LOG_FILE);
        ts_strlcpy(session->settings.log_filepath, sizeof(session->settings.log_filepath), log_path);
        if (!session->settings.hud_position[0]) {
            ts_strlcpy(session->settings.hud_position, sizeof(session->settings.hud_position), "top-left");
        }
        if (!ensure_parent_dir(settings_path)) {
            fprintf(stderr, "[TimeScope] Failed to create settings parent directory for %s\n", settings_path);
        } else {
            ts_session_save_settings_to_file(session, settings_path);
        }
    }

    if (!ts_session_load_settings(session, settings_path)) {
        fprintf(stderr, "[TimeScope] Using default settings (load failed: %s).\n", settings_path);
    }

    resolve_log_path(session, log_path, sizeof(log_path), program_dir);
    if (!ensure_parent_dir(log_path)) {
        fprintf(stderr, "[TimeScope] Failed to create log parent directory for %s\n", log_path);
    }
    ts_strlcpy(session->settings.log_filepath, sizeof(session->settings.log_filepath), log_path);

    fprintf(stderr,
            "[TimeScope] root=%s program=%s settings=%s log=%s\n",
            root_dir,
            program_name,
            settings_path,
            session->settings.log_filepath);

    if (ts_session_is_log_enabled(session)) {
        LogFormat format = LOG_FORMAT_JSON;
        if (strcmp(session->settings.log_format, "csv") == 0) {
            format = LOG_FORMAT_CSV;
        }
        logger_init(session, session->settings.log_filepath, format);
    }

    event_tracker_init(session);
    tm_init(session);
    hud_init(session);
}

void ts_init(void) {
    ts_session_init(ts_default_session_internal());
}

void ts_session_shutdown(TimerHUDSession* session) {
    if (!session) {
        return;
    }
    logger_shutdown(session);
    hud_shutdown(session);
}

void ts_shutdown(void) {
    ts_session_shutdown(ts_default_session_internal());
}

void ts_session_emit_event(TimerHUDSession* session, const char* tag) {
    if (ts_session_is_event_tagging_enabled(session)) {
        event_tracker_add(session, tag);
    }
}

void ts_emit_event(const char* tag) {
    ts_session_emit_event(ts_default_session_internal(), tag);
}

void ts_session_start_timer(TimerHUDSession* session, const char* name) {
    Timer* timer = tm_find_or_create_timer(session, name);
    if (timer) {
        timer_start(timer);
    }
}

void ts_start_timer(const char* name) {
    ts_session_start_timer(ts_default_session_internal(), name);
}

void ts_session_stop_timer(TimerHUDSession* session, const char* name) {
    Timer* timer = tm_find_timer(session, name);
    if (!timer) {
        fprintf(stderr, "[TimeScope] Ignoring stop for unknown timer '%s'\n", name ? name : "(null)");
        return;
    }
    timer_stop(timer);
}

void ts_stop_timer(const char* name) {
    ts_session_stop_timer(ts_default_session_internal(), name);
}

void ts_session_record_duration_ms(TimerHUDSession* session, const char* name, double duration_ms) {
    Timer* timer = tm_find_or_create_timer(session, name);
    if (timer) {
        timer_record_duration_ms(timer, duration_ms);
    }
}

void ts_record_duration_ms(const char* name, double duration_ms) {
    ts_session_record_duration_ms(ts_default_session_internal(), name, duration_ms);
}

void ts_frame_start(void) {
    ts_session_frame_start(ts_default_session_internal());
}

void ts_frame_end(void) {
    ts_session_frame_end(ts_default_session_internal());
}

void ts_session_render(TimerHUDSession* session) {
    hud_render(session);
}

void ts_render(void) {
    ts_session_render(ts_default_session_internal());
}

static bool point_in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return rw > 0 && rh > 0 && x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static TimerHUDVisualMode cycle_visual_mode(TimerHUDVisualMode mode, int delta) {
    static const TimerHUDVisualMode modes[] = {
        TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH,
        TIMER_HUD_VISUAL_MODE_STATS,
        TIMER_HUD_VISUAL_MODE_SPIKES,
        TIMER_HUD_VISUAL_MODE_COMPARE,
    };
    int count = (int)(sizeof(modes) / sizeof(modes[0]));
    int index = 0;

    for (int i = 0; i < count; ++i) {
        if (modes[i] == mode || (mode == TIMER_HUD_VISUAL_MODE_HYBRID && modes[i] == TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH)) {
            index = i;
            break;
        }
    }
    index = (index + delta) % count;
    if (index < 0) {
        index += count;
    }
    return modes[index];
}

bool ts_session_handle_pointer_down(TimerHUDSession* session, int x, int y, int button) {
    TimerHUDVisualMode mode = TIMER_HUD_VISUAL_MODE_INVALID;

    if (!session || button != 1 || !ts_session_is_hud_enabled(session)) {
        return false;
    }

    mode = ts_session_get_hud_visual_mode_kind(session);
    if (point_in_rect(x,
                      y,
                      session->control_mode_prev_x,
                      session->control_mode_prev_y,
                      session->control_mode_prev_w,
                      session->control_mode_prev_h)) {
        (void)ts_session_set_hud_visual_mode_kind(session, cycle_visual_mode(mode, -1));
        return true;
    }
    if (point_in_rect(x,
                      y,
                      session->control_mode_next_x,
                      session->control_mode_next_y,
                      session->control_mode_next_w,
                      session->control_mode_next_h)) {
        (void)ts_session_set_hud_visual_mode_kind(session, cycle_visual_mode(mode, 1));
        return true;
    }
    if (point_in_rect(x,
                      y,
                      session->control_mode_history_x,
                      session->control_mode_history_y,
                      session->control_mode_history_w,
                      session->control_mode_history_h)) {
        (void)ts_session_set_hud_visual_mode_kind(session, TIMER_HUD_VISUAL_MODE_HISTORY_GRAPH);
        return true;
    }

    return false;
}

bool ts_handle_pointer_down(int x, int y, int button) {
    return ts_session_handle_pointer_down(ts_default_session_internal(), x, y, button);
}
