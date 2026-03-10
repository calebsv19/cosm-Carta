/*
 * core_base.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_base.h"

#include <stdlib.h>
#include <string.h>

void *core_alloc(size_t size) {
    if (size == 0) return NULL;
    return malloc(size);
}

void *core_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;
    return calloc(count, size);
}

void *core_realloc(void *ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

void core_free(void *ptr) {
    free(ptr);
}

CoreStr core_str_from_cstr(const char *cstr) {
    CoreStr s;
    if (!cstr) {
        s.data = NULL;
        s.len = 0;
        return s;
    }
    s.data = cstr;
    s.len = strlen(cstr);
    return s;
}

bool core_str_eq(CoreStr a, CoreStr b) {
    if (a.len != b.len) return false;
    if (a.len == 0) return true;
    if (!a.data || !b.data) return false;
    return memcmp(a.data, b.data, a.len) == 0;
}

uint64_t core_hash64_fnv1a(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

uint32_t core_hash32_fnv1a(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

const char *core_path_basename(const char *path) {
    if (!path || !path[0]) return path;
    const char *last = path;
    for (const char *p = path; *p; ++p) {
        if (*p == '/' || *p == '\\') {
            last = p + 1;
        }
    }
    return last;
}

const char *core_path_ext(const char *path) {
    if (!path || !path[0]) return NULL;
    const char *base = core_path_basename(path);
    const char *dot = NULL;
    for (const char *p = base; *p; ++p) {
        if (*p == '.') dot = p;
    }
    return dot;
}

bool core_is_little_endian(void) {
    uint16_t v = 1;
    return *((uint8_t *)&v) == 1;
}
