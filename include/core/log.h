#ifndef MAPFORGE_CORE_LOG_H
#define MAPFORGE_CORE_LOG_H

// Logs an informational message to stderr.
void log_info(const char *fmt, ...);

// Logs an error message to stderr.
void log_error(const char *fmt, ...);

#endif
