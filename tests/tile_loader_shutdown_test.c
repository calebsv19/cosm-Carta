#include "map/tile_loader.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    TileLoader loader;
    memset(&loader, 0, sizeof(loader));

    if (!tile_loader_init(&loader, "/tmp")) {
        fprintf(stderr, "tile_loader_init failed\n");
        return 1;
    }

    uint32_t enqueued = 0u;
    for (uint32_t i = 0u; i < 512u; ++i) {
        TileCoord coord = {12u, i % 64u, (i / 64u) % 64u};
        if (tile_loader_enqueue(&loader, coord, TILE_LAYER_ROAD_ARTERY, TILE_BAND_DEFAULT, 1u)) {
            enqueued += 1u;
        }
    }

    if (enqueued == 0u) {
        fprintf(stderr, "no requests enqueued\n");
        tile_loader_shutdown(&loader);
        return 1;
    }

    // Give worker thread a brief chance to move requests/results, then shutdown with pending work.
    usleep(10000);
    tile_loader_shutdown(&loader);

    TileResult result = {0};
    if (tile_loader_pop_result(&loader, &result)) {
        fprintf(stderr, "result queue should be empty after shutdown reset\n");
        return 1;
    }

    // Shutdown must remain idempotent and safe post-reset.
    tile_loader_shutdown(&loader);

    puts("tile_loader_shutdown_test: success");
    return 0;
}
