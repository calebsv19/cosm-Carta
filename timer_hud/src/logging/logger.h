#ifndef TIMESCOPE_LOGGER_H
#define TIMESCOPE_LOGGER_H

#include <stdbool.h>

typedef enum {
    LOG_FORMAT_JSON,
    LOG_FORMAT_CSV
} LogFormat;

void logger_init(const char* filepath, LogFormat format);
void logger_shutdown(void);

void logger_log_frame(void);  // Call this at end of frame

#endif // TIMESCOPE_LOGGER_H

