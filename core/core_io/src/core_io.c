#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

/*
 * core_io.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_io.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum {
    CORE_IO_READ_CHUNK_SIZE = 64 * 1024
};

static bool core_io_has_path(const char *path) {
    return path && path[0];
}

static char *core_io_build_temp_template(const char *path) {
    const char *suffix = ".tmp.XXXXXX";
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);

    if (path_len > (SIZE_MAX - suffix_len - 1u)) {
        return NULL;
    }

    char *temp_path = (char *)core_alloc(path_len + suffix_len + 1u);
    if (!temp_path) {
        return NULL;
    }

    memcpy(temp_path, path, path_len);
    memcpy(temp_path + path_len, suffix, suffix_len + 1u);
    return temp_path;
}

static CoreResult core_io_grow_read_buffer(uint8_t **buffer,
                                           size_t *capacity,
                                           size_t required_size) {
    size_t new_capacity = (*capacity > 0) ? *capacity : CORE_IO_READ_CHUNK_SIZE;
    while (new_capacity < required_size) {
        if (new_capacity > (SIZE_MAX / 2)) {
            new_capacity = SIZE_MAX;
            break;
        }
        new_capacity *= 2;
    }

    if (new_capacity < required_size) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "file too large to fit in memory" };
        return r;
    }

    void *resized = core_realloc(*buffer, new_capacity);
    if (!resized) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    *buffer = (uint8_t *)resized;
    *capacity = new_capacity;
    return core_result_ok();
}

CoreResult core_io_read_all(const char *path, CoreBuffer *out) {
    if (!out) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }
    out->data = NULL;
    out->size = 0;

    if (!core_io_has_path(path)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        CoreResult r = { CORE_ERR_IO, "failed to open file for read" };
        return r;
    }

    uint8_t *buf = NULL;
    size_t size = 0;
    size_t capacity = 0;
    uint8_t chunk[CORE_IO_READ_CHUNK_SIZE];

    for (;;) {
        size_t n = fread(chunk, 1, sizeof(chunk), f);
        if (n > 0) {
            if (size > (SIZE_MAX - n)) {
                core_free(buf);
                fclose(f);
                CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "file too large to fit in memory" };
                return r;
            }

            size_t required_size = size + n;
            if (required_size > capacity) {
                CoreResult grow_result = core_io_grow_read_buffer(&buf, &capacity, required_size);
                if (grow_result.code != CORE_OK) {
                    core_free(buf);
                    fclose(f);
                    return grow_result;
                }
            }

            memcpy(buf + size, chunk, n);
            size += n;
        }

        if (n < sizeof(chunk)) {
            if (ferror(f)) {
                core_free(buf);
                fclose(f);
                CoreResult r = { CORE_ERR_IO, "failed to read file" };
                return r;
            }
            break;
        }
    }

    if (size == 0) {
        core_free(buf);
        buf = NULL;
    } else if (capacity > size) {
        void *resized = core_realloc(buf, size);
        if (!resized) {
            core_free(buf);
            fclose(f);
            CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            return r;
        }
        buf = (uint8_t *)resized;
    }

    if (fclose(f) != 0) {
        core_free(buf);
        CoreResult r = { CORE_ERR_IO, "failed to close file after read" };
        return r;
    }

    out->data = buf;
    out->size = size;
    return core_result_ok();
}

CoreResult core_io_write_all(const char *path, const void *data, size_t size) {
    if (!core_io_has_path(path) || (!data && size > 0)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    FILE *f = fopen(path, "wb");
    if (!f) {
        CoreResult r = { CORE_ERR_IO, "failed to open file for write" };
        return r;
    }

    if (size > 0) {
        size_t n = fwrite(data, 1, size, f);
        if (n != size) {
            fclose(f);
            CoreResult r = { CORE_ERR_IO, "failed to write file" };
            return r;
        }
    }

    if (fclose(f) != 0) {
        CoreResult r = { CORE_ERR_IO, "failed to close file after write" };
        return r;
    }

    return core_result_ok();
}

CoreResult core_io_write_all_atomic(const char *path, const void *data, size_t size) {
    if (!core_io_has_path(path) || (!data && size > 0)) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    char *temp_path = core_io_build_temp_template(path);
    if (!temp_path) {
        CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
        return r;
    }

    int fd = mkstemp(temp_path);
    if (fd < 0) {
        core_free(temp_path);
        CoreResult r = { CORE_ERR_IO, "failed to create temporary file" };
        return r;
    }

    FILE *f = fdopen(fd, "wb");
    if (!f) {
        close(fd);
        unlink(temp_path);
        core_free(temp_path);
        CoreResult r = { CORE_ERR_IO, "failed to open temporary file for write" };
        return r;
    }

    if (size > 0) {
        size_t n = fwrite(data, 1, size, f);
        if (n != size) {
            fclose(f);
            unlink(temp_path);
            core_free(temp_path);
            CoreResult r = { CORE_ERR_IO, "failed to write temporary file" };
            return r;
        }
    }

    if (fclose(f) != 0) {
        unlink(temp_path);
        core_free(temp_path);
        CoreResult r = { CORE_ERR_IO, "failed to close temporary file after write" };
        return r;
    }

    if (rename(temp_path, path) != 0) {
        unlink(temp_path);
        core_free(temp_path);
        CoreResult r = { CORE_ERR_IO, "failed to replace file atomically" };
        return r;
    }

    core_free(temp_path);
    return core_result_ok();
}

void core_io_buffer_free(CoreBuffer *buffer) {
    if (!buffer) return;
    core_free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
}

bool core_io_path_exists(const char *path) {
    struct stat st;
    if (!path || !path[0]) return false;
    return stat(path, &st) == 0;
}
