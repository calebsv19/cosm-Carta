#include <stdio.h>

#include "sys_shims/shim_ctype.h"
#include "sys_shims/shim_limits.h"
#include "sys_shims/shim_locale.h"
#include "sys_shims/shim_signal.h"
#include "sys_shims/shim_stdarg.h"
#include "sys_shims/shim_stdbool.h"
#include "sys_shims/shim_stddef.h"
#include "sys_shims/shim_stdint.h"
#include "sys_shims/shim_stdio.h"

static int sum_ints(int n, ...) {
    int total = 0;
    int i;
    shim_va_list ap;
    shim_va_start(ap, n);
    for (i = 0; i < n; ++i) {
        total += shim_va_arg(ap, int);
    }
    shim_va_end(ap);
    return total;
}

int main(void) {
    int total = sum_ints(4, 1, 2, 3, 4);
    shim_bool_t ok = (total == 10) ? SHIM_TRUE : SHIM_FALSE;
    shim_uint32_t marker = (shim_uint32_t)total;
    shim_size_t marker_size = sizeof(marker);
    shim_lconv_t *lc = shim_localeconv();
    shim_sig_atomic_t sig_value = 1;
    if (!ok) return 1;
    if (marker != 10u || marker_size != sizeof(uint32_t)) return 1;
    if (!shim_isalpha_i('A') || !shim_isdigit_i('7')) return 1;
    if (shim_tolower_i('Q') != 'q') return 1;
    if (SHIM_CHAR_BIT != CHAR_BIT || SHIM_INT_MAX != INT_MAX) return 1;
    if (!lc || !lc->decimal_point || !lc->decimal_point[0]) return 1;
    if (sig_value != 1 || SHIM_SIG_DFL != SIG_DFL || SHIM_SIG_IGN != SIG_IGN) return 1;

    if (shim_fputs_line(stdout, "sys_shims smoke test passed") == EOF) {
        return 1;
    }

    return 0;
}
