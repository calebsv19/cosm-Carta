#ifndef MAPFORGE_TILE_SOURCE_TEST_FIXTURE_H
#define MAPFORGE_TILE_SOURCE_TEST_FIXTURE_H

#include "map/tile_source.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct MapForgeTileSourceLayerCase {
    TileLayerKind kind;
    const char *layer_key;
    uint8_t marker;
} MapForgeTileSourceLayerCase;

typedef struct MapForgeTileSourceFixture {
    char root[512];
    char cache_dir[512];
    char archive_path[512];
    char missing_tiles_root[512];
    char fallback_tiles_root[512];
    char missing_archive_path[512];
    char previous_cache_dir[512];
    TileCoord coord;
    bool had_previous_cache_dir;
} MapForgeTileSourceFixture;

const MapForgeTileSourceLayerCase *mapforge_tile_source_fixture_layer_cases(size_t *out_count);
bool mapforge_tile_source_fixture_init(MapForgeTileSourceFixture *fixture);
void mapforge_tile_source_fixture_cleanup(MapForgeTileSourceFixture *fixture);
bool mapforge_tile_source_fixture_seed_archive(MapForgeTileSourceFixture *fixture);
bool mapforge_tile_source_fixture_write_tree_tile(const char *tiles_root,
                                                  TileCoord coord,
                                                  TileLayerKind kind,
                                                  uint8_t marker);

#endif
