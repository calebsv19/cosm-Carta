#ifndef SYS_SHIMS_SHIM_STDBOOL_H
#define SYS_SHIMS_SHIM_STDBOOL_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <stdbool.h>
#else
#include <stdbool.h>
#endif

typedef bool shim_bool_t;

#define SHIM_TRUE true
#define SHIM_FALSE false

#endif
