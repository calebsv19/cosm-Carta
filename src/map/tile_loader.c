#include "map/tile_loader.h"

#include "core/log.h"
#include "core_io.h"
#include "map/polygon_cache.h"

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

static bool tile_loader_request_evict_at(TileLoader *loader, uint32_t offset) {
    if (!loader || loader->req_count == 0u || offset >= loader->req_count) {
        return false;
    }

    uint32_t cap = loader->req_capacity;
    uint32_t head = loader->req_head;
    for (uint32_t i = offset; i + 1u < loader->req_count; ++i) {
        uint32_t from = (head + i + 1u) % cap;
        uint32_t to = (head + i) % cap;
        loader->requests[to] = loader->requests[from];
    }

    loader->req_tail = (loader->req_tail + cap - 1u) % cap;
    loader->req_count -= 1u;
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

static bool tile_loader_result_evict_at(TileLoader *loader, uint32_t offset) {
    if (!loader || loader->res_count == 0u || offset >= loader->res_count) {
        return false;
    }

    uint32_t cap = loader->res_capacity;
    uint32_t head = loader->res_head;
    uint32_t idx = (head + offset) % cap;
    TileResult doomed = loader->results[idx];

    for (uint32_t i = offset; i + 1u < loader->res_count; ++i) {
        uint32_t from = (head + i + 1u) % cap;
        uint32_t to = (head + i) % cap;
        loader->results[to] = loader->results[from];
    }

    loader->res_tail = (loader->res_tail + cap - 1u) % cap;
    loader->res_count -= 1u;
    memset(&loader->results[loader->res_tail], 0, sizeof(loader->results[loader->res_tail]));
    if (doomed.ok) {
        mft_free_tile(&doomed.tile);
    }
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

        TileResult result = {0};
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

        pthread_mutex_lock(&loader->mutex);
        if (!existed) {
            loader->missing_count += 1;
        } else if (result.ok) {
            loader->load_ok_count += 1;
        } else {
            loader->load_fail_count += 1;
        }
        bool pushed = tile_loader_result_push(loader, &result);
        if (!pushed && loader->res_count >= loader->res_capacity) {
            bool evicted = false;
            for (uint32_t i = 0u; i < loader->res_count; ++i) {
                uint32_t idx = (loader->res_head + i) % loader->res_capacity;
                if (loader->results[idx].request_id < loader->latest_request_id) {
                    evicted = tile_loader_result_evict_at(loader, i);
                    break;
                }
            }
            if (!evicted) {
                evicted = tile_loader_result_evict_at(loader, 0u);
            }
            if (evicted) {
                loader->result_evict_count += 1u;
                pushed = tile_loader_result_push(loader, &result);
            }
        }
        if (pushed) {
            loader->produced_count += 1;
        } else {
            loader->result_drop_count += 1;
        }
        pthread_mutex_unlock(&loader->mutex);
        if (!pushed) {
            if (result.ok) {
                mft_free_tile(&result.tile);
            }
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

bool tile_loader_enqueue(TileLoader *loader, TileCoord coord, TileLayerKind kind, TileZoomBand band, uint32_t request_id) {
    if (!loader || !loader->running) {
        return false;
    }

    TileRequest request = {coord, kind, band, request_id};
    bool ok = false;
    pthread_mutex_lock(&loader->mutex);
    if (request_id > loader->latest_request_id) {
        loader->latest_request_id = request_id;
    }
    ok = tile_loader_request_push(loader, &request);
    if (!ok && loader->req_count >= loader->req_capacity) {
        bool evicted = false;
        for (uint32_t i = 0u; i < loader->req_count; ++i) {
            uint32_t idx = (loader->req_head + i) % loader->req_capacity;
            if (loader->requests[idx].request_id != request_id) {
                evicted = tile_loader_request_evict_at(loader, i);
                break;
            }
        }
        if (!evicted) {
            evicted = tile_loader_request_evict_at(loader, 0u);
        }
        if (evicted) {
            loader->enqueue_evict_count += 1u;
            ok = tile_loader_request_push(loader, &request);
        }
    }
    if (ok) {
        loader->enqueued_count += 1;
        pthread_cond_signal(&loader->cond);
    } else {
        loader->enqueue_drop_count += 1;
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

void tile_loader_get_stats(TileLoader *loader, TileLoaderStats *out_stats) {
    if (!loader || !out_stats) {
        return;
    }

    pthread_mutex_lock(&loader->mutex);
    out_stats->req_count = loader->req_count;
    out_stats->res_count = loader->res_count;
    out_stats->req_capacity = loader->req_capacity;
    out_stats->res_capacity = loader->res_capacity;
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
