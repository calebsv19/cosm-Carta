#include "map/mft_loader.h"

#include "core_io.h"
#include "core/log.h"

#include <stdlib.h>
#include <string.h>

#define MFT_MAGIC "MFT1"
#define MFT_VERSION_BASE 1
#define MFT_VERSION_POLYGONS 2
#define MFT_VERSION_WATER_VARIANTS 3
#define MFT_WATER_VARIANT_COUNT 3
#define MFT_WATER_VARIANT_MAGIC "WSMP"

typedef struct MftCursor {
    const uint8_t *data;
    size_t size;
    size_t pos;
} MftCursor;

static bool cursor_read_bytes(MftCursor *cursor, void *out, size_t bytes) {
    if (!cursor || !out) return false;
    if (bytes > cursor->size - cursor->pos) return false;
    memcpy(out, cursor->data + cursor->pos, bytes);
    cursor->pos += bytes;
    return true;
}

static bool cursor_read_u16(MftCursor *cursor, uint16_t *out) {
    return cursor_read_bytes(cursor, out, sizeof(*out));
}

static bool cursor_read_u32(MftCursor *cursor, uint32_t *out) {
    return cursor_read_bytes(cursor, out, sizeof(*out));
}

static bool cursor_read_u8(MftCursor *cursor, uint8_t *out) {
    return cursor_read_bytes(cursor, out, sizeof(*out));
}

bool mft_load_tile(const char *path, MftTile *out_tile) {
    CoreBuffer file_data = {0};
    CoreResult read_result;
    MftCursor cursor = {0};
    if (!path || !out_tile) {
        return false;
    }

    memset(out_tile, 0, sizeof(*out_tile));

    read_result = core_io_read_all(path, &file_data);
    if (read_result.code != CORE_OK) {
        return false;
    }
    cursor.data = file_data.data;
    cursor.size = file_data.size;
    cursor.pos = 0u;

    char magic[4] = {0};
    if (!cursor_read_bytes(&cursor, magic, sizeof(magic)) || memcmp(magic, MFT_MAGIC, 4) != 0) {
        log_error("MFT invalid magic: %s", path);
        goto fail;
    }

    uint16_t version = 0;
    uint16_t z = 0;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t polyline_count = 0;

    if (!cursor_read_u16(&cursor, &version) || !cursor_read_u16(&cursor, &z) ||
        !cursor_read_u32(&cursor, &x) || !cursor_read_u32(&cursor, &y) ||
        !cursor_read_u32(&cursor, &polyline_count)) {
        log_error("MFT header truncated: %s", path);
        goto fail;
    }

    if (version != MFT_VERSION_BASE &&
        version != MFT_VERSION_POLYGONS &&
        version != MFT_VERSION_WATER_VARIANTS) {
        log_error("MFT unsupported version %u: %s", version, path);
        goto fail;
    }

    out_tile->coord.z = z;
    out_tile->coord.x = x;
    out_tile->coord.y = y;
    out_tile->polyline_count = polyline_count;

    if (polyline_count > 0) {
        out_tile->polylines = (MftPolyline *)calloc(polyline_count, sizeof(MftPolyline));
        if (!out_tile->polylines) {
            goto fail;
        }

        uint32_t total_points = 0;
        for (uint32_t i = 0; i < polyline_count; ++i) {
            uint8_t road_class = 0;
            uint32_t point_count = 0;

            if (!cursor_read_u8(&cursor, &road_class) || !cursor_read_u32(&cursor, &point_count)) {
                log_error("MFT polyline truncated: %s", path);
                goto fail;
            }

            out_tile->polylines[i].road_class = (RoadClass)road_class;
            out_tile->polylines[i].point_count = point_count;
            out_tile->polylines[i].point_offset = total_points;
            total_points += point_count;
        }

        if (total_points > 0) {
            out_tile->points = (uint16_t *)calloc(total_points * 2, sizeof(uint16_t));
            if (!out_tile->points) {
                goto fail;
            }

            if (!cursor_read_bytes(&cursor, out_tile->points, sizeof(uint16_t) * total_points * 2u)) {
                log_error("MFT point data truncated: %s", path);
                goto fail;
            }
        }

        out_tile->point_total = total_points;
    }

    if (version < MFT_VERSION_POLYGONS) {
        core_io_buffer_free(&file_data);
        return true;
    }

    uint32_t polygon_count = 0;
    if (!cursor_read_u32(&cursor, &polygon_count)) {
        log_error("MFT polygon count truncated: %s", path);
        goto fail;
    }

    out_tile->polygon_count = polygon_count;
    if (polygon_count == 0) {
        core_io_buffer_free(&file_data);
        return true;
    }

    out_tile->polygons = (MftPolygon *)calloc(polygon_count, sizeof(MftPolygon));
    if (!out_tile->polygons) {
        goto fail;
    }

    uint32_t ring_total = 0;
    for (uint32_t i = 0; i < polygon_count; ++i) {
        uint8_t polygon_class = 0;
        uint16_t ring_count = 0;
        if (!cursor_read_u8(&cursor, &polygon_class) || !cursor_read_u16(&cursor, &ring_count)) {
            log_error("MFT polygon header truncated: %s", path);
            goto fail;
        }

        out_tile->polygons[i].polygon_class = (PolygonClass)polygon_class;
        out_tile->polygons[i].ring_count = ring_count;
        out_tile->polygons[i].ring_offset = ring_total;
        out_tile->polygons[i].point_offset = 0;
        ring_total += ring_count;
    }

    out_tile->polygon_ring_total = ring_total;
    if (ring_total == 0) {
        core_io_buffer_free(&file_data);
        return true;
    }

    out_tile->polygon_rings = (uint32_t *)calloc(ring_total, sizeof(uint32_t));
    if (!out_tile->polygon_rings) {
        goto fail;
    }

    uint32_t total_polygon_points = 0;
    for (uint32_t i = 0; i < polygon_count; ++i) {
        for (uint16_t r = 0; r < out_tile->polygons[i].ring_count; ++r) {
            uint32_t point_count = 0;
            if (!cursor_read_u32(&cursor, &point_count)) {
                log_error("MFT polygon ring truncated: %s", path);
                goto fail;
            }

            if (r == 0) {
                out_tile->polygons[i].point_offset = total_polygon_points;
            }

            out_tile->polygon_rings[out_tile->polygons[i].ring_offset + r] = point_count;
            total_polygon_points += point_count;
        }
    }

    out_tile->polygon_point_total = total_polygon_points;
    if (total_polygon_points > 0) {
        out_tile->polygon_points = (uint16_t *)calloc(total_polygon_points * 2, sizeof(uint16_t));
        if (!out_tile->polygon_points) {
            goto fail;
        }

        if (!cursor_read_bytes(&cursor, out_tile->polygon_points, sizeof(uint16_t) * total_polygon_points * 2u)) {
            log_error("MFT polygon point data truncated: %s", path);
            goto fail;
        }
    }

    if (version >= MFT_VERSION_WATER_VARIANTS &&
        polygon_count > 0 &&
        out_tile->polygon_ring_total > 0) {
        char variant_magic[4] = {0};
        size_t remaining = cursor.size - cursor.pos;
        size_t got = remaining >= 4u ? 4u : remaining;
        if (got > 0u && !cursor_read_bytes(&cursor, variant_magic, got)) {
            goto fail;
        }
        if (got == 4 && memcmp(variant_magic, MFT_WATER_VARIANT_MAGIC, 4) == 0) {
            uint8_t variant_count = 0;
            if (!cursor_read_u8(&cursor, &variant_count) || variant_count != MFT_WATER_VARIANT_COUNT) {
                log_error("MFT water variant header invalid: %s", path);
                goto fail;
            }
            for (uint32_t v = 0; v < MFT_WATER_VARIANT_COUNT; ++v) {
                uint32_t total_points = 0u;
                uint32_t *rings = (uint32_t *)calloc(out_tile->polygon_ring_total, sizeof(uint32_t));
                if (!rings) {
                    goto fail;
                }
                for (uint32_t i = 0; i < out_tile->polygon_ring_total; ++i) {
                    if (!cursor_read_u32(&cursor, &rings[i])) {
                        log_error("MFT water variant ring data truncated: %s", path);
                        free(rings);
                        goto fail;
                    }
                    total_points += rings[i];
                }
                uint16_t *points = NULL;
                if (total_points > 0u) {
                    points = (uint16_t *)calloc(total_points * 2u, sizeof(uint16_t));
                    if (!points) {
                        free(rings);
                        goto fail;
                    }
                    if (!cursor_read_bytes(&cursor, points, sizeof(uint16_t) * total_points * 2u)) {
                        log_error("MFT water variant points truncated: %s", path);
                        free(points);
                        free(rings);
                        goto fail;
                    }
                }
                out_tile->water_variant_rings[v] = rings;
                out_tile->water_variant_point_total[v] = total_points;
                out_tile->water_variant_points[v] = points;
            }
            out_tile->water_variants_available = true;
        } else if (got == 4) {
            // Unknown extension payload: ignore for forward compatibility.
        }
    }

    core_io_buffer_free(&file_data);
    return true;

fail:
    mft_free_tile(out_tile);
    core_io_buffer_free(&file_data);
    return false;
}

void mft_free_tile(MftTile *tile) {
    if (!tile) {
        return;
    }

    free(tile->polylines);
    free(tile->points);
    free(tile->polygons);
    free(tile->polygon_rings);
    free(tile->polygon_points);
    free(tile->polygon_tri_ring_offsets);
    free(tile->polygon_tri_ring_counts);
    free(tile->polygon_tri_indices);
    for (uint32_t i = 0; i < 3u; ++i) {
        free(tile->water_variant_rings[i]);
        free(tile->water_variant_points[i]);
    }
    memset(tile, 0, sizeof(*tile));
}
