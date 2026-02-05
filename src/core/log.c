#include "core/log.h"

#include <stdarg.h>
#include <stdio.h>

static void log_vwrite(const char *prefix, const char *fmt, va_list args) {
    fputs(prefix, stderr);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
}

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_vwrite("[info] ", fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_vwrite("[error] ", fmt, args);
    va_end(args);
}
