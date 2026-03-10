#ifndef SYS_SHIMS_SHIM_STRING_H
#define SYS_SHIMS_SHIM_STRING_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <stdbool.h>
#include_next <stddef.h>
#include_next <string.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#endif

#include "sys_shims/shim_stddef.h"

static inline bool shim_streq(const char *a, const char *b) {
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}

static inline shim_size_t shim_strnlen_s(const char *text, shim_size_t max_len) {
    if (!text) return 0;
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
    return strnlen(text, max_len);
#else
    shim_size_t i = 0;
    while (i < max_len && text[i] != '\0') i++;
    return i;
#endif
}

static inline bool shim_memcpy_checked(void *dst,
                                       shim_size_t dst_size,
                                       const void *src,
                                       shim_size_t bytes) {
    if (!dst || !src) return false;
    if (bytes > dst_size) return false;
    (void)memcpy(dst, src, bytes);
    return true;
}

#endif
