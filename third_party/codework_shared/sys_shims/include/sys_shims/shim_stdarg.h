#ifndef SYS_SHIMS_SHIM_STDARG_H
#define SYS_SHIMS_SHIM_STDARG_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <stdarg.h>
#else
#include <stdarg.h>
#endif

typedef va_list shim_va_list;

#define shim_va_start(ap, last) va_start((ap), (last))
#define shim_va_end(ap) va_end((ap))
#define shim_va_arg(ap, type) va_arg((ap), type)

#endif
