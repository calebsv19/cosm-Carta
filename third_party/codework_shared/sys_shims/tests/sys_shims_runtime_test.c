#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "sys_shims/shim_errno.h"
#include "sys_shims/shim_stdlib.h"
#include "sys_shims/shim_string.h"
#include "sys_shims/shim_time.h"

static int test_stdlib_wrappers(void) {
    int *buf = (int *)shim_calloc(4, sizeof(int));
    long parsed = 0;
    int parse_errno = -1;

    if (!buf) return 1;
    buf[0] = 10;
    buf[1] = 20;

    buf = (int *)shim_realloc(buf, 8 * sizeof(int));
    if (!buf) return 1;

    if (!shim_strtol_checked("1234", 10, &parsed, &parse_errno)) return 1;
    if (parsed != 1234 || parse_errno != 0) return 1;

    if (shim_strtol_checked("12x", 10, &parsed, &parse_errno)) return 1;
    if (parse_errno != EINVAL) return 1;

    if (shim_abs_i(-7) != 7) return 1;

    shim_free(buf);
    return 0;
}

static int test_string_wrappers(void) {
    const char *src = "alpha";
    char dst[16];

    if (!shim_streq("same", "same")) return 1;
    if (shim_streq("same", "other")) return 1;

    if (shim_strnlen_s(src, 16) != 5) return 1;
    if (shim_strnlen_s(src, 3) != 3) return 1;

    memset(dst, 0, sizeof(dst));
    if (!shim_memcpy_checked(dst, sizeof(dst), src, 6)) return 1;
    if (!shim_streq(dst, src)) return 1;

    if (shim_memcpy_checked(dst, 2, src, 6)) return 1;
    return 0;
}

static int test_errno_wrappers(void) {
    shim_errno_clear();
    if (shim_errno_get() != 0) return 1;

    shim_errno_set(SHIM_EINVAL);
    if (shim_errno_get() != SHIM_EINVAL) return 1;

    shim_errno_clear();
    return shim_errno_get() == 0 ? 0 : 1;
}

static int test_time_wrappers(void) {
    time_t now = 0;
    struct tm out_tm;
    char buffer[64];

    memset(&out_tm, 0, sizeof(out_tm));
    memset(buffer, 0, sizeof(buffer));

    if (!shim_time_now(&now)) return 1;
    if (now <= 0) return 1;

    if (!shim_localtime_safe(&now, &out_tm)) return 1;
    if (!shim_time_format_utc(&out_tm, buffer, sizeof(buffer))) return 1;
    if (buffer[0] == '\0') return 1;

    return 0;
}

int main(void) {
    if (test_stdlib_wrappers() != 0) return 1;
    if (test_string_wrappers() != 0) return 1;
    if (test_errno_wrappers() != 0) return 1;
    if (test_time_wrappers() != 0) return 1;

    puts("sys_shims runtime test passed");
    return 0;
}
