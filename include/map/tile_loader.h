#ifndef MAPFORGE_MAP_TILE_LOADER_H
#define MAPFORGE_MAP_TILE_LOADER_H

#include "map/mft_loader.h"
#include "map/tile_layers.h"

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

typedef struct TileRequest {
    TileCoord coord;
    TileLayerKind kind;
    uint32_t request_id;
} TileRequest;

typedef struct TileResult {
    TileCoord coord;
    TileLayerKind kind;
    uint32_t request_id;
    bool ok;
    MftTile tile;
} TileResult;

typedef struct TileLoader {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    TileRequest *requests;
    TileResult *results;
    uint32_t req_head;
    uint32_t req_tail;
    uint32_t req_count;
    uint32_t res_head;
    uint32_t res_tail;
    uint32_t res_count;
    uint32_t req_capacity;
    uint32_t res_capacity;
    uint64_t enqueued_count;
    uint64_t enqueue_drop_count;
    uint64_t produced_count;
    uint64_t result_drop_count;
    uint64_t missing_count;
    uint64_t load_ok_count;
    uint64_t load_fail_count;
    bool running;
    char base_dir[256];
} TileLoader;

typedef struct TileLoaderStats {
    uint32_t req_count;
    uint32_t res_count;
    uint32_t req_capacity;
    uint32_t res_capacity;
    uint64_t enqueued_count;
    uint64_t enqueue_drop_count;
    uint64_t produced_count;
    uint64_t result_drop_count;
    uint64_t missing_count;
    uint64_t load_ok_count;
    uint64_t load_fail_count;
} TileLoaderStats;

bool tile_loader_init(TileLoader *loader, const char *base_dir);
void tile_loader_shutdown(TileLoader *loader);
bool tile_loader_enqueue(TileLoader *loader, TileCoord coord, TileLayerKind kind, uint32_t request_id);
bool tile_loader_pop_result(TileLoader *loader, TileResult *out_result);
void tile_loader_get_stats(TileLoader *loader, TileLoaderStats *out_stats);

#endif
