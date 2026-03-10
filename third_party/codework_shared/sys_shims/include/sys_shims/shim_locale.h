#ifndef SYS_SHIMS_SHIM_LOCALE_H
#define SYS_SHIMS_SHIM_LOCALE_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <locale.h>
#else
#include <locale.h>
#endif

typedef struct lconv shim_lconv_t;

#define SHIM_LC_ALL LC_ALL
#define SHIM_LC_NUMERIC LC_NUMERIC

static inline char *shim_setlocale(int category, const char *locale) {
    return setlocale(category, locale);
}

static inline shim_lconv_t *shim_localeconv(void) {
    return localeconv();
}

#endif
