#include "map/tile_math.h"

#include <math.h>

uint32_t tile_count(uint16_t z) {
    return 1u << z;
}

double tile_size_meters(uint16_t z) {
    return mercator_world_size_meters() / (double)tile_count(z);
}

TileCoord tile_from_meters(uint16_t z, MercatorMeters value) {
    double world_size = mercator_world_size_meters();
    double half_world = world_size * 0.5;
    double size = tile_size_meters(z);
    double count = (double)tile_count(z);
    double max_index = count - 1.0;

    double x = (value.x + half_world) / size;
    double y = (half_world - value.y) / size;

    // Slippy-map X wraps around the world seam.
    if (count > 0.0) {
        x = fmod(x, count);
        if (x < 0.0) {
            x += count;
        }
    }
    if (x > max_index) {
        x = max_index;
    }

    // Y does not wrap in Web Mercator; clamp to valid tile rows.
    if (y < 0.0) {
        y = 0.0;
    }
    if (y > max_index) {
        y = max_index;
    }

    TileCoord tile = {
        z,
        (uint32_t)floor(x),
        (uint32_t)floor(y)
    };
    return tile;
}

MercatorMeters tile_origin_meters(TileCoord tile) {
    double world_size = mercator_world_size_meters();
    double half_world = world_size * 0.5;
    double size = tile_size_meters(tile.z);

    double x = (double)tile.x * size - half_world;
    double y = half_world - (double)tile.y * size;

    MercatorMeters meters = {x, y};
    return meters;
}
