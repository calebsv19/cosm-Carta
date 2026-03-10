#ifndef SYS_SHIMS_SHIM_STDLIB_H
#define SYS_SHIMS_SHIM_STDLIB_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <errno.h>
#include_next <stdbool.h>
#include_next <stddef.h>
#include_next <stdlib.h>
#else
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#endif

#include "sys_shims/shim_stddef.h"

static inline void *shim_malloc(shim_size_t size) {
    return malloc(size);
}

static inline void *shim_calloc(shim_size_t count, shim_size_t size) {
    return calloc(count, size);
}

static inline void *shim_realloc(void *ptr, shim_size_t size) {
    return realloc(ptr, size);
}

static inline void shim_free(void *ptr) {
    free(ptr);
}

static inline int shim_abs_i(int value) {
    return abs(value);
}

static inline bool shim_strtol_checked(const char *text,
                                       int base,
                                       long *out_value,
                                       int *out_errno) {
    char *end = NULL;
    long value;

    if (!text || !out_value) {
        if (out_errno) *out_errno = EINVAL;
        return false;
    }

    errno = 0;
    value = strtol(text, &end, base);

    if (end == text || (end && *end != '\0')) {
        if (out_errno) *out_errno = EINVAL;
        return false;
    }
    if (errno != 0) {
        if (out_errno) *out_errno = errno;
        return false;
    }

    *out_value = value;
    if (out_errno) *out_errno = 0;
    return true;
}

#endif
