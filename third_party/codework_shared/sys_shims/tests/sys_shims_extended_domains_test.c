#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>

#include "sys_shims/shim_ctype.h"
#include "sys_shims/shim_limits.h"
#include "sys_shims/shim_locale.h"
#include "sys_shims/shim_signal.h"

static int check_ctype_parity(void) {
    static const int samples[] = {'A', 'z', '4', ' ', '\n', '#'};
    size_t i;

    for (i = 0; i < (sizeof(samples) / sizeof(samples[0])); ++i) {
        int ch = samples[i];
        if (shim_isalpha_i(ch) != (isalpha(ch) != 0)) return 1;
        if (shim_isdigit_i(ch) != (isdigit(ch) != 0)) return 1;
        if (shim_isspace_i(ch) != (isspace(ch) != 0)) return 1;
        if (shim_tolower_i(ch) != tolower(ch)) return 1;
        if (shim_toupper_i(ch) != toupper(ch)) return 1;
    }

    return 0;
}

int main(void) {
    shim_lconv_t *lc = NULL;
    char *active_locale = NULL;
    shim_sig_atomic_t signal_value = 2;

    if (SHIM_CHAR_BIT != CHAR_BIT) return 1;
    if (SHIM_SCHAR_MIN != SCHAR_MIN || SHIM_SCHAR_MAX != SCHAR_MAX) return 1;
    if (SHIM_UCHAR_MAX != UCHAR_MAX) return 1;
    if (SHIM_INT_MIN != INT_MIN || SHIM_INT_MAX != INT_MAX) return 1;
    if (SHIM_LONG_MIN != LONG_MIN || SHIM_LONG_MAX != LONG_MAX) return 1;

    if (check_ctype_parity() != 0) return 1;

    active_locale = shim_setlocale(SHIM_LC_ALL, NULL);
    if (!active_locale || !active_locale[0]) return 1;
    lc = shim_localeconv();
    if (!lc || !lc->decimal_point || !lc->decimal_point[0]) return 1;

    if (signal_value != 2) return 1;
    if (sizeof(shim_sig_atomic_t) != sizeof(sig_atomic_t)) return 1;
    if (SHIM_SIG_DFL != SIG_DFL) return 1;
    if (SHIM_SIG_IGN != SIG_IGN) return 1;
    if (SHIM_SIG_ERR != SIG_ERR) return 1;

    puts("sys_shims extended domains test passed");
    return 0;
}
