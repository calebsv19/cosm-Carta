#ifndef SYS_SHIMS_SHIM_LIMITS_H
#define SYS_SHIMS_SHIM_LIMITS_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <limits.h>
#else
#include <limits.h>
#endif

#define SHIM_CHAR_BIT CHAR_BIT
#define SHIM_SCHAR_MIN SCHAR_MIN
#define SHIM_SCHAR_MAX SCHAR_MAX
#define SHIM_UCHAR_MAX UCHAR_MAX
#define SHIM_INT_MAX INT_MAX
#define SHIM_INT_MIN INT_MIN
#define SHIM_LONG_MAX LONG_MAX
#define SHIM_LONG_MIN LONG_MIN

#endif
