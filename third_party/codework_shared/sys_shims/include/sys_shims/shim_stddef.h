#ifndef SYS_SHIMS_SHIM_STDDEF_H
#define SYS_SHIMS_SHIM_STDDEF_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <stddef.h>
#else
#include <stddef.h>
#endif

typedef size_t shim_size_t;
typedef ptrdiff_t shim_ptrdiff_t;

typedef max_align_t shim_max_align_t;

#define SHIM_NULL NULL
#define SHIM_OFFSET_OF(type, member) offsetof(type, member)

#endif
