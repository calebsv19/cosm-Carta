#ifndef SYS_SHIMS_SHIM_MATH_H
#define SYS_SHIMS_SHIM_MATH_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <errno.h>
#include_next <math.h>
#include_next <stdbool.h>
#else
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#endif

#include "sys_shims/shim_errno.h"

static inline bool shim_isnan_f64(double value) {
    return isnan(value) != 0;
}

static inline bool shim_isinf_f64(double value) {
    return isinf(value) != 0;
}

static inline bool shim_isfinite_f64(double value) {
    return isfinite(value) != 0;
}

static inline bool shim_nearly_equal_abs_f64(double a, double b, double epsilon) {
    if (epsilon < 0.0) epsilon = -epsilon;
    return fabs(a - b) <= epsilon;
}

static inline bool shim_sqrt_checked(double x, double *out_value, shim_errno_t *out_errno) {
    double value;
    if (!out_value) {
        if (out_errno) *out_errno = SHIM_EINVAL;
        return false;
    }
    if (x < 0.0) {
        if (out_errno) *out_errno = EDOM;
        return false;
    }

    errno = 0;
    value = sqrt(x);
    if (errno != 0) {
        if (out_errno) *out_errno = errno;
        return false;
    }

    *out_value = value;
    if (out_errno) *out_errno = 0;
    return true;
}

static inline bool shim_log_checked(double x, double *out_value, shim_errno_t *out_errno) {
    double value;
    if (!out_value) {
        if (out_errno) *out_errno = SHIM_EINVAL;
        return false;
    }
    if (x <= 0.0) {
        if (out_errno) *out_errno = EDOM;
        return false;
    }

    errno = 0;
    value = log(x);
    if (errno != 0) {
        if (out_errno) *out_errno = errno;
        return false;
    }

    *out_value = value;
    if (out_errno) *out_errno = 0;
    return true;
}

static inline bool shim_pow_checked(double base,
                                    double exponent,
                                    double *out_value,
                                    shim_errno_t *out_errno) {
    double value;
    if (!out_value) {
        if (out_errno) *out_errno = SHIM_EINVAL;
        return false;
    }

    errno = 0;
    value = pow(base, exponent);
    if (errno != 0) {
        if (out_errno) *out_errno = errno;
        return false;
    }

    *out_value = value;
    if (out_errno) *out_errno = 0;
    return true;
}

#endif
