/*
 * core_io.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_io.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

CoreResult core_io_read_all(const char *path, CoreBuffer *out) {
    if (!path || !out) {
        CoreResult r = { CORE_ERR_INVALID_ARG, "invalid argument" };
        return r;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        CoreResult r = { CORE_ERR_IO, "failed to open file for read" };
        return r;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        CoreResult r = { CORE_ERR_IO, "failed to seek file" };
        return r;
    }

    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        CoreResult r = { CORE_ERR_IO, "failed to measure file" };
        return r;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        CoreResult r = { CORE_ERR_IO, "failed to seek file" };
        return r;
    }

    uint8_t *buf = NULL;
    if (sz > 0) {
        buf = (uint8_t *)core_alloc((size_t)sz);
        if (!buf) {
            fclose(f);
            CoreResult r = { CORE_ERR_OUT_OF_MEMORY, "out of memory" };
            return r;
        }
        size_t n = fread(buf, 1, (size_t)sz, f);
        if (n != (size_t)sz) {
            core_free(buf);
            fclose(f);
            CoreResult r = { CORE_ERR_IO, "failed to read file" };
            return r;
        }
    }

    fclose(f);
    out->data = buf;
    out->size = (size_t)sz;
    (void)errno;
    return core_result_ok();
}

CoreResult core_io_write_all(const char *path, const void *data, size_t size) {
    if (!path || (!data && size > 0)) {
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

    fclose(f);
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
