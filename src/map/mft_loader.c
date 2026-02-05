#include "map/mft_loader.h"

#include "core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MFT_MAGIC "MFT1"

static bool read_u16(FILE *file, uint16_t *out) {
    return fread(out, sizeof(uint16_t), 1, file) == 1;
}

static bool read_u32(FILE *file, uint32_t *out) {
    return fread(out, sizeof(uint32_t), 1, file) == 1;
}

bool mft_load_tile(const char *path, MftTile *out_tile) {
    if (!path || !out_tile) {
        return false;
    }

    memset(out_tile, 0, sizeof(*out_tile));

    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }

    char magic[4] = {0};
    if (fread(magic, sizeof(magic), 1, file) != 1 || memcmp(magic, MFT_MAGIC, 4) != 0) {
        log_error("MFT invalid magic: %s", path);
        fclose(file);
        return false;
    }

    uint16_t version = 0;
    uint16_t z = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t polyline_count = 0;

    if (!read_u16(file, &version) || !read_u16(file, &z) || !read_u32(file, &x) || !read_u32(file, &y) || !read_u32(file, &polyline_count)) {
        log_error("MFT header truncated: %s", path);
        fclose(file);
        return false;
    }

    if (version != 1) {
        log_error("MFT unsupported version %u: %s", version, path);
        fclose(file);
        return false;
    }

    out_tile->coord.z = z;
    out_tile->coord.x = x;
    out_tile->coord.y = y;
    out_tile->polyline_count = polyline_count;

    if (polyline_count == 0) {
        fclose(file);
        return true;
    }

    out_tile->polylines = (MftPolyline *)calloc(polyline_count, sizeof(MftPolyline));
    if (!out_tile->polylines) {
        fclose(file);
        return false;
    }

    uint32_t total_points = 0;
    for (uint32_t i = 0; i < polyline_count; ++i) {
        uint8_t road_class = 0;
        uint32_t point_count = 0;

        if (fread(&road_class, sizeof(uint8_t), 1, file) != 1 || !read_u32(file, &point_count)) {
            log_error("MFT polyline truncated: %s", path);
            mft_free_tile(out_tile);
            fclose(file);
            return false;
        }

        out_tile->polylines[i].road_class = (RoadClass)road_class;
        out_tile->polylines[i].point_count = point_count;
        out_tile->polylines[i].point_offset = total_points;
        total_points += point_count;
    }

    if (total_points > 0) {
        out_tile->points = (uint16_t *)calloc(total_points * 2, sizeof(uint16_t));
        if (!out_tile->points) {
            mft_free_tile(out_tile);
            fclose(file);
            return false;
        }

        if (fread(out_tile->points, sizeof(uint16_t), total_points * 2, file) != total_points * 2) {
            log_error("MFT point data truncated: %s", path);
            mft_free_tile(out_tile);
            fclose(file);
            return false;
        }
    }

    out_tile->point_total = total_points;
    fclose(file);
    return true;
}

void mft_free_tile(MftTile *tile) {
    if (!tile) {
        return;
    }

    free(tile->polylines);
    free(tile->points);
    memset(tile, 0, sizeof(*tile));
}
