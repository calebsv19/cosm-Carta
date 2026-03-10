#ifndef SYS_SHIMS_SHIM_ERRNO_H
#define SYS_SHIMS_SHIM_ERRNO_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <errno.h>
#else
#include <errno.h>
#endif

typedef int shim_errno_t;

#define SHIM_EINVAL EINVAL
#define SHIM_ERANGE ERANGE
#define SHIM_ENOENT ENOENT

static inline shim_errno_t shim_errno_get(void) {
    return errno;
}

static inline void shim_errno_set(shim_errno_t value) {
    errno = value;
}

static inline void shim_errno_clear(void) {
    errno = 0;
}

#endif
