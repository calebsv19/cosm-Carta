#include "map/mft_loader.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void append_u8(uint8_t *blob, size_t cap, size_t *pos, uint8_t value) {
    assert(blob && pos);
    assert(*pos + sizeof(value) <= cap);
    blob[*pos] = value;
    *pos += sizeof(value);
}

static void append_u16(uint8_t *blob, size_t cap, size_t *pos, uint16_t value) {
    assert(blob && pos);
    assert(*pos + sizeof(value) <= cap);
    memcpy(blob + *pos, &value, sizeof(value));
    *pos += sizeof(value);
}

static void append_u32(uint8_t *blob, size_t cap, size_t *pos, uint32_t value) {
    assert(blob && pos);
    assert(*pos + sizeof(value) <= cap);
    memcpy(blob + *pos, &value, sizeof(value));
    *pos += sizeof(value);
}

static bool write_blob_file(const uint8_t *blob, size_t size, char *path_template) {
    assert(blob && path_template);
    int fd = mkstemp(path_template);
    if (fd < 0) {
        return false;
    }

    size_t written = 0u;
    while (written < size) {
        ssize_t rc = write(fd, blob + written, size - written);
        if (rc <= 0) {
            close(fd);
            unlink(path_template);
            return false;
        }
        written += (size_t)rc;
    }

    if (close(fd) != 0) {
        unlink(path_template);
        return false;
    }
    return true;
}

static void test_polygon_point_accumulation_overflow_rejected(void) {
    uint8_t blob[64];
    size_t pos = 0u;
    char path_template[] = "/tmp/mapforge_mft_overflow_XXXXXX";
    MftTile tile;
    memset(&tile, 0, sizeof(tile));

    memcpy(blob + pos, "MFT1", 4u);
    pos += 4u;
    append_u16(blob, sizeof(blob), &pos, 2u);
    append_u16(blob, sizeof(blob), &pos, 8u);
    append_u32(blob, sizeof(blob), &pos, 42u);
    append_u32(blob, sizeof(blob), &pos, 17u);
    append_u32(blob, sizeof(blob), &pos, 0u);
    append_u32(blob, sizeof(blob), &pos, 1u);
    append_u8(blob, sizeof(blob), &pos, (uint8_t)POLYGON_CLASS_BUILDING);
    append_u16(blob, sizeof(blob), &pos, 2u);
    append_u32(blob, sizeof(blob), &pos, UINT32_MAX);
    append_u32(blob, sizeof(blob), &pos, 1u);

    assert(write_blob_file(blob, pos, path_template));
    assert(!mft_load_tile(path_template, &tile));
    assert(tile.polygon_points == NULL);
    assert(tile.polygon_rings == NULL);
    assert(tile.polygon_ring_total == 0u);
    assert(tile.polygons == NULL);

    mft_free_tile(&tile);
    assert(unlink(path_template) == 0);
}

int main(void) {
    test_polygon_point_accumulation_overflow_rejected();
    printf("mft_loader_security_test: success\n");
    return 0;
}
