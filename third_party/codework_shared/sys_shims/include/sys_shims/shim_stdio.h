#ifndef SYS_SHIMS_SHIM_STDIO_H
#define SYS_SHIMS_SHIM_STDIO_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <stdio.h>
#else
#include <stdio.h>
#endif

static inline int shim_fputs_line(FILE *stream, const char *text) {
    int rc;
    if (!stream || !text) return EOF;
    rc = fputs(text, stream);
    if (rc == EOF) return rc;
    return fputc('\n', stream);
}

#endif
