/*
 * time_scope.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "timer_hud/time_scope.h"
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

static const TimerHUDBackend* g_backend = NULL;
static char g_settings_path[PATH_MAX] = {0};
static char g_output_root[PATH_MAX] = {0};
static char g_program_name[64] = {0};

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

static void resolve_program_name(char* out, size_t out_cap) {
    if (!out || out_cap == 0) return;

    if (g_program_name[0]) {
        sanitize_program_name(g_program_name, out, out_cap);
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

static void resolve_output_root(char* out, size_t out_cap) {
    if (!out || out_cap == 0) return;

    if (g_output_root[0]) {
        if (path_is_absolute(g_output_root)) {
            ts_strlcpy(out, out_cap, g_output_root);
            return;
        }

        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd))) {
            join_path(out, out_cap, cwd, g_output_root);
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

static void resolve_settings_path(char* out, size_t out_cap, const char* root, const char* program_dir) {
    if (!out || out_cap == 0) return;

    if (g_settings_path[0]) {
        if (path_is_absolute(g_settings_path)) {
            ts_strlcpy(out, out_cap, g_settings_path);
        } else {
            join_path(out, out_cap, root, g_settings_path);
        }
        return;
    }

    join_path(out, out_cap, program_dir, TS_DEFAULT_SETTINGS_FILE);
}

static void resolve_log_path(char* out, size_t out_cap, const char* program_dir) {
    if (!out || out_cap == 0) return;

    const char* configured = ts_settings.log_filepath;
    if (!configured || !configured[0]) {
        configured = TS_DEFAULT_LOG_FILE;
    }

    if (path_is_absolute(configured)) {
        ts_strlcpy(out, out_cap, configured);
        return;
    }

    join_path(out, out_cap, program_dir, configured);
}

void ts_register_backend(const TimerHUDBackend* backend) {
    g_backend = backend;
    hud_set_backend(backend);
}

void ts_set_settings_path(const char* path) {
    ts_strlcpy(g_settings_path, sizeof(g_settings_path), path ? path : "");
}

void ts_set_output_root(const char* path) {
    ts_strlcpy(g_output_root, sizeof(g_output_root), path ? path : "");
}

void ts_set_program_name(const char* name) {
    ts_strlcpy(g_program_name, sizeof(g_program_name), name ? name : "");
}

void ts_set_hud_visual_mode(const char* mode) {
    if (!mode) return;
    if (strcmp(mode, "text_compact") != 0 &&
        strcmp(mode, "history_graph") != 0 &&
        strcmp(mode, "hybrid") != 0) {
        return;
    }
    ts_strlcpy(ts_settings.hud_visual_mode, sizeof(ts_settings.hud_visual_mode), mode);
}

void ts_init(void) {
    char root_dir[PATH_MAX];
    char program_name[64];
    char output_dir[PATH_MAX];
    char program_dir[PATH_MAX];
    char settings_path[PATH_MAX];
    char log_path[PATH_MAX];

    resolve_output_root(root_dir, sizeof(root_dir));
    resolve_program_name(program_name, sizeof(program_name));

    join_path(output_dir, sizeof(output_dir), root_dir, TS_OUTPUT_DIR_NAME);
    join_path(program_dir, sizeof(program_dir), output_dir, program_name);

    if (!ensure_dir_recursive(output_dir) || !ensure_dir_recursive(program_dir)) {
        fprintf(stderr, "[TimeScope] Failed to create output directory: %s\n", program_dir);
    }

    resolve_settings_path(settings_path, sizeof(settings_path), root_dir, program_dir);

    if (!path_exists(settings_path)) {
        join_path(log_path, sizeof(log_path), program_dir, TS_DEFAULT_LOG_FILE);
        ts_strlcpy(ts_settings.log_filepath, sizeof(ts_settings.log_filepath), log_path);
        if (!ts_settings.hud_position[0]) {
            ts_strlcpy(ts_settings.hud_position, sizeof(ts_settings.hud_position), "top-left");
        }
        if (!ensure_parent_dir(settings_path)) {
            fprintf(stderr, "[TimeScope] Failed to create settings parent directory for %s\n", settings_path);
        } else {
            save_settings_to_file(settings_path);
        }
    }

    if (!ts_load_settings(settings_path)) {
        fprintf(stderr, "[TimeScope] Using default settings (load failed: %s).\n", settings_path);
    }

    resolve_log_path(log_path, sizeof(log_path), program_dir);
    if (!ensure_parent_dir(log_path)) {
        fprintf(stderr, "[TimeScope] Failed to create log parent directory for %s\n", log_path);
    }
    ts_strlcpy(ts_settings.log_filepath, sizeof(ts_settings.log_filepath), log_path);

    fprintf(stderr,
            "[TimeScope] root=%s program=%s settings=%s log=%s\n",
            root_dir,
            program_name,
            settings_path,
            ts_settings.log_filepath);

    if (ts_settings.log_enabled) {
        LogFormat format = LOG_FORMAT_JSON;
        if (strcmp(ts_settings.log_format, "csv") == 0) {
            format = LOG_FORMAT_CSV;
        }
        logger_init(ts_settings.log_filepath, format);
    }

    event_tracker_init();
    tm_init();
    hud_init();
    (void)g_backend;
}

void ts_shutdown(void) {
    logger_shutdown();
    hud_shutdown();
}

void ts_emit_event(const char* tag) {
    if (ts_settings.event_tagging_enabled) {
        event_tracker_add(tag);
    }
}
