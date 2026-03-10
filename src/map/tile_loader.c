#include "map/tile_loader.h"

#include "core/log.h"
#include "core_io.h"
#include "map/polygon_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TileLoaderTaskCtx {
    TileLoader *loader;
    TileRequest request;
} TileLoaderTaskCtx;

typedef struct TileLoaderResultNode {
    TileResult result;
} TileLoaderResultNode;

static const char *tile_loader_suffix(TileLayerKind kind) {
    switch (kind) {
        case TILE_LAYER_ROAD_ARTERY:
            return "artery.mft";
        case TILE_LAYER_ROAD_LOCAL:
            return "local.mft";
        case TILE_LAYER_CONTOUR:
            return "contour.mft";
        case TILE_LAYER_POLY_WATER:
            return "water.mft";
        case TILE_LAYER_POLY_PARK:
            return "park.mft";
        case TILE_LAYER_POLY_LANDUSE:
            return "landuse.mft";
        case TILE_LAYER_POLY_BUILDING:
            return "building.mft";
        default:
            return "mft";
    }
}

static const char *tile_loader_band_dir(TileZoomBand band) {
    switch (band) {
        case TILE_BAND_COARSE:
            return "coarse";
        case TILE_BAND_MID:
            return "mid";
        case TILE_BAND_FINE:
            return "fine";
        case TILE_BAND_DEFAULT:
        default:
            return NULL;
    }
}

static bool tile_loader_resolve_path(const char *base_dir,
                                     TileCoord coord,
                                     TileLayerKind kind,
                                     TileZoomBand band,
                                     char *out_path,
                                     size_t out_size) {
    if (!base_dir || !out_path || out_size == 0u) {
        return false;
    }

    const char *band_dir = tile_loader_band_dir(band);
    if (band_dir) {
        char band_path[512];
        snprintf(band_path, sizeof(band_path), "%s/bands/%s/%u/%u/%u.%s",
                 base_dir, band_dir, coord.z, coord.x, coord.y, tile_loader_suffix(kind));
        if (core_io_path_exists(band_path)) {
            snprintf(out_path, out_size, "%s", band_path);
            return true;
        }
    }

    snprintf(out_path, out_size, "%s/%u/%u/%u.%s", base_dir, coord.z, coord.x, coord.y, tile_loader_suffix(kind));
    if (!core_io_path_exists(out_path)) {
        return false;
    }
    return true;
}

static bool tile_loader_take_request(TileLoader *loader, TileLoaderTaskCtx **out_ctx) {
    if (!loader || !out_ctx) {
        return false;
    }
    void *item = NULL;
    if (!core_queue_mutex_pop(&loader->request_queue, &item)) {
        return false;
    }
    if (!item) {
        return false;
    }
    *out_ctx = (TileLoaderTaskCtx *)item;
    return true;
}

static void tile_loader_free_request(TileLoaderTaskCtx *ctx) {
    free(ctx);
}

static void tile_loader_free_result_node(TileLoaderResultNode *node) {
    if (!node) {
        return;
    }
    if (node->result.ok) {
        mft_free_tile(&node->result.tile);
    }
    free(node);
}

static void tile_loader_record_load_stats(TileLoader *loader, bool existed, bool ok) {
    pthread_mutex_lock(&loader->mutex);
    if (!existed) {
        loader->missing_count += 1u;
    } else if (ok) {
        loader->load_ok_count += 1u;
    } else {
        loader->load_fail_count += 1u;
    }
    pthread_mutex_unlock(&loader->mutex);
}

static void tile_loader_publish_result(TileLoader *loader, const TileResult *result) {
    if (!loader || !result) {
        return;
    }

    TileLoaderResultNode *node = (TileLoaderResultNode *)calloc(1u, sizeof(*node));
    if (!node) {
        pthread_mutex_lock(&loader->mutex);
        loader->result_drop_count += 1u;
        pthread_mutex_unlock(&loader->mutex);
        if (result->ok) {
            mft_free_tile((MftTile *)&result->tile);
        }
        return;
    }
    node->result = *result;

    bool pushed = core_queue_mutex_push(&loader->result_queue, node);
    if (!pushed) {
        void *old_item = NULL;
        if (core_queue_mutex_pop(&loader->result_queue, &old_item) && old_item) {
            tile_loader_free_result_node((TileLoaderResultNode *)old_item);
            pthread_mutex_lock(&loader->mutex);
            loader->result_evict_count += 1u;
            pthread_mutex_unlock(&loader->mutex);
            pushed = core_queue_mutex_push(&loader->result_queue, node);
        }
    }

    if (pushed) {
        pthread_mutex_lock(&loader->mutex);
        loader->produced_count += 1u;
        pthread_mutex_unlock(&loader->mutex);
        (void)core_wake_signal(&loader->wake);
    } else {
        pthread_mutex_lock(&loader->mutex);
        loader->result_drop_count += 1u;
        pthread_mutex_unlock(&loader->mutex);
        tile_loader_free_result_node(node);
    }
}

static void tile_loader_process_request(TileLoaderTaskCtx *ctx) {
    if (!ctx || !ctx->loader) {
        tile_loader_free_request(ctx);
        return;
    }

    TileLoader *loader = ctx->loader;
    TileRequest request = ctx->request;
    tile_loader_free_request(ctx);

    TileResult result;
    memset(&result, 0, sizeof(result));
    result.coord = request.coord;
    result.kind = request.kind;
    result.band = request.band;
    result.request_id = request.request_id;

    bool existed = false;
    char path[512];
    if (!tile_loader_resolve_path(loader->base_dir, request.coord, request.kind, request.band, path, sizeof(path))) {
        result.ok = false;
    } else if (mft_load_tile(path, &result.tile)) {
        result.ok = true;
        existed = true;
    } else {
        log_error("Failed to load tile: %s", path);
        result.ok = false;
        existed = true;
    }

    if (result.ok && request.kind == TILE_LAYER_POLY_BUILDING) {
        polygon_cache_build(&result.tile);
    }

    tile_loader_record_load_stats(loader, existed, result.ok);
    tile_loader_publish_result(loader, &result);
}

static void *tile_loader_worker_main(void *task_ctx) {
    TileLoader *loader = (TileLoader *)task_ctx;
    if (!loader) {
        return NULL;
    }

    for (;;) {
        pthread_mutex_lock(&loader->mutex);
        bool running = loader->running;
        pthread_mutex_unlock(&loader->mutex);
        if (!running) {
            break;
        }

        TileLoaderTaskCtx *ctx = NULL;
        bool had_work = false;
        while (tile_loader_take_request(loader, &ctx)) {
            had_work = true;
            tile_loader_process_request(ctx);
        }
        if (had_work) {
            continue;
        }

        CoreWakeWaitResult wait_result = core_wake_wait(&loader->wake, CORE_WAKE_TIMEOUT_INFINITE);
        if (wait_result == CORE_WAKE_WAIT_ERROR) {
            break;
        }
    }

    TileLoaderTaskCtx *ctx = NULL;
    while (tile_loader_take_request(loader, &ctx)) {
        tile_loader_free_request(ctx);
    }
    return NULL;
}

static void tile_loader_reset(TileLoader *loader) {
    if (!loader) {
        return;
    }
    memset(loader, 0, sizeof(*loader));
}

bool tile_loader_init(TileLoader *loader, const char *base_dir) {
    if (!loader || !base_dir) {
        return false;
    }

    memset(loader, 0, sizeof(*loader));
    snprintf(loader->base_dir, sizeof(loader->base_dir), "%s", base_dir);

    if (pthread_mutex_init(&loader->mutex, NULL) != 0) {
        tile_loader_reset(loader);
        return false;
    }
    if (!core_queue_mutex_init(&loader->request_queue, loader->request_queue_backing, TILE_LOADER_REQ_CAPACITY)) {
        pthread_mutex_destroy(&loader->mutex);
        tile_loader_reset(loader);
        return false;
    }
    if (!core_queue_mutex_init(&loader->result_queue, loader->result_queue_backing, TILE_LOADER_RES_CAPACITY)) {
        core_queue_mutex_destroy(&loader->request_queue);
        pthread_mutex_destroy(&loader->mutex);
        tile_loader_reset(loader);
        return false;
    }
    if (!core_wake_init_cond(&loader->wake)) {
        core_queue_mutex_destroy(&loader->result_queue);
        core_queue_mutex_destroy(&loader->request_queue);
        pthread_mutex_destroy(&loader->mutex);
        tile_loader_reset(loader);
        return false;
    }

    loader->running = true;
    if (!core_workers_init(&loader->workers,
                           loader->worker_threads,
                           TILE_LOADER_WORKER_THREADS,
                           loader->worker_tasks,
                           TILE_LOADER_WORKER_TASK_CAPACITY,
                           NULL)) {
        core_wake_shutdown(&loader->wake);
        core_queue_mutex_destroy(&loader->result_queue);
        core_queue_mutex_destroy(&loader->request_queue);
        pthread_mutex_destroy(&loader->mutex);
        tile_loader_reset(loader);
        return false;
    }
    if (!core_workers_submit(&loader->workers, tile_loader_worker_main, loader)) {
        core_workers_shutdown_with_mode(&loader->workers, CORE_WORKERS_SHUTDOWN_CANCEL);
        core_wake_shutdown(&loader->wake);
        core_queue_mutex_destroy(&loader->result_queue);
        core_queue_mutex_destroy(&loader->request_queue);
        pthread_mutex_destroy(&loader->mutex);
        tile_loader_reset(loader);
        return false;
    }

    return true;
}

void tile_loader_shutdown(TileLoader *loader) {
    if (!loader) {
        return;
    }

    pthread_mutex_lock(&loader->mutex);
    bool was_running = loader->running;
    loader->running = false;
    pthread_mutex_unlock(&loader->mutex);

    if (was_running) {
        (void)core_wake_signal(&loader->wake);
    }

    if (loader->workers.initialized) {
        core_workers_shutdown_with_mode(&loader->workers, CORE_WORKERS_SHUTDOWN_DRAIN);
    }

    void *item = NULL;
    while (core_queue_mutex_pop(&loader->request_queue, &item)) {
        tile_loader_free_request((TileLoaderTaskCtx *)item);
    }
    while (core_queue_mutex_pop(&loader->result_queue, &item)) {
        tile_loader_free_result_node((TileLoaderResultNode *)item);
    }

    core_wake_shutdown(&loader->wake);
    core_queue_mutex_destroy(&loader->result_queue);
    core_queue_mutex_destroy(&loader->request_queue);
    pthread_mutex_destroy(&loader->mutex);
    tile_loader_reset(loader);
}

bool tile_loader_enqueue(TileLoader *loader, TileCoord coord, TileLayerKind kind, TileZoomBand band, uint32_t request_id) {
    if (!loader) {
        return false;
    }

    pthread_mutex_lock(&loader->mutex);
    bool running = loader->running;
    if (request_id > loader->latest_request_id) {
        loader->latest_request_id = request_id;
    }
    pthread_mutex_unlock(&loader->mutex);
    if (!running) {
        return false;
    }

    TileLoaderTaskCtx *ctx = (TileLoaderTaskCtx *)calloc(1u, sizeof(*ctx));
    if (!ctx) {
        pthread_mutex_lock(&loader->mutex);
        loader->enqueue_drop_count += 1u;
        pthread_mutex_unlock(&loader->mutex);
        return false;
    }
    ctx->loader = loader;
    ctx->request.coord = coord;
    ctx->request.kind = kind;
    ctx->request.band = band;
    ctx->request.request_id = request_id;

    bool ok = core_queue_mutex_push(&loader->request_queue, ctx);
    if (!ok) {
        void *old_item = NULL;
        if (core_queue_mutex_pop(&loader->request_queue, &old_item) && old_item) {
            tile_loader_free_request((TileLoaderTaskCtx *)old_item);
            pthread_mutex_lock(&loader->mutex);
            loader->enqueue_evict_count += 1u;
            pthread_mutex_unlock(&loader->mutex);
            ok = core_queue_mutex_push(&loader->request_queue, ctx);
        }
    }

    pthread_mutex_lock(&loader->mutex);
    if (ok) {
        loader->enqueued_count += 1u;
    } else {
        loader->enqueue_drop_count += 1u;
    }
    pthread_mutex_unlock(&loader->mutex);

    if (!ok) {
        tile_loader_free_request(ctx);
        return false;
    }

    (void)core_wake_signal(&loader->wake);
    return true;
}

bool tile_loader_pop_result(TileLoader *loader, TileResult *out_result) {
    if (!loader || !out_result) {
        return false;
    }

    void *item = NULL;
    if (!core_queue_mutex_pop(&loader->result_queue, &item)) {
        return false;
    }
    if (!item) {
        return false;
    }

    TileLoaderResultNode *node = (TileLoaderResultNode *)item;
    *out_result = node->result;
    free(node);
    return true;
}

void tile_loader_get_stats(TileLoader *loader, TileLoaderStats *out_stats) {
    if (!loader || !out_stats) {
        return;
    }

    pthread_mutex_lock(&loader->mutex);
    out_stats->req_count = (uint32_t)core_queue_mutex_size(&loader->request_queue);
    out_stats->res_count = (uint32_t)core_queue_mutex_size(&loader->result_queue);
    out_stats->req_capacity = TILE_LOADER_REQ_CAPACITY;
    out_stats->res_capacity = TILE_LOADER_RES_CAPACITY;
    out_stats->enqueued_count = loader->enqueued_count;
    out_stats->enqueue_drop_count = loader->enqueue_drop_count;
    out_stats->enqueue_evict_count = loader->enqueue_evict_count;
    out_stats->produced_count = loader->produced_count;
    out_stats->result_drop_count = loader->result_drop_count;
    out_stats->result_evict_count = loader->result_evict_count;
    out_stats->missing_count = loader->missing_count;
    out_stats->load_ok_count = loader->load_ok_count;
    out_stats->load_fail_count = loader->load_fail_count;
    pthread_mutex_unlock(&loader->mutex);
}
