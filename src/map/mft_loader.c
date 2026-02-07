#include "map/mft_loader.h"

#include "core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MFT_MAGIC "MFT1"
#define MFT_VERSION_BASE 1
#define MFT_VERSION_POLYGONS 2
#define MFT_VERSION_WATER_VARIANTS 3
#define MFT_WATER_VARIANT_COUNT 3
#define MFT_WATER_VARIANT_MAGIC "WSMP"

static bool read_u16(FILE *file, uint16_t *out) {
    return fread(out, sizeof(uint16_t), 1, file) == 1;
}

static bool read_u32(FILE *file, uint32_t *out) {
    return fread(out, sizeof(uint32_t), 1, file) == 1;
}

static bool read_u8(FILE *file, uint8_t *out) {
    return fread(out, sizeof(uint8_t), 1, file) == 1;
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

    if (version != MFT_VERSION_BASE &&
        version != MFT_VERSION_POLYGONS &&
        version != MFT_VERSION_WATER_VARIANTS) {
        log_error("MFT unsupported version %u: %s", version, path);
        fclose(file);
        return false;
    }

    out_tile->coord.z = z;
    out_tile->coord.x = x;
    out_tile->coord.y = y;
    out_tile->polyline_count = polyline_count;

    if (polyline_count > 0) {
        out_tile->polylines = (MftPolyline *)calloc(polyline_count, sizeof(MftPolyline));
        if (!out_tile->polylines) {
            fclose(file);
            return false;
        }

        uint32_t total_points = 0;
        for (uint32_t i = 0; i < polyline_count; ++i) {
            uint8_t road_class = 0;
            uint32_t point_count = 0;

            if (!read_u8(file, &road_class) || !read_u32(file, &point_count)) {
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
    }

    if (version < MFT_VERSION_POLYGONS) {
        fclose(file);
        return true;
    }

    uint32_t polygon_count = 0;
    if (!read_u32(file, &polygon_count)) {
        log_error("MFT polygon count truncated: %s", path);
        mft_free_tile(out_tile);
        fclose(file);
        return false;
    }

    out_tile->polygon_count = polygon_count;
    if (polygon_count == 0) {
        fclose(file);
        return true;
    }

    out_tile->polygons = (MftPolygon *)calloc(polygon_count, sizeof(MftPolygon));
    if (!out_tile->polygons) {
        mft_free_tile(out_tile);
        fclose(file);
        return false;
    }

    uint32_t ring_total = 0;
    for (uint32_t i = 0; i < polygon_count; ++i) {
        uint8_t polygon_class = 0;
        uint16_t ring_count = 0;
        if (!read_u8(file, &polygon_class) || !read_u16(file, &ring_count)) {
            log_error("MFT polygon header truncated: %s", path);
            mft_free_tile(out_tile);
            fclose(file);
            return false;
        }

        out_tile->polygons[i].polygon_class = (PolygonClass)polygon_class;
        out_tile->polygons[i].ring_count = ring_count;
        out_tile->polygons[i].ring_offset = ring_total;
        out_tile->polygons[i].point_offset = 0;
        ring_total += ring_count;
    }

    out_tile->polygon_ring_total = ring_total;
    if (ring_total == 0) {
        fclose(file);
        return true;
    }

    out_tile->polygon_rings = (uint32_t *)calloc(ring_total, sizeof(uint32_t));
    if (!out_tile->polygon_rings) {
        mft_free_tile(out_tile);
        fclose(file);
        return false;
    }

    uint32_t total_polygon_points = 0;
    for (uint32_t i = 0; i < polygon_count; ++i) {
        for (uint16_t r = 0; r < out_tile->polygons[i].ring_count; ++r) {
            uint32_t point_count = 0;
            if (!read_u32(file, &point_count)) {
                log_error("MFT polygon ring truncated: %s", path);
                mft_free_tile(out_tile);
                fclose(file);
                return false;
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
            mft_free_tile(out_tile);
            fclose(file);
            return false;
        }

        if (fread(out_tile->polygon_points, sizeof(uint16_t), total_polygon_points * 2, file) != total_polygon_points * 2) {
            log_error("MFT polygon point data truncated: %s", path);
            mft_free_tile(out_tile);
            fclose(file);
            return false;
        }
    }

    if (version >= MFT_VERSION_WATER_VARIANTS &&
        polygon_count > 0 &&
        out_tile->polygon_ring_total > 0) {
        char variant_magic[4] = {0};
        size_t got = fread(variant_magic, sizeof(char), 4, file);
        if (got == 4 && memcmp(variant_magic, MFT_WATER_VARIANT_MAGIC, 4) == 0) {
            uint8_t variant_count = 0;
            if (!read_u8(file, &variant_count) || variant_count != MFT_WATER_VARIANT_COUNT) {
                log_error("MFT water variant header invalid: %s", path);
                mft_free_tile(out_tile);
                fclose(file);
                return false;
            }
            for (uint32_t v = 0; v < MFT_WATER_VARIANT_COUNT; ++v) {
                uint32_t total_points = 0u;
                uint32_t *rings = (uint32_t *)calloc(out_tile->polygon_ring_total, sizeof(uint32_t));
                if (!rings) {
                    mft_free_tile(out_tile);
                    fclose(file);
                    return false;
                }
                for (uint32_t i = 0; i < out_tile->polygon_ring_total; ++i) {
                    if (!read_u32(file, &rings[i])) {
                        log_error("MFT water variant ring data truncated: %s", path);
                        free(rings);
                        mft_free_tile(out_tile);
                        fclose(file);
                        return false;
                    }
                    total_points += rings[i];
                }
                uint16_t *points = NULL;
                if (total_points > 0u) {
                    points = (uint16_t *)calloc(total_points * 2u, sizeof(uint16_t));
                    if (!points) {
                        free(rings);
                        mft_free_tile(out_tile);
                        fclose(file);
                        return false;
                    }
                    if (fread(points, sizeof(uint16_t), total_points * 2u, file) != total_points * 2u) {
                        log_error("MFT water variant points truncated: %s", path);
                        free(points);
                        free(rings);
                        mft_free_tile(out_tile);
                        fclose(file);
                        return false;
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

    fclose(file);
    return true;
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
