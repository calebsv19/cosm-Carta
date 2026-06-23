#include "map/tile_source.h"

#include "core_io.h"
#include "tile_source_test_fixture.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#if !defined(MAPFORGE_HAVE_SQLITE)
int main(void) {
    printf("tile_source_archive_test: skipped (sqlite disabled)\n");
    return 0;
}
#else

int main(void) {
    MapForgeTileSourceFixture fixture;
    size_t case_count = 0u;
    const MapForgeTileSourceLayerCase *cases = mapforge_tile_source_fixture_layer_cases(&case_count);
    if (!mapforge_tile_source_fixture_init(&fixture)) {
        fprintf(stderr, "failed to initialize tile-source fixture\n");
        return 1;
    }
    if (!mapforge_tile_source_fixture_seed_archive(&fixture)) {
        fprintf(stderr, "failed to seed archive db\n");
        mapforge_tile_source_fixture_cleanup(&fixture);
        return 1;
    }

    TileSourceConfig source = {0};
    assert(tile_source_config_set_archive(&source, fixture.missing_tiles_root, fixture.archive_path));
    tile_source_runtime_stats_reset();

    for (int pass = 0; pass < 2; ++pass) {
        for (size_t i = 0u; i < case_count; ++i) {
            char resolved[MAPFORGE_TILE_SOURCE_PATH_CAPACITY];
            assert(tile_source_resolve_path(&source,
                                            fixture.coord,
                                            cases[i].kind,
                                            TILE_BAND_DEFAULT,
                                            resolved,
                                            sizeof(resolved)));
            assert(core_io_path_exists(resolved));
            CoreBuffer readback = {0};
            CoreResult rr = core_io_read_all(resolved, &readback);
            assert(rr.code == CORE_OK);
            assert(readback.size > 0u);
            const uint8_t *bytes = (const uint8_t *)readback.data;
            assert(bytes[readback.size - 1u] == cases[i].marker);
            core_io_buffer_free(&readback);
        }
    }

    TileSourceRuntimeStats stats = {0};
    tile_source_runtime_stats_get(&stats);
    assert(stats.archive_request_count == (uint64_t)case_count * 2u);
    assert(stats.archive_hit_count == (uint64_t)case_count * 2u);
    assert(stats.archive_extract_count == (uint64_t)case_count);
    assert(stats.archive_extract_fail_count == 0u);
    assert(stats.archive_fallback_tree_count == 0u);
    assert(stats.archive_policy_block_count == 0u);

    assert(mapforge_tile_source_fixture_write_tree_tile(fixture.fallback_tiles_root,
                                                        fixture.coord,
                                                        TILE_LAYER_ROAD_ARTERY,
                                                        0x91));

    TileSourceConfig preferred_source = {0};
    assert(tile_source_config_set_archive_with_policy(&preferred_source,
                                                      fixture.fallback_tiles_root,
                                                      fixture.missing_archive_path,
                                                      TILE_SOURCE_POLICY_ARCHIVE_PREFERRED));
    tile_source_runtime_stats_reset();
    {
        char resolved[MAPFORGE_TILE_SOURCE_PATH_CAPACITY];
        assert(tile_source_resolve_path(&preferred_source,
                                        fixture.coord,
                                        TILE_LAYER_ROAD_ARTERY,
                                        TILE_BAND_DEFAULT,
                                        resolved,
                                        sizeof(resolved)));
        CoreBuffer readback = {0};
        CoreResult rr = core_io_read_all(resolved, &readback);
        assert(rr.code == CORE_OK);
        assert(readback.size > 0u);
        const uint8_t *bytes = (const uint8_t *)readback.data;
        assert(bytes[readback.size - 1u] == 0x91);
        core_io_buffer_free(&readback);
    }
    tile_source_runtime_stats_get(&stats);
    assert(stats.archive_request_count == 1u);
    assert(stats.archive_hit_count == 0u);
    assert(stats.archive_extract_count == 0u);
    assert(stats.archive_extract_fail_count == 1u);
    assert(stats.archive_fallback_tree_count == 1u);
    assert(stats.archive_policy_block_count == 0u);

    TileSourceConfig required_source = {0};
    assert(tile_source_config_set_archive_with_policy(&required_source,
                                                      fixture.fallback_tiles_root,
                                                      fixture.missing_archive_path,
                                                      TILE_SOURCE_POLICY_ARCHIVE_REQUIRED));
    tile_source_runtime_stats_reset();
    {
        char resolved[MAPFORGE_TILE_SOURCE_PATH_CAPACITY];
        assert(!tile_source_resolve_path(&required_source,
                                         fixture.coord,
                                         TILE_LAYER_ROAD_ARTERY,
                                         TILE_BAND_DEFAULT,
                                         resolved,
                                         sizeof(resolved)));
    }
    tile_source_runtime_stats_get(&stats);
    assert(stats.archive_request_count == 1u);
    assert(stats.archive_hit_count == 0u);
    assert(stats.archive_extract_count == 0u);
    assert(stats.archive_extract_fail_count == 1u);
    assert(stats.archive_fallback_tree_count == 0u);
    assert(stats.archive_policy_block_count == 1u);

    TileSourceConfig filesystem_only_source = {0};
    assert(tile_source_config_set_archive_with_policy(&filesystem_only_source,
                                                      fixture.fallback_tiles_root,
                                                      fixture.missing_archive_path,
                                                      TILE_SOURCE_POLICY_FILESYSTEM_ONLY));
    tile_source_runtime_stats_reset();
    {
        char resolved[MAPFORGE_TILE_SOURCE_PATH_CAPACITY];
        assert(tile_source_resolve_path(&filesystem_only_source,
                                        fixture.coord,
                                        TILE_LAYER_ROAD_ARTERY,
                                        TILE_BAND_DEFAULT,
                                        resolved,
                                        sizeof(resolved)));
    }
    tile_source_runtime_stats_get(&stats);
    assert(stats.archive_request_count == 0u);
    assert(stats.archive_hit_count == 0u);
    assert(stats.archive_extract_count == 0u);
    assert(stats.archive_extract_fail_count == 0u);
    assert(stats.archive_fallback_tree_count == 0u);
    assert(stats.archive_policy_block_count == 0u);

    mapforge_tile_source_fixture_cleanup(&fixture);
    printf("tile_source_archive_test: success\n");
    return 0;
}
#endif
