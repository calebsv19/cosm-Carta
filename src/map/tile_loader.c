#include "map/tile_loader.h"

#include "core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void tile_loader_reset(TileLoader *loader) {
    if (!loader) {
        return;
    }
    memset(loader, 0, sizeof(*loader));
}

static bool tile_loader_request_push(TileLoader *loader, const TileRequest *request) {
    if (!loader || !request || loader->req_count >= loader->req_capacity) {
        return false;
    }

    loader->requests[loader->req_tail] = *request;
    loader->req_tail = (loader->req_tail + 1) % loader->req_capacity;
    loader->req_count += 1;
    return true;
}

static bool tile_loader_request_pop(TileLoader *loader, TileRequest *out_request) {
    if (!loader || !out_request || loader->req_count == 0) {
        return false;
    }

    *out_request = loader->requests[loader->req_head];
    loader->req_head = (loader->req_head + 1) % loader->req_capacity;
    loader->req_count -= 1;
    return true;
}

static bool tile_loader_result_push(TileLoader *loader, const TileResult *result) {
    if (!loader || !result || loader->res_count >= loader->res_capacity) {
        return false;
    }

    loader->results[loader->res_tail] = *result;
    loader->res_tail = (loader->res_tail + 1) % loader->res_capacity;
    loader->res_count += 1;
    return true;
}

static bool tile_loader_result_pop(TileLoader *loader, TileResult *out_result) {
    if (!loader || !out_result || loader->res_count == 0) {
        return false;
    }

    *out_result = loader->results[loader->res_head];
    memset(&loader->results[loader->res_head], 0, sizeof(TileResult));
    loader->res_head = (loader->res_head + 1) % loader->res_capacity;
    loader->res_count -= 1;
    return true;
}

static const char *tile_loader_suffix(TileLayerKind kind) {
    switch (kind) {
        case TILE_LAYER_ROAD_ARTERY:
            return "artery.mft";
        case TILE_LAYER_ROAD_LOCAL:
            return "local.mft";
        case TILE_LAYER_POLYGON:
            return "poly.mft";
        default:
            return "mft";
    }
}

static bool tile_loader_tile_exists(const char *base_dir, TileCoord coord, TileLayerKind kind) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%u/%u/%u.%s", base_dir, coord.z, coord.x, coord.y, tile_loader_suffix(kind));
    FILE *probe = fopen(path, "rb");
    if (!probe) {
        return false;
    }
    fclose(probe);
    return true;
}

static bool tile_loader_load_tile(const char *base_dir, TileCoord coord, TileLayerKind kind, MftTile *out_tile) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%u/%u/%u.%s", base_dir, coord.z, coord.x, coord.y, tile_loader_suffix(kind));
    return mft_load_tile(path, out_tile);
}

static void *tile_loader_thread(void *userdata) {
    TileLoader *loader = (TileLoader *)userdata;
    if (!loader) {
        return NULL;
    }

    for (;;) {
        pthread_mutex_lock(&loader->mutex);
        while (loader->running && loader->req_count == 0) {
            pthread_cond_wait(&loader->cond, &loader->mutex);
        }
        if (!loader->running) {
            pthread_mutex_unlock(&loader->mutex);
            break;
        }

        TileRequest request;
        bool has_request = tile_loader_request_pop(loader, &request);
        pthread_mutex_unlock(&loader->mutex);
        if (!has_request) {
            continue;
        }

        if (!tile_loader_tile_exists(loader->base_dir, request.coord, request.kind)) {
            continue;
        }

        TileResult result = {0};
        result.coord = request.coord;
        result.kind = request.kind;
        result.request_id = request.request_id;
        if (tile_loader_load_tile(loader->base_dir, request.coord, request.kind, &result.tile)) {
            result.ok = true;
        } else {
            log_error("Failed to load tile: %s/%u/%u/%u.%s", loader->base_dir,
                request.coord.z, request.coord.x, request.coord.y, tile_loader_suffix(request.kind));
        }

        if (!result.ok) {
            mft_free_tile(&result.tile);
            continue;
        }

        pthread_mutex_lock(&loader->mutex);
        bool pushed = tile_loader_result_push(loader, &result);
        pthread_mutex_unlock(&loader->mutex);
        if (!pushed) {
            mft_free_tile(&result.tile);
        }
    }

    return NULL;
}

bool tile_loader_init(TileLoader *loader, const char *base_dir) {
    if (!loader || !base_dir) {
        return false;
    }

    memset(loader, 0, sizeof(*loader));
    loader->req_capacity = 1024;
    loader->res_capacity = 256;
    loader->requests = (TileRequest *)calloc(loader->req_capacity, sizeof(TileRequest));
    loader->results = (TileResult *)calloc(loader->res_capacity, sizeof(TileResult));
    if (!loader->requests || !loader->results) {
        free(loader->requests);
        free(loader->results);
        tile_loader_reset(loader);
        return false;
    }

    snprintf(loader->base_dir, sizeof(loader->base_dir), "%s", base_dir);
    pthread_mutex_init(&loader->mutex, NULL);
    pthread_cond_init(&loader->cond, NULL);
    loader->running = true;
    if (pthread_create(&loader->thread, NULL, tile_loader_thread, loader) != 0) {
        tile_loader_shutdown(loader);
        return false;
    }

    return true;
}

void tile_loader_shutdown(TileLoader *loader) {
    if (!loader) {
        return;
    }

    if (loader->running) {
        pthread_mutex_lock(&loader->mutex);
        loader->running = false;
        pthread_cond_broadcast(&loader->cond);
        pthread_mutex_unlock(&loader->mutex);
        pthread_join(loader->thread, NULL);
    }

    pthread_mutex_destroy(&loader->mutex);
    pthread_cond_destroy(&loader->cond);

    if (loader->results) {
        TileResult result = {0};
        while (tile_loader_result_pop(loader, &result)) {
            if (result.ok) {
                mft_free_tile(&result.tile);
            }
            memset(&result, 0, sizeof(result));
        }
    }

    free(loader->requests);
    free(loader->results);
    tile_loader_reset(loader);
}

bool tile_loader_enqueue(TileLoader *loader, TileCoord coord, TileLayerKind kind, uint32_t request_id) {
    if (!loader || !loader->running) {
        return false;
    }

    TileRequest request = {coord, kind, request_id};
    bool ok = false;
    pthread_mutex_lock(&loader->mutex);
    ok = tile_loader_request_push(loader, &request);
    if (ok) {
        pthread_cond_signal(&loader->cond);
    }
    pthread_mutex_unlock(&loader->mutex);
    return ok;
}

bool tile_loader_pop_result(TileLoader *loader, TileResult *out_result) {
    if (!loader || !out_result) {
        return false;
    }

    bool ok = false;
    pthread_mutex_lock(&loader->mutex);
    ok = tile_loader_result_pop(loader, out_result);
    pthread_mutex_unlock(&loader->mutex);
    return ok;
}
