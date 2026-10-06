#include "app/region_prepare.h"
#include "map/mercator.h"
#include "map/polygon_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Uses the existing map-specific camera policy on the private staged camera.
extern void app_center_camera_on_region(Camera *, const RegionInfo *, int, int);
// Uses the existing source-level selection on the staged region.
extern uint16_t app_zoom_to_tile_level(float, const RegionInfo *);

// Reads cancellation under the same lock as progress publication.
static bool prepare_canceled(RegionPrepare *p) {
    pthread_mutex_lock(&p->mutex);
    bool cancel = p->cancel;
    pthread_mutex_unlock(&p->mutex);
    return cancel;
}

// Adapts the synchronized job cancellation to exact polygon preparation.
static bool prepare_cancel_check(void *context) { return prepare_canceled(context); }

// Publishes progress and terminal ownership under one lock.
static void prepare_report(RegionPrepare *p, bool done, bool ok, const char *message) {
    pthread_mutex_lock(&p->mutex);
    p->progress.done = done;
    p->progress.ok = ok && !p->cancel;
    p->progress.canceled = p->cancel;
    snprintf(p->progress.message, sizeof(p->progress.message), "%s", message);
    pthread_mutex_unlock(&p->mutex);
}

// Validates and decodes at most 64 source coordinates without touching Vulkan or AppState.
static void *prepare_worker(void *ctx) {
    RegionPrepare *p = ctx;
    if (prepare_canceled(p)) goto canceled;
    if (!region_validate_package(&p->region, &p->validation)) {
        prepare_report(p, true, false, p->validation.summary);
        return NULL;
    }
    if (prepare_canceled(p)) goto canceled;
    if (!region_load_meta(&p->region, &p->region)) {
        prepare_report(p, true, false, "Region metadata unavailable");
        return NULL;
    }
    app_center_camera_on_region(&p->camera, &p->region, p->width, p->height);
    float min_x, min_y, max_x, max_y;
    if (!camera_visible_world_aabb(&p->camera, p->width, p->height, &min_x, &min_y, &max_x, &max_y)) goto unavailable;
    uint16_t z = app_zoom_to_tile_level(p->camera.zoom, &p->region);
    TileCoord first = tile_from_meters(z, (MercatorMeters){min_x, max_y});
    TileCoord last = tile_from_meters(z, (MercatorMeters){max_x, min_y});
    if (p->region.has_bounds) {
        MercatorMeters lo = mercator_from_latlon((LatLon){p->region.min_lat, p->region.min_lon});
        MercatorMeters hi = mercator_from_latlon((LatLon){p->region.max_lat, p->region.max_lon});
        TileCoord a = tile_from_meters(z, (MercatorMeters){lo.x, hi.y});
        TileCoord b = tile_from_meters(z, (MercatorMeters){hi.x, lo.y});
        if (first.x < a.x) first.x = a.x;
        if (first.y < a.y) first.y = a.y;
        if (last.x > b.x) last.x = b.x;
        if (last.y > b.y) last.y = b.y;
    }
    if (first.x > last.x || first.y > last.y ||
        (uint64_t)(last.x - first.x + 1u) * (last.y - first.y + 1u) > 64u) goto unavailable;
    pthread_mutex_lock(&p->mutex);
    p->progress.total = (last.x - first.x + 1u) * (last.y - first.y + 1u) * (TILE_LAYER_COUNT - 1u);
    snprintf(p->progress.message, sizeof(p->progress.message), "Preparing map tiles");
    pthread_mutex_unlock(&p->mutex);
    for (unsigned kind = 0; kind < TILE_LAYER_COUNT; ++kind) {
        if (prepare_canceled(p)) goto canceled;
        if (p->cpu_limits[kind] == 0) goto unavailable;
        if (!tile_manager_init_with_source_for_layer(&p->managers[kind], 256, &p->region.tile_source, (TileLayerKind)kind)) goto unavailable;
        p->managers[kind].byte_limit = p->cpu_limits[kind];
        if (kind == TILE_LAYER_CONTOUR) continue;
        for (uint32_t y = first.y; y <= last.y; ++y) for (uint32_t x = first.x; x <= last.x; ++x) {
            if (prepare_canceled(p)) goto canceled;
            TileCoord coord = {z, x, y};
            TileZoomBand band = region_tile_coverage_resolve_band(&p->region, (TileLayerKind)kind, TILE_BAND_COARSE, z);
            char path[512];
            bool exists = tile_source_resolve_path(&p->region.tile_source, coord, (TileLayerKind)kind, band, path, sizeof(path));
            if (!exists && band != TILE_BAND_DEFAULT) {
                band = TILE_BAND_DEFAULT;
                exists = tile_source_resolve_path(&p->region.tile_source, coord, (TileLayerKind)kind, band, path, sizeof(path));
            }
            if (exists) {
                const MftTile *tile = tile_manager_get_tile(&p->managers[kind], coord, band);
                if (!tile) goto unavailable;
                if (kind >= TILE_LAYER_POLY_WATER && tile->polygon_ring_total &&
                    !polygon_cache_build_cancelable((MftTile *)tile, kind == TILE_LAYER_POLY_WATER ? 32768u : 8192u,
                                                     NULL, prepare_cancel_check, p)) {
                    if (prepare_canceled(p)) goto canceled;
                    goto unavailable;
                }
                if (tile_manager_payload_bytes(&p->managers[kind]) > p->cpu_limits[kind]) goto unavailable;
                for (uint32_t i = 0; i < p->managers[kind].capacity; ++i)
                    if (p->managers[kind].entries[i].occupied) p->managers[kind].entries[i].presentation_pinned = true;
            }
            uint64_t staged_bytes = 0;
            for (unsigned layer = 0; layer < TILE_LAYER_COUNT; ++layer)
                staged_bytes += tile_manager_payload_bytes(&p->managers[layer]);
            pthread_mutex_lock(&p->mutex);
            p->progress.resident_bytes = staged_bytes;
            p->progress.examined++;
            if (exists) p->progress.ready++;
            pthread_mutex_unlock(&p->mutex);
        }
    }
    if (prepare_canceled(p)) goto canceled;
    if (p->progress.ready == 0) goto unavailable;
    prepare_report(p, true, true, "Map tiles ready");
    return NULL;
unavailable:
    prepare_report(p, true, false, "Region unavailable or preparation budget exceeded");
    return NULL;
canceled:
    prepare_report(p, true, false, "Map preparation canceled");
    return NULL;
}

RegionPrepare *region_prepare_start(const RegionInfo *info, const Camera *camera, int width, int height,
                                    const uint64_t cpu_limits[TILE_LAYER_COUNT]) {
    if (!info || !info->name || !camera || !cpu_limits || width <= 0 || height <= 0) return NULL;
    RegionPrepare *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->region = *info;
    snprintf(p->name, sizeof(p->name), "%s", info->name);
    p->region.name = p->name;
    p->camera = *camera;
    p->width = width;
    p->height = height;
    memcpy(p->cpu_limits, cpu_limits, sizeof(p->cpu_limits));
    snprintf(p->progress.message, sizeof(p->progress.message), "Validating map package");
    if (pthread_mutex_init(&p->mutex, NULL) != 0) { free(p); return NULL; }
    if (!core_workers_init(&p->workers, &p->thread, 1, &p->task, 1, NULL)) {
        pthread_mutex_destroy(&p->mutex); free(p); return NULL;
    }
    if (!core_workers_submit(&p->workers, prepare_worker, p)) { region_prepare_destroy(p); return NULL; }
    return p;
}

void region_prepare_cancel(RegionPrepare *p) {
    if (!p) return;
    pthread_mutex_lock(&p->mutex);
    p->cancel = true;
    pthread_mutex_unlock(&p->mutex);
}

RegionPrepareProgress region_prepare_progress(RegionPrepare *p) {
    RegionPrepareProgress progress = {0};
    if (!p) return progress;
    pthread_mutex_lock(&p->mutex);
    progress = p->progress;
    progress.canceled = p->cancel;
    pthread_mutex_unlock(&p->mutex);
    return progress;
}

void region_prepare_destroy(RegionPrepare *p) {
    if (!p) return;
    region_prepare_cancel(p);
    core_workers_shutdown(&p->workers);
    for (unsigned k = 0; k < TILE_LAYER_COUNT; ++k) tile_manager_shutdown(&p->managers[k]);
    pthread_mutex_destroy(&p->mutex);
    free(p);
}
