#include "logger.h"
#include "../core/session.h"
#include "../core/timer_manager.h"
#include <stdio.h>

void logger_init(TimerHUDSession* session, const char* filepath, LogFormat format) {
    if (!session) {
        return;
    }
    session->log_file = fopen(filepath, "w");
    session->log_format = format;
    session->wrote_csv_header = false;

    if (!session->log_file) {
        fprintf(stderr, "[TimeScope] Failed to open log file: %s\n", filepath);
    } else {
        fprintf(stderr, "[TimeScope] Logging to %s (%s format)\n",
                filepath, format == LOG_FORMAT_CSV ? "CSV" : "JSON");
    }
}

void logger_shutdown(TimerHUDSession* session) {
    if (!session) {
        return;
    }
    if (session->log_file) {
        fclose(session->log_file);
        session->log_file = NULL;
    }
}

static void log_csv_frame(TimerHUDSession* session) {
    TimerManager* tm = &session->timer_manager;

    if (!session->wrote_csv_header) {
        for (int i = 0; i < tm->count; i++) {
            fprintf(session->log_file, "%s_avg", tm->timers[i].name);
            if (i < tm->count - 1) fprintf(session->log_file, ",");
        }
        fprintf(session->log_file, "\n");
        session->wrote_csv_header = true;
    }

    for (int i = 0; i < tm->count; i++) {
        fprintf(session->log_file, "%.3f", tm->timers[i].avg);
        if (i < tm->count - 1) fprintf(session->log_file, ",");
    }
    fprintf(session->log_file, "\n");
}

static void log_json_frame(TimerHUDSession* session) {
    TimerManager* tm = &session->timer_manager;

    fprintf(session->log_file, "{");
    for (int i = 0; i < tm->count; i++) {
        fprintf(session->log_file, "\"%s\": %.3f", tm->timers[i].name, tm->timers[i].avg);
        if (i < tm->count - 1) fprintf(session->log_file, ", ");
    }
    fprintf(session->log_file, "}\n");
}

void logger_log_frame(TimerHUDSession* session) {
    if (!session || !session->log_file) return;

    switch (session->log_format) {
        case LOG_FORMAT_CSV:
            log_csv_frame(session);
            break;
        case LOG_FORMAT_JSON:
            log_json_frame(session);
            break;
    }

    fflush(session->log_file);  // Optional: remove for perf
}
