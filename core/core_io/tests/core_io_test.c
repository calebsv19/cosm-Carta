#include "core_io.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void cleanup_path(const char *path) {
    if (path) {
        remove(path);
    }
}

static void test_roundtrip_and_path_exists(void) {
    const char *path = "/tmp/core_io_test_roundtrip.bin";
    const char *missing = "/tmp/core_io_test_missing.bin";
    const char payload[] = "core-io-roundtrip";

    cleanup_path(path);
    cleanup_path(missing);

    CoreResult w = core_io_write_all(path, payload, sizeof(payload));
    assert(w.code == CORE_OK);
    assert(core_io_path_exists(path));
    assert(!core_io_path_exists(missing));

    CoreBuffer buf = {0};
    CoreResult r = core_io_read_all(path, &buf);
    assert(r.code == CORE_OK);
    assert(buf.size == sizeof(payload));
    assert(memcmp(buf.data, payload, sizeof(payload)) == 0);

    core_io_buffer_free(&buf);
    assert(buf.data == NULL);
    assert(buf.size == 0);

    cleanup_path(path);
}

static void test_empty_file_and_zero_byte_write(void) {
    const char *path = "/tmp/core_io_test_empty.bin";
    CoreBuffer buf = { (uint8_t *)0x1, 7 };

    cleanup_path(path);

    CoreResult w = core_io_write_all(path, NULL, 0);
    assert(w.code == CORE_OK);
    assert(core_io_path_exists(path));

    CoreResult r = core_io_read_all(path, &buf);
    assert(r.code == CORE_OK);
    assert(buf.data == NULL);
    assert(buf.size == 0);

    core_io_buffer_free(&buf);
    cleanup_path(path);
}

static void test_invalid_arguments(void) {
    const char payload[] = "x";
    CoreBuffer buf = { (uint8_t *)0x1, 7 };

    assert(core_io_read_all(NULL, &buf).code == CORE_ERR_INVALID_ARG);
    assert(buf.data == NULL);
    assert(buf.size == 0);

    buf.data = (uint8_t *)0x1;
    buf.size = 7;
    assert(core_io_read_all("", &buf).code == CORE_ERR_INVALID_ARG);
    assert(buf.data == NULL);
    assert(buf.size == 0);

    assert(core_io_read_all("/tmp/core_io_test_invalid.bin", NULL).code == CORE_ERR_INVALID_ARG);
    assert(core_io_write_all(NULL, payload, sizeof(payload)).code == CORE_ERR_INVALID_ARG);
    assert(core_io_write_all("", payload, sizeof(payload)).code == CORE_ERR_INVALID_ARG);
    assert(core_io_write_all("/tmp/core_io_test_invalid.bin", NULL, 1).code == CORE_ERR_INVALID_ARG);
}

static void test_missing_path_read(void) {
    const char *missing = "/tmp/core_io_test_missing_read.bin";
    CoreBuffer buf = { (uint8_t *)0x1, 7 };

    cleanup_path(missing);
    assert(!core_io_path_exists(missing));
    assert(core_io_read_all(missing, &buf).code == CORE_ERR_IO);
    assert(buf.data == NULL);
    assert(buf.size == 0);
}

static void test_large_file_read(void) {
    const char *path = "/tmp/core_io_test_large.bin";
    const size_t payload_size = (64u * 1024u * 3u) + 123u;
    uint8_t *payload = (uint8_t *)malloc(payload_size);
    CoreBuffer buf = {0};

    assert(payload != NULL);
    for (size_t i = 0; i < payload_size; ++i) {
        payload[i] = (uint8_t)(i % 251u);
    }

    cleanup_path(path);
    assert(core_io_write_all(path, payload, payload_size).code == CORE_OK);
    assert(core_io_read_all(path, &buf).code == CORE_OK);
    assert(buf.size == payload_size);
    assert(memcmp(buf.data, payload, payload_size) == 0);

    core_io_buffer_free(&buf);
    free(payload);
    cleanup_path(path);
}

static void test_atomic_write_replace(void) {
    const char *path = "/tmp/core_io_test_atomic.bin";
    const char original[] = "original";
    const char replacement[] = "replacement";
    CoreBuffer buf = {0};

    cleanup_path(path);
    assert(core_io_write_all(path, original, sizeof(original)).code == CORE_OK);
    assert(core_io_write_all_atomic(path, replacement, sizeof(replacement)).code == CORE_OK);
    assert(core_io_read_all(path, &buf).code == CORE_OK);
    assert(buf.size == sizeof(replacement));
    assert(memcmp(buf.data, replacement, sizeof(replacement)) == 0);

    core_io_buffer_free(&buf);
    cleanup_path(path);
}

static void test_atomic_write_create_and_invalid_args(void) {
    const char *path = "/tmp/core_io_test_atomic_create.bin";
    const char payload[] = "atomic-create";
    CoreBuffer buf = {0};

    cleanup_path(path);
    assert(core_io_write_all_atomic(path, payload, sizeof(payload)).code == CORE_OK);
    assert(core_io_path_exists(path));
    assert(core_io_read_all(path, &buf).code == CORE_OK);
    assert(buf.size == sizeof(payload));
    assert(memcmp(buf.data, payload, sizeof(payload)) == 0);
    core_io_buffer_free(&buf);

    assert(core_io_write_all_atomic(NULL, payload, sizeof(payload)).code == CORE_ERR_INVALID_ARG);
    assert(core_io_write_all_atomic("", payload, sizeof(payload)).code == CORE_ERR_INVALID_ARG);
    assert(core_io_write_all_atomic(path, NULL, 1).code == CORE_ERR_INVALID_ARG);

    cleanup_path(path);
}

int main(void) {
    test_roundtrip_and_path_exists();
    test_empty_file_and_zero_byte_write();
    test_invalid_arguments();
    test_missing_path_read();
    test_large_file_read();
    test_atomic_write_replace();
    test_atomic_write_create_and_invalid_args();
    return 0;
}
