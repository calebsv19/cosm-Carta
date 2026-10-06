#include "map/tile_manager.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static MftTile make_tile(uint16_t z, uint32_t x, uint32_t y) {
    MftTile tile;
    memset(&tile, 0, sizeof(tile));
    tile.coord.z = z;
    tile.coord.x = x;
    tile.coord.y = y;
    return tile;
}

static void put_tile(TileManager *manager, uint16_t z, uint32_t x, uint32_t y, TileZoomBand band) {
    MftTile tile = make_tile(z, x, y);
    TileCoord coord = {z, x, y};
    bool ok = tile_manager_put_tile(manager, coord, band, &tile);
    assert(ok);
}

static void test_trim_policy_preserves_visible_tiles(void) {
    TileSourceConfig source;
    tile_source_config_init(&source);
    assert(tile_source_config_set_filesystem(&source, "."));

    TileManager manager;
    assert(tile_manager_init_with_source(&manager, 16u, &source));

    put_tile(&manager, 15u, 1u, 1u, TILE_BAND_COARSE);
    put_tile(&manager, 15u, 2u, 2u, TILE_BAND_FINE);
    put_tile(&manager, 15u, 9u, 9u, TILE_BAND_COARSE);
    put_tile(&manager, 15u, 10u, 10u, TILE_BAND_MID);
    put_tile(&manager, 15u, 10u, 11u, TILE_BAND_FINE);
    put_tile(&manager, 15u, 12u, 12u, TILE_BAND_FINE);
    put_tile(&manager, 14u, 5u, 5u, TILE_BAND_FINE);
    put_tile(&manager, 15u, 3u, 3u, TILE_BAND_DEFAULT);

    TileManagerTrimPolicy policy;
    memset(&policy, 0, sizeof(policy));
    policy.protect_visible_bounds = true;
    policy.visible_zoom = 15u;
    policy.visible_top_left = (TileCoord){15u, 10u, 10u};
    policy.visible_bottom_right = (TileCoord){15u, 11u, 11u};
    policy.protect_queue_bounds = true;
    policy.queue_zoom = 15u;
    policy.queue_top_left = (TileCoord){15u, 9u, 9u};
    policy.queue_bottom_right = (TileCoord){15u, 12u, 12u};
    policy.prefer_band = true;
    policy.preferred_band = TILE_BAND_FINE;

    uint32_t evicted = tile_manager_trim_to_count(&manager, 5u, &policy);
    assert(evicted == 3u);
    assert(tile_manager_count(&manager) == 5u);

    assert(tile_manager_peek_tile(&manager, (TileCoord){15u, 10u, 10u}, TILE_BAND_MID) != NULL);
    assert(tile_manager_peek_tile(&manager, (TileCoord){15u, 10u, 11u}, TILE_BAND_FINE) != NULL);
    assert(tile_manager_peek_tile(&manager, (TileCoord){15u, 9u, 9u}, TILE_BAND_COARSE) != NULL);

    tile_manager_shutdown(&manager);
}

static void test_trim_without_policy_is_lru(void) {
    TileSourceConfig source;
    tile_source_config_init(&source);
    assert(tile_source_config_set_filesystem(&source, "."));

    TileManager manager;
    assert(tile_manager_init_with_source(&manager, 8u, &source));

    put_tile(&manager, 12u, 1u, 1u, TILE_BAND_DEFAULT);
    put_tile(&manager, 12u, 2u, 2u, TILE_BAND_DEFAULT);
    put_tile(&manager, 12u, 3u, 3u, TILE_BAND_DEFAULT);
    put_tile(&manager, 12u, 4u, 4u, TILE_BAND_DEFAULT);

    uint32_t evicted = tile_manager_trim_to_count(&manager, 2u, NULL);
    assert(evicted == 2u);
    assert(tile_manager_count(&manager) == 2u);

    assert(tile_manager_peek_tile(&manager, (TileCoord){12u, 1u, 1u}, TILE_BAND_DEFAULT) == NULL);
    assert(tile_manager_peek_tile(&manager, (TileCoord){12u, 2u, 2u}, TILE_BAND_DEFAULT) == NULL);
    assert(tile_manager_peek_tile(&manager, (TileCoord){12u, 3u, 3u}, TILE_BAND_DEFAULT) != NULL);
    assert(tile_manager_peek_tile(&manager, (TileCoord){12u, 4u, 4u}, TILE_BAND_DEFAULT) != NULL);

    tile_manager_shutdown(&manager);
}

// Exercises admission refusal and last-visible retention under independent pressure.
static void test_presentation_pressure_and_byte_limit(void) {
    TileSourceConfig source;
    tile_source_config_init(&source);
    assert(tile_source_config_set_filesystem(&source, "."));
    TileManager manager;
    assert(tile_manager_init_with_source(&manager, 2, &source));
    put_tile(&manager, 14, 1, 1, TILE_BAND_MID);
    put_tile(&manager, 14, 2, 2, TILE_BAND_MID);
    manager.entries[0].presentation_pinned = true;
    manager.entries[1].presentation_pinned = true;
    assert(tile_manager_trim_to_count(&manager, 0, NULL) == 0);
    MftTile next = make_tile(14, 3, 3);
    assert(!tile_manager_put_tile(&manager, next.coord, TILE_BAND_MID, &next));
    assert(tile_manager_peek_tile(&manager, (TileCoord){14,1,1}, TILE_BAND_MID));
    manager.entries[1].presentation_pinned = false;
    assert(tile_manager_put_tile(&manager, next.coord, TILE_BAND_MID, &next));
    assert(tile_manager_peek_tile(&manager, (TileCoord){14,1,1}, TILE_BAND_MID));
    manager.byte_limit = 1;
    next = make_tile(14, 4, 4);
    assert(!tile_manager_put_tile(&manager, next.coord, TILE_BAND_MID, &next));
    assert(manager.budget_refusals == 1);
    assert(tile_manager_payload_bytes(&manager) >= 2 * sizeof(TileEntry));
    tile_manager_shutdown(&manager);
}

int main(void) {
    test_presentation_pressure_and_byte_limit();
    test_trim_policy_preserves_visible_tiles();
    test_trim_without_policy_is_lru();
    printf("tile_manager_residency_test: success\n");
    return 0;
}
