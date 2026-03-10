#ifndef CORE_IO_H
#define CORE_IO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core_base.h"

typedef size_t (*CoreStreamReadFn)(void *user, void *dst, size_t bytes);
typedef size_t (*CoreStreamWriteFn)(void *user, const void *src, size_t bytes);

typedef struct CoreReader {
    void *user;
    CoreStreamReadFn read;
} CoreReader;

typedef struct CoreWriter {
    void *user;
    CoreStreamWriteFn write;
} CoreWriter;

typedef struct CoreBuffer {
    uint8_t *data;
    size_t size;
} CoreBuffer;

CoreResult core_io_read_all(const char *path, CoreBuffer *out);
CoreResult core_io_write_all(const char *path, const void *data, size_t size);
void core_io_buffer_free(CoreBuffer *buffer);
bool core_io_path_exists(const char *path);

#endif
