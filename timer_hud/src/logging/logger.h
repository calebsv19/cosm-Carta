#ifndef TIMESCOPE_LOGGER_H
#define TIMESCOPE_LOGGER_H

#include "../core/session_fwd.h"

#include <stdbool.h>

typedef enum {
    LOG_FORMAT_JSON,
    LOG_FORMAT_CSV
} LogFormat;

void logger_init(TimerHUDSession* session, const char* filepath, LogFormat format);
void logger_shutdown(TimerHUDSession* session);

void logger_log_frame(TimerHUDSession* session);  // Call this at end of frame

#endif // TIMESCOPE_LOGGER_H
