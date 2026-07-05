#include "vk_polygon_fill_geometry_guard.h"

#include <stdlib.h>
#include <string.h>

static bool env_value_enabled(const char *value) {
    return value && (strcmp(value, "1") == 0 ||
                     strcmp(value, "true") == 0 ||
                     strcmp(value, "TRUE") == 0 ||
                     strcmp(value, "on") == 0 ||
                     strcmp(value, "ON") == 0);
}

static bool fill_kind_needs_slab_guard(TileLayerKind kind) {
    return kind == TILE_LAYER_POLY_LANDUSE || kind == TILE_LAYER_POLY_BUILDING;
}

bool vk_polygon_fill_kind_allowed_for_retained_mesh(TileLayerKind kind) {
    if (kind == TILE_LAYER_POLY_LANDUSE || kind == TILE_LAYER_POLY_BUILDING) {
        return env_value_enabled(getenv("MAPFORGE_RETAINED_URBAN_FILLS"));
    }
    return true;
}

static bool point_on_tile_boundary(uint16_t x, uint16_t y) {
    const uint16_t kTileExtent = 4096u;
    return x == 0u || x == kTileExtent || y == 0u || y == kTileExtent;
}

static bool edge_on_tile_boundary(const uint16_t *ring_points, uint32_t p0, uint32_t p1) {
    const uint16_t kTileExtent = 4096u;
    uint16_t x0 = ring_points[p0 * 2u];
    uint16_t y0 = ring_points[p0 * 2u + 1u];
    uint16_t x1 = ring_points[p1 * 2u];
    uint16_t y1 = ring_points[p1 * 2u + 1u];
    return (x0 == 0u && x1 == 0u) ||
           (x0 == kTileExtent && x1 == kTileExtent) ||
           (y0 == 0u && y1 == 0u) ||
           (y0 == kTileExtent && y1 == kTileExtent);
}

bool vk_polygon_fill_ring_allowed_for_retained_mesh(TileLayerKind kind,
                                                    const uint16_t *ring_points,
                                                    uint32_t ring_count) {
    if (!vk_polygon_fill_kind_allowed_for_retained_mesh(kind)) {
        return false;
    }
    if (!ring_points || ring_count < 3u || !fill_kind_needs_slab_guard(kind)) {
        return true;
    }

    uint16_t min_x = ring_points[0u];
    uint16_t min_y = ring_points[1u];
    uint16_t max_x = min_x;
    uint16_t max_y = min_y;
    uint32_t boundary_points = 0u;
    uint32_t boundary_edges = 0u;
    for (uint32_t i = 0u; i < ring_count; ++i) {
        uint16_t x = ring_points[i * 2u];
        uint16_t y = ring_points[i * 2u + 1u];
        if (x < min_x) {
            min_x = x;
        }
        if (x > max_x) {
            max_x = x;
        }
        if (y < min_y) {
            min_y = y;
        }
        if (y > max_y) {
            max_y = y;
        }
        if (point_on_tile_boundary(x, y)) {
            boundary_points += 1u;
        }
        if (edge_on_tile_boundary(ring_points, i, (i + 1u) % ring_count)) {
            boundary_edges += 1u;
        }
    }

    const uint64_t tile_area = 4096ull * 4096ull;
    uint64_t width = (uint64_t)(max_x - min_x);
    uint64_t height = (uint64_t)(max_y - min_y);
    uint64_t bbox_area = width * height;
    bool large_bbox = bbox_area * 100ull >= tile_area * 18ull;
    bool medium_bbox = bbox_area * 100ull >= tile_area * 12ull;
    bool boundary_dominated = boundary_points * 4u >= ring_count;
    bool mostly_tile_rect = ring_count <= 8u && bbox_area * 100ull >= tile_area * 15ull &&
                            boundary_points * 2u >= ring_count;

    if (mostly_tile_rect) {
        return false;
    }
    if (large_bbox && boundary_dominated) {
        return false;
    }
    if (medium_bbox && boundary_edges >= 2u) {
        return false;
    }
    return true;
}
