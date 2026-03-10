#ifndef SYS_SHIMS_SHIM_CTYPE_H
#define SYS_SHIMS_SHIM_CTYPE_H

#include <stdbool.h>

#if defined(__clang__) || defined(__GNUC__)
#include_next <ctype.h>
#else
#include <ctype.h>
#endif

static inline bool shim_isalpha_i(int ch) {
    return isalpha(ch) != 0;
}

static inline bool shim_isdigit_i(int ch) {
    return isdigit(ch) != 0;
}

static inline bool shim_isspace_i(int ch) {
    return isspace(ch) != 0;
}

static inline int shim_tolower_i(int ch) {
    return tolower(ch);
}

static inline int shim_toupper_i(int ch) {
    return toupper(ch);
}

#endif
