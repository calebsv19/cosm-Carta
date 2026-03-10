#include "logger.h"
#include "../core/timer_manager.h"
#include <stdio.h>
#include <string.h>

static FILE* log_file = NULL;
static LogFormat current_format = LOG_FORMAT_JSON;
static bool wrote_csv_header = false;

void logger_init(const char* filepath, LogFormat format) {
    log_file = fopen(filepath, "w");
    current_format = format;
    wrote_csv_header = false;

    if (!log_file) {
        fprintf(stderr, "[TimeScope] Failed to open log file: %s\n", filepath);
    } else {
        fprintf(stderr, "[TimeScope] Logging to %s (%s format)\n",
                filepath, format == LOG_FORMAT_CSV ? "CSV" : "JSON");
    }
}

void logger_shutdown(void) {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

static void log_csv_frame(void) {
    TimerManager* tm = &g_timer_manager;

    if (!wrote_csv_header) {
        for (int i = 0; i < tm->count; i++) {
            fprintf(log_file, "%s_avg", tm->timers[i].name);
            if (i < tm->count - 1) fprintf(log_file, ",");
        }
        fprintf(log_file, "\n");
        wrote_csv_header = true;
    }

    for (int i = 0; i < tm->count; i++) {
        fprintf(log_file, "%.3f", tm->timers[i].avg);
        if (i < tm->count - 1) fprintf(log_file, ",");
    }
    fprintf(log_file, "\n");
}

static void log_json_frame(void) {
    TimerManager* tm = &g_timer_manager;

    fprintf(log_file, "{");
    for (int i = 0; i < tm->count; i++) {
        fprintf(log_file, "\"%s\": %.3f", tm->timers[i].name, tm->timers[i].avg);
        if (i < tm->count - 1) fprintf(log_file, ", ");
    }
    fprintf(log_file, "}\n");
}

void logger_log_frame(void) {
    if (!log_file) return;

    switch (current_format) {
        case LOG_FORMAT_CSV:
            log_csv_frame();
            break;
        case LOG_FORMAT_JSON:
            log_json_frame();
            break;
    }

    fflush(log_file);  // Optional: remove for perf
}

