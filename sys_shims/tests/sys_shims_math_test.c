#include <math.h>
#include <stdio.h>

#include "sys_shims/shim_math.h"

static int test_numeric_predicates(void) {
    double nan_value = NAN;
    double inf_value = INFINITY;

    if (!shim_isnan_f64(nan_value)) return 1;
    if (!shim_isinf_f64(inf_value)) return 1;
    if (!shim_isfinite_f64(1.25)) return 1;
    if (shim_isfinite_f64(inf_value)) return 1;
    return 0;
}

static int test_nearly_equal(void) {
    if (!shim_nearly_equal_abs_f64(1.0, 1.0000001, 1e-6)) return 1;
    if (shim_nearly_equal_abs_f64(1.0, 1.1, 1e-3)) return 1;
    return 0;
}

static int test_checked_ops(void) {
    double out = 0.0;
    shim_errno_t err = -1;

    if (!shim_sqrt_checked(9.0, &out, &err)) return 1;
    if (!shim_nearly_equal_abs_f64(out, 3.0, 1e-12) || err != 0) return 1;

    if (shim_sqrt_checked(-1.0, &out, &err)) return 1;
    if (err != EDOM) return 1;

    if (!shim_log_checked(10.0, &out, &err)) return 1;
    if (!shim_nearly_equal_abs_f64(out, log(10.0), 1e-12) || err != 0) return 1;

    if (shim_log_checked(0.0, &out, &err)) return 1;
    if (err != EDOM) return 1;

    if (!shim_pow_checked(2.0, 8.0, &out, &err)) return 1;
    if (!shim_nearly_equal_abs_f64(out, 256.0, 1e-12) || err != 0) return 1;

    return 0;
}

int main(void) {
    if (test_numeric_predicates() != 0) return 1;
    if (test_nearly_equal() != 0) return 1;
    if (test_checked_ops() != 0) return 1;

    puts("sys_shims math test passed");
    return 0;
}
