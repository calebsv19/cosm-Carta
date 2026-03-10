#ifndef SYS_SHIMS_SHIM_STDINT_H
#define SYS_SHIMS_SHIM_STDINT_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <stdint.h>
#else
#include <stdint.h>
#endif

typedef int8_t shim_int8_t;
typedef int16_t shim_int16_t;
typedef int32_t shim_int32_t;
typedef int64_t shim_int64_t;

typedef uint8_t shim_uint8_t;
typedef uint16_t shim_uint16_t;
typedef uint32_t shim_uint32_t;
typedef uint64_t shim_uint64_t;

typedef intptr_t shim_intptr_t;
typedef uintptr_t shim_uintptr_t;

#define SHIM_INT8_MIN INT8_MIN
#define SHIM_INT8_MAX INT8_MAX
#define SHIM_UINT8_MAX UINT8_MAX

#define SHIM_INT16_MIN INT16_MIN
#define SHIM_INT16_MAX INT16_MAX
#define SHIM_UINT16_MAX UINT16_MAX

#define SHIM_INT32_MIN INT32_MIN
#define SHIM_INT32_MAX INT32_MAX
#define SHIM_UINT32_MAX UINT32_MAX

#define SHIM_INT64_MIN INT64_MIN
#define SHIM_INT64_MAX INT64_MAX
#define SHIM_UINT64_MAX UINT64_MAX

#endif
