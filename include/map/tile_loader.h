#ifndef MAPFORGE_MAP_TILE_LOADER_H
#define MAPFORGE_MAP_TILE_LOADER_H

#include "map/mft_loader.h"
#include "map/tile_layers.h"
#include "core_queue.h"
#include "core_wake.h"
#include "core_workers.h"

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

typedef struct TileRequest {
    TileCoord coord;
    TileLayerKind kind;
    TileZoomBand band;
    uint32_t request_id;
} TileRequest;

typedef struct TileResult {
    TileCoord coord;
    TileLayerKind kind;
    TileZoomBand band;
    uint32_t request_id;
    bool ok;
    MftTile tile;
} TileResult;

enum {
    TILE_LOADER_REQ_CAPACITY = 1024u,
    TILE_LOADER_RES_CAPACITY = 256u,
    TILE_LOADER_WORKER_THREADS = 1u,
    TILE_LOADER_WORKER_TASK_CAPACITY = 4u
};

typedef struct TileLoader {
    pthread_mutex_t mutex;
    CoreQueueMutex request_queue;
    CoreQueueMutex result_queue;
    void *request_queue_backing[TILE_LOADER_REQ_CAPACITY];
    void *result_queue_backing[TILE_LOADER_RES_CAPACITY];
    CoreWake wake;
    CoreWorkers workers;
    pthread_t worker_threads[TILE_LOADER_WORKER_THREADS];
    CoreWorkerTask worker_tasks[TILE_LOADER_WORKER_TASK_CAPACITY];
    uint64_t enqueued_count;
    uint64_t enqueue_drop_count;
    uint64_t enqueue_evict_count;
    uint64_t produced_count;
    uint64_t result_drop_count;
    uint64_t result_evict_count;
    uint64_t missing_count;
    uint64_t load_ok_count;
    uint64_t load_fail_count;
    uint32_t latest_request_id;
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
    uint64_t enqueue_evict_count;
    uint64_t produced_count;
    uint64_t result_drop_count;
    uint64_t result_evict_count;
    uint64_t missing_count;
    uint64_t load_ok_count;
    uint64_t load_fail_count;
} TileLoaderStats;

bool tile_loader_init(TileLoader *loader, const char *base_dir);
void tile_loader_shutdown(TileLoader *loader);
bool tile_loader_enqueue(TileLoader *loader, TileCoord coord, TileLayerKind kind, TileZoomBand band, uint32_t request_id);
bool tile_loader_pop_result(TileLoader *loader, TileResult *out_result);
void tile_loader_get_stats(TileLoader *loader, TileLoaderStats *out_stats);

#endif
