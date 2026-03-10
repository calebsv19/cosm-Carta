#ifndef CORE_BASE_H
#define CORE_BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum CoreError {
    CORE_OK = 0,
    CORE_ERR_INVALID_ARG = 1,
    CORE_ERR_OUT_OF_MEMORY = 2,
    CORE_ERR_IO = 3,
    CORE_ERR_FORMAT = 4,
    CORE_ERR_NOT_FOUND = 5
} CoreError;

typedef struct CoreResult {
    CoreError code;
    const char *message;
} CoreResult;

typedef struct CoreStr {
    const char *data;
    size_t len;
} CoreStr;

static inline CoreResult core_result_ok(void) {
    CoreResult r = { CORE_OK, "ok" };
    return r;
}

void *core_alloc(size_t size);
void *core_calloc(size_t count, size_t size);
void *core_realloc(void *ptr, size_t size);
void core_free(void *ptr);

CoreStr core_str_from_cstr(const char *cstr);
bool core_str_eq(CoreStr a, CoreStr b);

uint64_t core_hash64_fnv1a(const void *data, size_t len);
uint32_t core_hash32_fnv1a(const void *data, size_t len);

const char *core_path_basename(const char *path);
const char *core_path_ext(const char *path);

bool core_is_little_endian(void);

#endif
