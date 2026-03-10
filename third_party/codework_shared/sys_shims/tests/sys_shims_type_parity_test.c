#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "sys_shims/shim_stddef.h"
#include "sys_shims/shim_stdint.h"

struct OffsetProbe {
    int a;
    double b;
};

int main(void) {
    if (sizeof(shim_size_t) != sizeof(size_t)) return 1;
    if (sizeof(shim_ptrdiff_t) != sizeof(ptrdiff_t)) return 1;
    if (sizeof(shim_max_align_t) != sizeof(max_align_t)) return 1;

    if (sizeof(shim_int8_t) != sizeof(int8_t)) return 1;
    if (sizeof(shim_int16_t) != sizeof(int16_t)) return 1;
    if (sizeof(shim_int32_t) != sizeof(int32_t)) return 1;
    if (sizeof(shim_int64_t) != sizeof(int64_t)) return 1;

    if (sizeof(shim_uint8_t) != sizeof(uint8_t)) return 1;
    if (sizeof(shim_uint16_t) != sizeof(uint16_t)) return 1;
    if (sizeof(shim_uint32_t) != sizeof(uint32_t)) return 1;
    if (sizeof(shim_uint64_t) != sizeof(uint64_t)) return 1;

    if (SHIM_INT8_MIN != INT8_MIN || SHIM_INT8_MAX != INT8_MAX || SHIM_UINT8_MAX != UINT8_MAX) return 1;
    if (SHIM_INT16_MIN != INT16_MIN || SHIM_INT16_MAX != INT16_MAX || SHIM_UINT16_MAX != UINT16_MAX) return 1;
    if (SHIM_INT32_MIN != INT32_MIN || SHIM_INT32_MAX != INT32_MAX || SHIM_UINT32_MAX != UINT32_MAX) return 1;
    if (SHIM_INT64_MIN != INT64_MIN || SHIM_INT64_MAX != INT64_MAX || SHIM_UINT64_MAX != UINT64_MAX) return 1;

    if (SHIM_NULL != NULL) return 1;
    if (SHIM_OFFSET_OF(struct OffsetProbe, b) != offsetof(struct OffsetProbe, b)) return 1;

    puts("sys_shims type parity test passed");
    return 0;
}
